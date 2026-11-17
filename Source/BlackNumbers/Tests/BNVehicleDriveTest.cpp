#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float SpawnTimeoutSeconds = 30.f;
	constexpr float SettleSeconds = 1.f;
	/** Stops at 100 km/h or after this long; ~100 m of the 380 m start straight. */
	constexpr float DriveTimeoutSeconds = 10.f;
	/** Mid-2000s sports car: 0-100 km/h in roughly 4.5-8 s. Faster means wheelspin-y and arcade-light. */
	constexpr float Min0To100Seconds = 4.5f;
	constexpr float Max0To100Seconds = 8.f;
	/** On flat ground the body should not hop; a few cm/s of suspension motion is fine. */
	constexpr float MaxBodyVerticalSpeedCms = 60.f;
	/** Full steering lock from 100 km/h with a little throttle: a planted car scrubs, a slippery one spins. */
	constexpr float CornerSeconds = 2.f;
	constexpr float CornerThrottle = 0.3f;
	constexpr float MaxCornerSlipDegrees = 15.f;
	constexpr float MinSpeedForSlipKmh = 20.f;
	/** Full throttle and full lock from a standstill: without traction the rear steps out into a donut. */
	constexpr float StopSpeedKmh = 5.f;
	constexpr float StopTimeoutSeconds = 10.f;
	constexpr float PowerTurnSeconds = 3.f;
	constexpr float MaxPowerTurnSlipDegrees = 20.f;

	ABNVehiclePawn* FindPlayerCar()
	{
		const UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	}

	struct FDriveRun
	{
		double StartTime = -1.0;
		float TimeTo100 = -1.f;
		float MaxVerticalSpeed = 0.f;
		double CornerStart = -1.0;
		float MaxSlipDegrees = 0.f;
		double StopStart = -1.0;
		double PowerTurnStart = -1.0;
		float MaxPowerTurnSlipDegrees = 0.f;
	};

	/** Angle between where the car points and where it is actually going, on the ground plane. */
	float SlipAngleDegrees(const AActor& Car)
	{
		const FVector Velocity = Car.GetVelocity().GetSafeNormal2D();
		const FVector Forward = Car.GetActorForwardVector().GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Velocity, Forward), -1.f, 1.f)));
	}
}

// Plays the test track in PIE and floors the throttle on flat ground. Checks the car drives at all
// (mesh, wheel bones, torque curve, game mode) and that acceleration and body stability stay believable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleDriveTest, "BlackNumbers.Vehicle.DrivesForward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleDriveTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/TestTrack")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));

	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar())
		{
			return true;
		}
		if (FPlatformTime::Seconds() - WaitStart > SpawnTimeoutSeconds)
		{
			AddError(TEXT("Player was not spawned into an ABNVehiclePawn"));
			return true;
		}
		return false;
	}));

	// Let the car drop onto its suspension before measuring.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(SettleSeconds));

	const TSharedRef<FDriveRun> Run = MakeShared<FDriveRun>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Run]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared while driving"));
			return true;
		}
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (Run->StartTime < 0.0)
		{
			Run->StartTime = Now;
			Car->GetChaosMovement()->SetThrottleInput(1.f);
		}
		const float Elapsed = static_cast<float>(Now - Run->StartTime);
		const float Speed = Car->GetSpeedKmh();
		if (Run->TimeTo100 < 0.f && Speed >= 100.f)
		{
			Run->TimeTo100 = Elapsed;
		}
		Run->MaxVerticalSpeed = FMath::Max(Run->MaxVerticalSpeed, FMath::Abs(static_cast<float>(Car->GetVelocity().Z)));
		if (Run->TimeTo100 < 0.f && Elapsed < DriveTimeoutSeconds)
		{
			return false;
		}

		UE_LOG(LogTemp, Display, TEXT("BN drive test: 0-100 km/h in %.2fs (%.1f km/h at stop), gear %d, max body vertical speed %.1f cm/s"),
			Run->TimeTo100, Speed, Car->GetChaosMovement()->GetCurrentGear(), Run->MaxVerticalSpeed);
		TestTrue(TEXT("car reaches 100 km/h"), Run->TimeTo100 >= 0.f);
		TestTrue(TEXT("0-100 km/h is not arcade-fast"), Run->TimeTo100 < 0.f || Run->TimeTo100 >= Min0To100Seconds);
		TestTrue(TEXT("0-100 km/h is not sluggish"), Run->TimeTo100 <= Max0To100Seconds);
		TestTrue(TEXT("body does not hop on flat ground"), Run->MaxVerticalSpeed < MaxBodyVerticalSpeedCms);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Run]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car || Run->TimeTo100 < 0.f)
		{
			return true; // Already reported above.
		}
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (Run->CornerStart < 0.0)
		{
			Run->CornerStart = Now;
			Move->SetThrottleInput(CornerThrottle);
			Move->SetSteeringInput(1.f);
		}
		if (FMath::Abs(Car->GetSpeedKmh()) > MinSpeedForSlipKmh)
		{
			Run->MaxSlipDegrees = FMath::Max(Run->MaxSlipDegrees, SlipAngleDegrees(*Car));
		}
		if (Now - Run->CornerStart < CornerSeconds)
		{
			return false;
		}

		UE_LOG(LogTemp, Display, TEXT("BN drive test: full lock from 100 km/h, max slip angle %.1f deg, %.1f km/h after %.0fs"),
			Run->MaxSlipDegrees, Car->GetSpeedKmh(), CornerSeconds);
		TestTrue(TEXT("car grips in a full-lock corner instead of sliding out"), Run->MaxSlipDegrees < MaxCornerSlipDegrees);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Run]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car || Run->TimeTo100 < 0.f)
		{
			return true;
		}
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (Run->PowerTurnStart < 0.0)
		{
			if (Run->StopStart < 0.0)
			{
				Run->StopStart = Now;
				Move->SetThrottleInput(0.f);
				Move->SetSteeringInput(0.f);
				Move->SetBrakeInput(1.f);
			}
			const bool bStopped = FMath::Abs(Car->GetSpeedKmh()) < StopSpeedKmh;
			if (!bStopped && Now - Run->StopStart < StopTimeoutSeconds)
			{
				return false;
			}
			Run->PowerTurnStart = Now;
			Move->SetBrakeInput(0.f);
			Move->SetThrottleInput(1.f);
			Move->SetSteeringInput(1.f);
		}
		if (FMath::Abs(Car->GetSpeedKmh()) > MinSpeedForSlipKmh)
		{
			Run->MaxPowerTurnSlipDegrees = FMath::Max(Run->MaxPowerTurnSlipDegrees, SlipAngleDegrees(*Car));
		}
		if (Now - Run->PowerTurnStart < PowerTurnSeconds)
		{
			return false;
		}

		UE_LOG(LogTemp, Display, TEXT("BN drive test: full throttle + full lock from standstill, max slip angle %.1f deg, %.1f km/h after %.0fs"),
			Run->MaxPowerTurnSlipDegrees, Car->GetSpeedKmh(), PowerTurnSeconds);
		TestTrue(TEXT("rear does not step out under full throttle in a tight turn"), Run->MaxPowerTurnSlipDegrees < MaxPowerTurnSlipDegrees);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
