#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Vehicle/BNVehicleEffects.h"
#include "World/BNTrafficSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Pure headless unit test: verifies null-owner/null-world safety, VFX state machine counters,
// and Kuzey Viyadugu elevation profile math without requiring a rendered viewport.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNTrafficAndVfxUnitTest, "BlackNumbers.VFX.EffectsAndTrafficMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNTrafficAndVfxUnitTest::RunTest(const FString& Parameters)
{
	UBNVehicleEffectsComponent* Fx = NewObject<UBNVehicleEffectsComponent>(GetTransientPackage());
	TestNotNull(TEXT("standalone effects component created"), Fx);
	if (!Fx)
	{
		return false;
	}

	// Idle cruising produces no flames or smoke.
	Fx->UpdateEffects(0.05f, 80.f, 2.f, false, true, false, false);
	TestFalse(TEXT("flame hidden while cruising without NOS/backfire"), Fx->IsFlameVisible());
	TestEqual(TEXT("no smoke emitted during straight cruise"), Fx->GetActiveSmokeCount(), 0);
	TestEqual(TEXT("no skidmarks emitted during straight cruise"), Fx->GetActiveSkidmarkCount(), 0);

	// Drifting + NOS activates flame and smoke puffs safely even with null Owner/World.
	for (int32 Step = 0; Step < 8; ++Step)
	{
		Fx->UpdateEffects(0.06f, 110.f, 24.f, true, true, true, false);
	}
	TestTrue(TEXT("flame visible during NOS"), Fx->IsFlameVisible());
	TestTrue(TEXT("drift emits active smoke puffs"), Fx->GetActiveSmokeCount() > 0);

	// Backfire alone also shows flame; collision sparks activate pool entries.
	Fx->UpdateEffects(0.05f, 95.f, 0.f, false, true, false, true);
	TestTrue(TEXT("flame visible during backfire"), Fx->IsFlameVisible());

	Fx->TriggerCollisionSparks(FVector::ZeroVector, FVector::UpVector, 120000.f);
	TestTrue(TEXT("collision sparks burst activates pool"), Fx->GetActiveSparkCount() >= 8);

	// Advance time with effects off so all pooled particles expire cleanly.
	for (int32 Step = 0; Step < 25; ++Step)
	{
		Fx->UpdateEffects(0.1f, 0.f, 0.f, false, true, false, false);
	}
	TestFalse(TEXT("flame hides after NOS/backfire end"), Fx->IsFlameVisible());
	TestEqual(TEXT("smoke puffs expire cleanly"), Fx->GetActiveSmokeCount(), 0);
	TestEqual(TEXT("sparks expire cleanly"), Fx->GetActiveSparkCount(), 0);

	// Kuzey Viyadugu elevation profile checks.
	TestEqual(TEXT("viaduct west apron is ground level"), UBNTrafficSubsystem::SampleViaductElevationZ(-24000.f), 2.f);
	TestEqual(TEXT("viaduct deck reaches Z=700 cm"), UBNTrafficSubsystem::SampleViaductElevationZ(0.f), 700.f);
	TestEqual(TEXT("viaduct east apron returns to ground"), UBNTrafficSubsystem::SampleViaductElevationZ(24000.f), 2.f);
	const float MidRampZ = UBNTrafficSubsystem::SampleViaductElevationZ(-17000.f);
	TestTrue(TEXT("viaduct west ramp mid-point rises smoothly"), MidRampZ > 300.f && MidRampZ < 400.f);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

using namespace BNPlayTest;

// Ensures civilian traffic remains strictly dormant (0 cars) on /Game/Maps/TestTrack.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNTrafficDormantOnTestTrackTest, "BlackNumbers.World.TrafficDormantOnTestTrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNTrafficDormantOnTestTrackTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		UWorld* World = PlayWorld();
		UBNTrafficSubsystem* Traffic = World ? World->GetSubsystem<UBNTrafficSubsystem>() : nullptr;
		TestNotNull(TEXT("traffic subsystem exists on TestTrack"), Traffic);
		if (Traffic)
		{
			TestEqual(TEXT("TestTrack spawns 0 traffic cars"), Traffic->GetActiveTrafficCount(), 0);
			TArray<FVector> Locations;
			Traffic->GetTrafficLocations(Locations);
			TestEqual(TEXT("TestTrack reports 0 traffic locations"), Locations.Num(), 0);

			Traffic->SetTrafficEnabled(true);
			TestEqual(TEXT("re-enabling traffic keeps TestTrack dormant"), Traffic->GetActiveTrafficCount(), 0);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
	// Spacious Dogu Sanayi Drift Courtyard in 2 km KaramaraSlice (X: +330..+550 m, Y: +450..+640 m).
	const FVector CourtyardStart(36500.f, 50000.f, 65.f);
	constexpr float CourtyardYaw = 90.f;
	constexpr float TrafficSampleSeconds = 1.2f;
	constexpr float PhaseTimeoutSeconds = 15.f;

	struct FKaramaraTrafficVfxState
	{
		enum class EPhase
		{
			CheckSpawn,
			SampleMovement,
			PlaceCar,
			Settle,
			AccelerateNos,
			DriftKick,
			DriftHold,
			Finish
		} Phase = EPhase::CheckSpawn;

		double PhaseStart = 0.0;
		TArray<FVector> InitialTrafficLocations;
		bool bSawFlame = false;
		int32 MaxSmoke = 0;
		int32 MaxSkidmarks = 0;
		int32 MaxSparks = 0;
	};
}

// Loads /Game/Maps/KaramaraSlice in PIE: verifies >= 10 civilian traffic cars spawn and move smoothly
// along lanes, toggle cleanly, and driving/drifting + NOS activates UBNVehicleEffectsComponent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNKaramaraTrafficAndVfxTest, "BlackNumbers.World.KaramaraTrafficAndVfx",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNKaramaraTrafficAndVfxTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar())
		{
			return true;
		}
		if (FPlatformTime::Seconds() - WaitStart > 30.0)
		{
			AddError(TEXT("Player car did not spawn on KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	const TSharedRef<FKaramaraTrafficVfxState> State = MakeShared<FKaramaraTrafficVfxState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FKaramaraTrafficVfxState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		UWorld* World = PlayWorld();
		UBNTrafficSubsystem* Traffic = World ? World->GetSubsystem<UBNTrafficSubsystem>() : nullptr;
		if (!Car || !World || !Traffic)
		{
			AddError(TEXT("Car, PlayWorld or UBNTrafficSubsystem missing on KaramaraSlice"));
			return true;
		}

		UBNVehicleEffectsComponent* Fx = Car->FindComponentByClass<UBNVehicleEffectsComponent>();
		if (!Fx)
		{
			AddError(TEXT("UBNVehicleEffectsComponent not attached to ABNVehiclePawn"));
			return true;
		}

		const double Now = World->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		if (State->Phase != EPhase::CheckSpawn && InPhase > PhaseTimeoutSeconds)
		{
			Car->SetNosWanted(false);
			Car->GetChaosMovement()->SetThrottleInput(0.f);
			Car->GetChaosMovement()->SetSteeringInput(0.f);
			Car->GetChaosMovement()->SetHandbrakeInput(false);
			Traffic->SetTrafficEnabled(true);
			AddError(TEXT("KaramaraTrafficAndVfx test phase timed out"));
			return true;
		}

		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();

		switch (State->Phase)
		{
		case EPhase::CheckSpawn:
		{
			TestTrue(TEXT("traffic enabled by default on Karamara"), Traffic->IsTrafficEnabled());
			TestTrue(TEXT("Karamara spawns >= 10 civilian traffic cars"), Traffic->GetActiveTrafficCount() >= 10);
			Traffic->GetTrafficLocations(State->InitialTrafficLocations);
			TestTrue(TEXT("GetTrafficLocations returns >= 10 positions"), State->InitialTrafficLocations.Num() >= 10);
			Enter(EPhase::SampleMovement);
			return false;
		}
		case EPhase::SampleMovement:
		{
			if (InPhase < TrafficSampleSeconds)
			{
				return false;
			}
			TArray<FVector> LaterLocations;
			Traffic->GetTrafficLocations(LaterLocations);
			const int32 PairCount = FMath::Min(State->InitialTrafficLocations.Num(), LaterLocations.Num());
			int32 MovedSmoothly = 0;
			for (int32 i = 0; i < PairCount; ++i)
			{
				const float DeltaCm = FVector::Dist(State->InitialTrafficLocations[i], LaterLocations[i]);
				if (DeltaCm >= 300.f && DeltaCm <= 8000.f)
				{
					++MovedSmoothly;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("BN traffic test: %d/%d cars moved smoothly over %.2fs"),
				MovedSmoothly, PairCount, InPhase);
			TestTrue(TEXT("civilian traffic moves smoothly along Karamara lanes"), MovedSmoothly >= 8);

			// Verify clean disable/re-enable toggle.
			Traffic->SetTrafficEnabled(false);
			TestEqual(TEXT("disabling traffic despawns all cars"), Traffic->GetActiveTrafficCount(), 0);
			Traffic->SetTrafficEnabled(true);
			TestTrue(TEXT("re-enabling traffic respawns >= 10 cars"), Traffic->GetActiveTrafficCount() >= 10);

			Enter(EPhase::PlaceCar);
			return false;
		}
		case EPhase::PlaceCar:
			PlaceStopped(*Car, CourtyardStart, CourtyardYaw);
			Car->SetNosWanted(false);
			Enter(EPhase::Settle);
			return false;
		case EPhase::Settle:
			if (InPhase >= 0.8f)
			{
				Move->SetThrottleInput(1.f);
				Car->SetNosWanted(true);
				Enter(EPhase::AccelerateNos);
			}
			return false;
		case EPhase::AccelerateNos:
			State->bSawFlame |= Fx->IsFlameVisible();
			if (Car->GetSpeedKmh() >= 65.f && InPhase >= 0.8f)
			{
				Car->SetNosWanted(false);
				Move->SetThrottleInput(0.55f);
				Move->SetSteeringInput(1.f);
				Move->SetHandbrakeInput(true);
				Enter(EPhase::DriftKick);
			}
			return false;
		case EPhase::DriftKick:
			State->MaxSmoke = FMath::Max(State->MaxSmoke, Fx->GetActiveSmokeCount());
			State->MaxSkidmarks = FMath::Max(State->MaxSkidmarks, Fx->GetActiveSkidmarkCount());
			if (InPhase >= 0.65f)
			{
				Move->SetHandbrakeInput(false);
				Move->SetThrottleInput(1.f);
				Move->SetSteeringInput(0.75f);
				Enter(EPhase::DriftHold);
			}
			return false;
		case EPhase::DriftHold:
			State->MaxSmoke = FMath::Max(State->MaxSmoke, Fx->GetActiveSmokeCount());
			State->MaxSkidmarks = FMath::Max(State->MaxSkidmarks, Fx->GetActiveSkidmarkCount());
			if (InPhase >= 1.2f)
			{
				Fx->TriggerCollisionSparks(Car->GetActorLocation(), FVector::UpVector, 100000.f);
				State->MaxSparks = FMath::Max(State->MaxSparks, Fx->GetActiveSparkCount());
				Enter(EPhase::Finish);
			}
			return false;
		case EPhase::Finish:
			UE_LOG(LogTemp, Display, TEXT("BN VFX test: flame=%d maxSmoke=%d maxSkids=%d maxSparks=%d"),
				State->bSawFlame ? 1 : 0, State->MaxSmoke, State->MaxSkidmarks, State->MaxSparks);
			TestTrue(TEXT("NOS activates 3D exhaust flames"), State->bSawFlame);
			TestTrue(TEXT("drifting spawns pooled tire smoke"), State->MaxSmoke > 0);
			TestTrue(TEXT("drifting lays down pooled asphalt skidmarks"), State->MaxSkidmarks > 0);
			TestTrue(TEXT("hard impact triggers pooled sparks"), State->MaxSparks > 0);

			// Clean reset.
			Car->SetNosWanted(false);
			Move->SetThrottleInput(0.f);
			Move->SetSteeringInput(0.f);
			Move->SetHandbrakeInput(false);
			Traffic->SetTrafficEnabled(true);
			return true;
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
