// bn.Benchmark: repeatable in-game performance capture over fixed scenarios. Not built into shipping.
// Run through Tools/benchmark.sh; writes Saved/Benchmark/benchmark.md and one screenshot per scenario.

#if !UE_BUILD_SHIPPING

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "RHIGlobals.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNDayNightCycle.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	struct FScenario
	{
		const TCHAR* Name;
		FVector Location; // see Tools/build_test_map.py layout
		float Yaw;
		float Hour;
		bool bDrive;
		/** Car-local position of a fixed inspection camera looking at the car; zero = normal chase camera. */
		FVector InspectFrom = FVector::ZeroVector;
		/** Starts the circuit race (keep last: the race keeps running into later scenarios). */
		bool bStartCircuitRace = false;
		/** Runs the clock with this day length (time-lapse); 0 holds the hour. */
		float DayLengthMinutes = 0.f;
	};

	const FScenario Scenarios[] = {
		// First, while caches are cold, as for a player's first sunset. 1-minute day: the 10 s measurement runs
		// ~18:00 -> 22:00, through sunset and the street lamps coming on.
		{ TEXT("sunset_timelapse"), FVector(-9000.f, -12500.f, 60.f), 0.f, 16.8f, false, FVector::ZeroVector, false, 1.f },
		{ TEXT("street_day"), FVector(-9000.f, -12500.f, 60.f), 0.f, 13.f, false },
		{ TEXT("street_night"), FVector(-9000.f, -12500.f, 60.f), 0.f, 22.f, false },
		{ TEXT("hairpin_dusk"), FVector(7000.f, -12500.f, 60.f), 0.f, 18.5f, false },
		{ TEXT("runway_drive_night"), FVector(-18000.f, 0.f, 60.f), 0.f, 22.f, true },
		// Visual checks: wheel rims face outward on both sides, headlights on at night.
		{ TEXT("inspect_left_side"), FVector(-9000.f, -12500.f, 60.f), 0.f, 13.f, false, FVector(0.f, -650.f, 120.f) },
		{ TEXT("inspect_right_side"), FVector(-9000.f, -12500.f, 60.f), 0.f, 13.f, false, FVector(0.f, 650.f, 120.f) },
		{ TEXT("inspect_front_night"), FVector(-9000.f, -12500.f, 60.f), 0.f, 22.f, false, FVector(900.f, 150.f, 150.f) },
		{ TEXT("circuit_race_dusk"), FVector(-9000.f, -12500.f, 60.f), 0.f, 18.5f, false, FVector::ZeroVector, true },
	};

	constexpr double WarmupSeconds = 3.0;
	constexpr double MeasureSeconds = 10.0;
	constexpr double ScreenshotDelaySeconds = 1.0;
	constexpr double FrameBudgetMs = 1000.0 / 60.0;
	/** Frames slower than this are logged with the time of day, to find hitches. */
	constexpr float HitchMs = 25.f;

	struct FFrameSample { float FrameMs, GameMs, RenderMs, GpuMs; };

	struct FRun
	{
		enum class EStep { WaitShaders, Place, Warmup, Measure, Screenshot, Done } Step = EStep::WaitShaders;
		int32 Index = 0;
		double StepStart = 0.0;
		TArray<FFrameSample> Frames;
		FString Report;
	};

	float Percentile(TArray<float> Values, float P)
	{
		if (Values.IsEmpty())
		{
			return 0.f;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::FloorToInt(P * (Values.Num() - 1)), 0, Values.Num() - 1)];
	}

	FString Summarise(const FScenario& Scenario, const TArray<FFrameSample>& Frames)
	{
		TArray<float> FrameMs;
		double Game = 0.0, Render = 0.0, Gpu = 0.0;
		int32 OverBudget = 0;
		for (const FFrameSample& F : Frames)
		{
			FrameMs.Add(F.FrameMs);
			Game += F.GameMs;
			Render += F.RenderMs;
			Gpu += F.GpuMs;
			OverBudget += F.FrameMs > FrameBudgetMs;
		}
		const int32 N = FMath::Max(Frames.Num(), 1);
		double TotalMs = 0.0;
		for (float Ms : FrameMs)
		{
			TotalMs += Ms;
		}
		return FString::Printf(TEXT("| %s | %d | %.1f | %.2f | %.2f | %.2f | %.2f | %.1f%% | %.2f | %.2f | %.2f | %.1f |\n"),
			Scenario.Name, Frames.Num(), 1000.0 * N / FMath::Max(TotalMs, 1.0), TotalMs / N,
			Percentile(FrameMs, 0.5f), Percentile(FrameMs, 0.95f), Percentile(FrameMs, 0.99f),
			100.0 * OverBudget / N, Game / N, Render / N, Gpu / N, Percentile(FrameMs, 1.f));
	}

	ABNVehiclePawn* PlayerCar(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	}

	void Place(ABNVehiclePawn& Car, const FScenario& Scenario)
	{
		APlayerController* PC = Car.GetWorld()->GetFirstPlayerController();
		if (Scenario.InspectFrom.IsZero())
		{
			PC->SetViewTarget(&Car);
		}
		else
		{
			const FTransform CarPose(FRotator(0.f, Scenario.Yaw, 0.f), Scenario.Location);
			const FVector Eye = CarPose.TransformPosition(Scenario.InspectFrom);
			const FRotator Look = (Scenario.Location + FVector(0.f, 0.f, 40.f) - Eye).Rotation();
			PC->SetViewTarget(Car.GetWorld()->SpawnActor<ACameraActor>(Eye, Look));
		}

		Car.SetActorLocationAndRotation(Scenario.Location, FRotator(0.f, Scenario.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Car.GetChaosMovement()->SetThrottleInput(Scenario.bDrive ? 1.f : 0.f);
		for (TActorIterator<ABNDayNightCycle> It(Car.GetWorld()); It; ++It)
		{
			It->bPaused = Scenario.DayLengthMinutes <= 0.f;
			if (!It->bPaused)
			{
				It->DayLengthMinutes = Scenario.DayLengthMinutes;
			}
			It->SetTimeOfDay(Scenario.Hour);
		}
		for (TActorIterator<ABNRaceEvent> It(Car.GetWorld()); It && Scenario.bStartCircuitRace; ++It)
		{
			if (It->Definition && It->Definition->Discipline == EBNRaceDiscipline::Circuit)
			{
				It->StartRace(&Car);
				break;
			}
		}
	}

	int32 RemainingShaderJobs()
	{
#if WITH_EDITOR
		return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#else
		return 0;
#endif
	}

	/** One benchmark step per frame; returns false when finished (removes the ticker). */
	bool Step(TWeakObjectPtr<UWorld> WeakWorld, FRun& Run, float DeltaSeconds)
	{
		UWorld* World = WeakWorld.Get();
		ABNVehiclePawn* Car = PlayerCar(World);
		if (!Car)
		{
			UE_LOG(LogTemp, Error, TEXT("bn.Benchmark: no player car, aborting"));
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		const double InStep = Now - Run.StepStart;
		auto Next = [&](FRun::EStep S) { Run.Step = S; Run.StepStart = Now; };
		const FScenario& Scenario = Scenarios[Run.Index];

		switch (Run.Step)
		{
		case FRun::EStep::WaitShaders:
			if (RemainingShaderJobs() == 0)
			{
				Next(FRun::EStep::Place);
			}
			break;
		case FRun::EStep::Place:
			Place(*Car, Scenario);
			Next(FRun::EStep::Warmup);
			break;
		case FRun::EStep::Warmup:
			if (InStep >= WarmupSeconds && RemainingShaderJobs() == 0)
			{
				Run.Frames.Reset();
				Next(FRun::EStep::Measure);
			}
			break;
		case FRun::EStep::Measure:
			Run.Frames.Add({ DeltaSeconds * 1000.f, FPlatformTime::ToMilliseconds(GGameThreadTime),
				FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)) });
			if (DeltaSeconds * 1000.f > HitchMs)
			{
				TActorIterator<ABNDayNightCycle> Cycle(World);
				UE_LOG(LogTemp, Display, TEXT("bn.Benchmark %s: hitch %.1f ms (game %.1f, render %.1f, gpu %.1f) at %.2f h"), Scenario.Name,
					DeltaSeconds * 1000.f, Run.Frames.Last().GameMs, Run.Frames.Last().RenderMs, Run.Frames.Last().GpuMs,
					Cycle ? Cycle->GetTimeOfDay() : -1.f);
			}
			if (InStep >= MeasureSeconds)
			{
				const USkeletalMeshComponent* Body = Car->GetMesh();
				const APlayerCameraManager* CameraManager = World->GetFirstPlayerController()->PlayerCameraManager;
				UE_LOG(LogTemp, Display, TEXT("bn.Benchmark %s: car mesh %s visible=%d bounds=%s at %s, camera at %s"),
					Scenario.Name, *GetNameSafe(Body->GetSkeletalMeshAsset()), Body->IsVisible(), *Body->Bounds.BoxExtent.ToString(),
					*Car->GetActorLocation().ToString(), CameraManager ? *CameraManager->GetCameraLocation().ToString() : TEXT("?"));
				Run.Report += Summarise(Scenario, Run.Frames);
				// Through the player controller so the command reaches the game viewport.
				World->GetFirstPlayerController()->ConsoleCommand(TEXT("Shot"));
				Next(FRun::EStep::Screenshot);
			}
			break;
		case FRun::EStep::Screenshot:
			if (InStep >= ScreenshotDelaySeconds)
			{
				const bool bMore = ++Run.Index < UE_ARRAY_COUNT(Scenarios);
				Next(bMore ? FRun::EStep::Place : FRun::EStep::Done);
			}
			break;
		case FRun::EStep::Done:
		{
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Benchmark/benchmark.md");
			const FIntPoint Size = World->GetGameViewport() && World->GetGameViewport()->Viewport
				? World->GetGameViewport()->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
			const FString Header = FString::Printf(
				TEXT("# bn.Benchmark %s\n\nGPU: %s - RHI: %s - Viewport: %dx%d - Measure: %.0fs per scenario after %.0fs warm-up\n\n"),
				*FDateTime::Now().ToString(), *GRHIAdapterName, GDynamicRHI ? GDynamicRHI->GetName() : TEXT("?"),
				Size.X, Size.Y, MeasureSeconds, WarmupSeconds)
				+ TEXT("| Scenario | Frames | Avg FPS | Avg ms | p50 ms | p95 ms | p99 ms | >16.7 ms | Game ms | Render ms | GPU ms | Max ms |\n")
				+ TEXT("|---|---|---|---|---|---|---|---|---|---|---|---|\n");
			FFileHelper::SaveStringToFile(Header + Run.Report, *Path);
			UE_LOG(LogTemp, Display, TEXT("bn.Benchmark: wrote %s\n%s"), *Path, *(Header + Run.Report));
			FPlatformMisc::RequestExit(false);
			return false;
		}
		}
		return true;
	}

	FAutoConsoleCommandWithWorld GBNBenchmarkCommand(
		TEXT("bn.Benchmark"),
		TEXT("Measures frame timings over fixed test-track scenarios, writes Saved/Benchmark/benchmark.md and exits."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			TSharedRef<FRun> Run = MakeShared<FRun>();
			Run->StepStart = FPlatformTime::Seconds();
			TWeakObjectPtr<UWorld> WeakWorld(World);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Run](float DeltaSeconds)
			{
				return Step(WeakWorld, *Run, DeltaSeconds);
			}));
		}));
}

#endif
