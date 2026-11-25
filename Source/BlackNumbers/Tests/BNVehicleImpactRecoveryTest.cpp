#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/BNPlayTestUtils.h"
#include "World/BNWorldActivities.h"

namespace
{
	const FVector Start(25000.f, -7400.f, 70.f);
	constexpr float Heading = 90.f;

	struct FImpactRun
	{
		enum class EPhase { SettleForCar, CarImpact, SettleForBump, Bump, SettleForRamp, Ramp, SettleAfterRamp, Recovery };
		EPhase Phase = EPhase::SettleForCar;
		double StartTime = 0.0;
		double StartZ = 0.0;
		double PeakRise = 0.0;
		double LowestUp = 1.0;
		double LowestForwardSpeed = 0.0;
		FVector SafePosition = FVector::ZeroVector;
		TWeakObjectPtr<ABNVehiclePawn> OtherCar;

		void Enter(EPhase Next, const ABNVehiclePawn& Car)
		{
			Phase = Next;
			StartTime = Car.GetWorld()->GetTimeSeconds();
			StartZ = Car.GetActorLocation().Z;
			PeakRise = 0.0;
			LowestUp = 1.0;
			LowestForwardSpeed = Car.GetVelocity().Y;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleImpactRecoveryTest, "BlackNumbers.Vehicle.ImpactAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleImpactRecoveryTest::RunTest(const FString& Parameters)
{
	using EPhase = FImpactRun::EPhase;
	const TSharedRef<FImpactRun> Run = MakeShared<FImpactRun>();
	BNPlayTest::StartTestTrack(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Run]()
	{
		ABNVehiclePawn* Car = BNPlayTest::FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared after an impact or crossing Kill Z"));
			return true;
		}
		UWorld& World = *Car->GetWorld();
		const double Elapsed = World.GetTimeSeconds() - Run->StartTime;
		const FVector Position = Car->GetActorLocation();
		Run->PeakRise = FMath::Max(Run->PeakRise, Position.Z - Run->StartZ);
		Run->LowestUp = FMath::Min(Run->LowestUp, Car->GetActorUpVector().Z);
		Run->LowestForwardSpeed = FMath::Min(Run->LowestForwardSpeed, Car->GetVelocity().Y);

		auto ResetAtStart = [&](EPhase Phase)
		{
			BNPlayTest::PlaceStopped(*Car, Start, Heading);
			Run->Enter(Phase, *Car);
		};
		if (Run->StartTime == 0.0)
		{
			ResetAtStart(EPhase::SettleForCar);
			ABNVehiclePawn* Target = World.SpawnActor<ABNVehiclePawn>(Start + FVector(75.f, 1550.f, 0.f),
				FRotator(0.f, Heading - 180.f, 0.f));
			Run->OtherCar = Target;
			if (!TestNotNull(TEXT("Opponent placed for offset head-on impact"), Target))
			{
				return true;
			}
			Target->SpawnDefaultController();
			return false;
		}

		switch (Run->Phase)
		{
		case EPhase::SettleForCar:
			if (Elapsed >= 0.8 && Run->OtherCar.IsValid())
			{
				Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0.f, 3400.f, 0.f));
				Run->OtherCar->GetMesh()->SetPhysicsLinearVelocity(FVector(0.f, -2100.f, 0.f));
				Run->Enter(EPhase::CarImpact, *Car);
			}
			break;
		case EPhase::CarImpact:
			if (Elapsed >= 1.2)
			{
				UE_LOG(LogTemp, Display, TEXT("BN impact test: head-on rise %.1f cm, lowest up %.2f, slowest %.1f cm/s"),
					Run->PeakRise, Run->LowestUp, Run->LowestForwardSpeed);
				TestTrue(TEXT("Head-on impact stays near the road and upright"), Run->PeakRise < 75.f && Run->LowestUp > 0.55f);
				TestTrue(TEXT("Impact materially reduced the player's forward speed"), Run->LowestForwardSpeed < 1600.f);
				Run->OtherCar->Destroy();
				AStaticMeshActor* Bump = World.SpawnActor<AStaticMeshActor>(FVector(Start.X, Start.Y + 1300.f, 0.f),
					FRotator(0.f, Heading, 0.f));
				UStaticMesh* RampMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_JumpRamp.SM_BN_JumpRamp"));
				if (!TestNotNull(TEXT("Bump actor spawned"), Bump)
					|| !TestNotNull(TEXT("Curved ramp mesh for low road bump"), RampMesh))
				{
					return true;
				}
				Bump->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Bump->GetStaticMeshComponent()->SetStaticMesh(RampMesh);
				Bump->SetActorScale3D(FVector(0.3f, 1.f, 0.085f));
				ResetAtStart(EPhase::SettleForBump);
			}
			break;
		case EPhase::SettleForBump:
			if (Elapsed >= 0.8)
			{
				Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0.f, 3150.f, 0.f));
				Run->Enter(EPhase::Bump, *Car);
			}
			break;
		case EPhase::Bump:
			if (Elapsed >= 1.3)
			{
				UE_LOG(LogTemp, Display, TEXT("BN impact test: 20 cm bump rise %.1f cm, lowest up %.2f"), Run->PeakRise, Run->LowestUp);
				TestTrue(TEXT("A 20 cm bump moves the suspension without launching the car"),
					Run->PeakRise > 5.f && Run->PeakRise < 70.f && Run->LowestUp > 0.7f);
				TestTrue(TEXT("Car actually crossed the bump"), Position.Y > Start.Y + 1500.f);
				const FVector RampStart = Start + FVector(0.f, 4000.f, 0.f);
				World.SpawnActor<ABNStuntJump>(FVector(RampStart.X, RampStart.Y + 1650.f, 0.f), FRotator(0.f, Heading, 0.f));
				BNPlayTest::PlaceStopped(*Car, RampStart, Heading);
				Run->Enter(EPhase::SettleForRamp, *Car);
			}
			break;
		case EPhase::SettleForRamp:
			if (Elapsed >= 0.85)
			{
				Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0.f, 3200.f, 0.f));
				Run->Enter(EPhase::Ramp, *Car);
			}
			break;
		case EPhase::Ramp:
			if ((Elapsed >= 1.6 && Car->IsOnGround()) || Elapsed >= 3.8)
			{
				UE_LOG(LogTemp, Display, TEXT("BN impact test: ramp rise %.1f cm, lowest up %.2f"), Run->PeakRise, Run->LowestUp);
				TestTrue(TEXT("Designed ramp still launches past its 235 cm lip"), Run->PeakRise > 320.f && Run->PeakRise < 1050.f);
				TestTrue(TEXT("Car comes down on its tires"), Run->LowestUp > 0.55f && Car->IsOnGround());
				Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Car->GetChaosMovement()->SetHandbrakeInput(true);
				Run->Enter(EPhase::SettleAfterRamp, *Car);
			}
			break;
		case EPhase::SettleAfterRamp:
			if (Elapsed >= 0.9)
			{
				Run->SafePosition = Position;
				World.GetWorldSettings()->KillZ = -500.f;
				BNPlayTest::PlaceStopped(*Car, FVector(50000.f, 50000.f, -430.f), Heading);
				Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -500.f));
				Run->Enter(EPhase::Recovery, *Car);
			}
			break;
		case EPhase::Recovery:
			if (Elapsed >= 0.9)
			{
				UE_LOG(LogTemp, Display, TEXT("BN impact test: recovered %.1f cm from solid ground sample"),
					FVector::Dist2D(Position, Run->SafePosition));
				TestTrue(TEXT("Void recovery returns to nearby solid ground"), FVector::Dist2D(Position, Run->SafePosition) < 600.f);
				TestEqual(TEXT("Recovery happens once"), Car->GetRecoveryCount(), 1u);
				TestTrue(TEXT("Same player car is standing back on its wheels"), Position.Z > 0.f && Car->IsOnGround());
				return true;
			}
			break;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
