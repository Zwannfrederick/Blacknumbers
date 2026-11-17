#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Tests/BNPlayTestUtils.h"

using namespace BNPlayTest;

namespace
{
	struct FSurfaceRun
	{
		const TCHAR* Name;
		FVector Start; // see the layout in Tools/build_test_map.py
		const TCHAR* ExpectedPhysMaterial;
		float BrakeDistanceM = -1.f;
	};

	constexpr float SettleSeconds = 1.f;
	constexpr float DriveSeconds = 3.f;
	constexpr float StoppedKmh = 2.f;
	constexpr float BrakeTimeoutSeconds = 15.f;
	/** Braking is grip-limited, so each looser surface must need at least this much more distance. */
	constexpr float MinExtraDistanceRatio = 0.1f;

	struct FSurfaceTestState
	{
		TArray<FSurfaceRun> Runs = {
			{ TEXT("asphalt"), FVector(-18000.f, 0.f, 60.f), TEXT("PM_Asphalt") },
			{ TEXT("dirt"), FVector(-24000.f, 11000.f, 60.f), TEXT("PM_Dirt") },
			{ TEXT("grass"), FVector(-24000.f, 17000.f, 60.f), TEXT("PM_Grass") },
		};
		int32 Index = 0;
		enum class EPhase { Place, Settle, Drive, Brake } Phase = EPhase::Place;
		double PhaseStart = 0.0;
		FVector BrakeStart = FVector::ZeroVector;
	};

	FString WheelPhysMaterial(const ABNVehiclePawn& Car)
	{
		const UPhysicalMaterial* Material = Car.GetChaosMovement()->GetWheelState(0).PhysMaterial.Get();
		return Material ? Material->GetName() : TEXT("none");
	}

	void PlaceStopped(ABNVehiclePawn& Car, const FVector& Location)
	{
		Car.SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Car.GetChaosMovement()->SetThrottleInput(0.f);
	}
}

// Accelerates for a few seconds, then brakes to a stop on asphalt, dirt and grass. Physical material
// friction feeds Chaos tyre grip; braking is grip-limited, so looser surfaces must stop measurably later.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleSurfaceTest, "BlackNumbers.Vehicle.SurfaceGrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleSurfaceTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FSurfaceTestState> State = MakeShared<FSurfaceTestState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared"));
			return true;
		}
		FSurfaceRun& Run = State->Runs[State->Index];
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		const double InPhase = Now - State->PhaseStart;
		switch (State->Phase)
		{
		case FSurfaceTestState::EPhase::Place:
			PlaceStopped(*Car, Run.Start);
			State->Phase = FSurfaceTestState::EPhase::Settle;
			State->PhaseStart = Now;
			return false;
		case FSurfaceTestState::EPhase::Settle:
			if (InPhase < SettleSeconds)
			{
				return false;
			}
			TestEqual(FString::Printf(TEXT("wheels report the %s physical material"), Run.Name),
				WheelPhysMaterial(*Car), FString(Run.ExpectedPhysMaterial));
			Move->SetThrottleInput(1.f);
			State->Phase = FSurfaceTestState::EPhase::Drive;
			State->PhaseStart = Now;
			return false;
		case FSurfaceTestState::EPhase::Drive:
			if (InPhase < DriveSeconds)
			{
				return false;
			}
			Move->SetThrottleInput(0.f);
			Move->SetBrakeInput(1.f);
			State->BrakeStart = Car->GetActorLocation();
			State->Phase = FSurfaceTestState::EPhase::Brake;
			State->PhaseStart = Now;
			UE_LOG(LogTemp, Display, TEXT("BN surface test: %s braking from %.1f km/h"), Run.Name, Car->GetSpeedKmh());
			return false;
		case FSurfaceTestState::EPhase::Brake:
			if (FMath::Abs(Car->GetSpeedKmh()) > StoppedKmh && InPhase < BrakeTimeoutSeconds)
			{
				return false;
			}
			Move->SetBrakeInput(0.f);
			break;
		}

		Run.BrakeDistanceM = FVector::Dist2D(State->BrakeStart, Car->GetActorLocation()) / 100.f;
		UE_LOG(LogTemp, Display, TEXT("BN surface test: %s stopped in %.1f m"), Run.Name, Run.BrakeDistanceM);
		State->Phase = FSurfaceTestState::EPhase::Place;
		if (++State->Index < State->Runs.Num())
		{
			return false;
		}

		const TArray<FSurfaceRun>& Runs = State->Runs;
		for (int32 i = 1; i < Runs.Num(); ++i)
		{
			TestTrue(FString::Printf(TEXT("%s needs more braking distance than %s"), Runs[i].Name, Runs[i - 1].Name),
				Runs[i].BrakeDistanceM > Runs[i - 1].BrakeDistanceM * (1.f + MinExtraDistanceRatio));
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
