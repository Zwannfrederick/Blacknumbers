#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Tests/BNPlayTestUtils.h"

using namespace BNPlayTest;

namespace
{
	constexpr float DriveSeconds = 2.f;
	/** cos of the angle between a wheel's outward axle and the car's outward side; 0.995 is about 6 degrees. */
	constexpr float MinAxleAlignment = 0.995f;
}

// Drives straight and checks every visible wheel spins about the car's lateral axis with its rim facing out.
// Catches wheel meshes attached to the rig with the wrong rotation (they wobble like tossed coins while the
// bones spin) or mounted inside out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleWheelAlignmentTest, "BlackNumbers.Vehicle.WheelsAligned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleWheelAlignmentTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	struct FState { double Start = -1.0; float Worst = 1.f; bool bLoggedBones = false; };
	const TSharedRef<FState> State = MakeShared<FState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car || Car->WheelMeshes.Num() != 4)
		{
			AddError(TEXT("Player car or its four wheel meshes are missing"));
			return true;
		}
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (State->Start < 0.0)
		{
			State->Start = Now;
			Car->GetChaosMovement()->SetThrottleInput(1.f);
			return false;
		}
		const FVector CarRight = Car->GetActorRightVector();
		for (int32 i = 0; i < Car->WheelMeshes.Num(); ++i)
		{
			// The mesh is thin along its local Y (the axle) and its rim faces +Y: right wheels (FR, BR) must
			// point +Y to the car's right, left wheels (FL, BL) to the car's left.
			const float Outward = (i % 2 == 1) ? 1.f : -1.f;
			State->Worst = FMath::Min(State->Worst, Outward * FVector::DotProduct(Car->WheelMeshes[i]->GetRightVector(), CarRight));
		}
		if (!State->bLoggedBones)
		{
			State->bLoggedBones = true;
			for (const UStaticMeshComponent* Wheel : Car->WheelMeshes)
			{
				const FRotator Bone = Car->GetMesh()->GetSocketTransform(Wheel->GetAttachSocketName(), RTS_Component).Rotator();
				UE_LOG(LogTemp, Display, TEXT("BN wheel test: %s bone %s (component space), wheel relative %s"),
					*Wheel->GetAttachSocketName().ToString(), *Bone.ToString(), *Wheel->GetRelativeRotation().ToString());
			}
		}
		if (Now - State->Start < DriveSeconds)
		{
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("BN wheel test: worst axle alignment %.4f (1 = perfect)"), State->Worst);
		TestTrue(TEXT("every wheel spins about the car's lateral axis with its rim facing out"), State->Worst >= MinAxleAlignment);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
