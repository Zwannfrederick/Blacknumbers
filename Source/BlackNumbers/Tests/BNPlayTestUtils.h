#pragma once

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicle/BNVehiclePawn.h"

/** Shared setup for tests that play /Game/Maps/TestTrack in PIE. */
namespace BNPlayTest
{
	inline UWorld* PlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	inline ABNVehiclePawn* FindPlayerCar()
	{
		const UWorld* World = PlayWorld();
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	}

	/**
	 * Queues: load the test track, optionally edit the editor world (PIE copies it), start PIE,
	 * wait for the player car, let it settle on its suspension.
	 */
	inline void StartTestTrack(FAutomationTestBase& Test, TFunction<void(UWorld&)> BeforePlay = nullptr)
	{
		constexpr float SpawnTimeoutSeconds = 30.f;
		constexpr float SettleSeconds = 1.f;

		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/TestTrack")));
		if (BeforePlay)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([BeforePlay]()
			{
				BeforePlay(*GEditor->GetEditorWorldContext().World());
				return true;
			}));
		}
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		const double WaitStart = FPlatformTime::Seconds();
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, WaitStart]()
		{
			if (FindPlayerCar())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - WaitStart > SpawnTimeoutSeconds)
			{
				Test.AddError(TEXT("Player was not spawned into an ABNVehiclePawn"));
				return true;
			}
			return false;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(SettleSeconds));
	}

	/** Angle between where the car points and where it is actually going, on the ground plane. */
	inline float SlipAngleDegrees(const AActor& Car)
	{
		const FVector Velocity = Car.GetVelocity().GetSafeNormal2D();
		const FVector Forward = Car.GetActorForwardVector().GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Velocity, Forward), -1.f, 1.f)));
	}

	/** Puts the car at rest at Location facing Yaw, with all inputs released. */
	inline void PlaceStopped(ABNVehiclePawn& Car, const FVector& Location, float Yaw = 0.f)
	{
		Car.SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(0.f);
		Move->SetSteeringInput(0.f);
		Move->SetHandbrakeInput(false);
	}
}

#endif
