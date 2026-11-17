#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Tests/BNPlayTestUtils.h"

using namespace BNPlayTest;

namespace
{
	/** Skidpad from Tools/build_test_map.py: start at its south end heading +Y, 45 m of room either side. */
	const FVector SkidpadStart(25000.f, -8000.f, 60.f);
	constexpr float SkidpadYaw = 90.f;
	constexpr float SettleSeconds = 1.f;

	/** Signed slip: positive when the car is moving to the right of where it points. */
	float SignedSlipDegrees(const AActor& Car)
	{
		const FVector Velocity = Car.GetVelocity().GetSafeNormal2D();
		const FVector Forward = Car.GetActorForwardVector().GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, Velocity).Z, FVector::DotProduct(Forward, Velocity)));
	}

	// Drift script and pass bars.
	constexpr float EntrySpeedKmh = 70.f;
	constexpr float EntryTimeoutSeconds = 10.f;
	constexpr float KickSeconds = 0.7f;
	constexpr float KickThrottle = 0.5f;
	constexpr float HoldSeconds = 2.5f;
	constexpr float HoldThrottle = 0.6f;
	constexpr float CountersteerFullLockDegrees = 30.f;
	constexpr float RecoverTimeoutSeconds = 3.f;
	/** Initiation: drift angle must be reached within this long of pulling the handbrake. */
	constexpr float InitiationWindowSeconds = 1.5f;
	constexpr float MinKickSlipDegrees = 20.f;
	constexpr float DriftSlipDegrees = 10.f;
	constexpr float MinDriftHeldSeconds = 1.f;
	constexpr float SpinSlipDegrees = 75.f;
	constexpr float GripSlipDegrees = 5.f;
	constexpr float MinSpeedForSlipKmh = 15.f;

	struct FDriftState
	{
		enum class EPhase { Place, Settle, Accelerate, Kick, Hold, Recover } Phase = EPhase::Place;
		double PhaseStart = 0.0;
		double LastTime = 0.0;
		float KickPeakSlip = 0.f; // peak within InitiationWindowSeconds of the handbrake pull
		double KickStart = 0.0;
		float HoldPeakSlip = 0.f;
		float DriftHeldSeconds = 0.f;
		float RecoverSeconds = -1.f;
	};

	void Countersteer(UChaosWheeledVehicleMovementComponent& Move, float SignedSlip)
	{
		Move.SetSteeringInput(FMath::Clamp(SignedSlip / CountersteerFullLockDegrees, -1.f, 1.f));
	}
}

// Free-driving drift on the skidpad, the way a player does it: build speed, kick the rear out with
// steering + handbrake, hold the slide with throttle and countersteer, then lift and let the car grip up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleDriftTest, "BlackNumbers.Vehicle.Drift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleDriftTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FDriftState> State = MakeShared<FDriftState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FDriftState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared"));
			return true;
		}
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		const float Dt = static_cast<float>(Now - State->LastTime);
		State->LastTime = Now;
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		const float Slip = FMath::Abs(Car->GetSpeedKmh()) > MinSpeedForSlipKmh ? SignedSlipDegrees(*Car) : 0.f;
		auto Enter = [&](EPhase Phase) { State->Phase = Phase; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::Place:
			PlaceStopped(*Car, SkidpadStart, SkidpadYaw);
			Enter(EPhase::Settle);
			return false;
		case EPhase::Settle:
			if (InPhase >= SettleSeconds)
			{
				Move->SetThrottleInput(1.f);
				Enter(EPhase::Accelerate);
			}
			return false;
		case EPhase::Accelerate:
			if (Car->GetSpeedKmh() >= EntrySpeedKmh || InPhase > EntryTimeoutSeconds)
			{
				Move->SetThrottleInput(KickThrottle);
				Move->SetSteeringInput(1.f);
				Move->SetHandbrakeInput(true);
				Enter(EPhase::Kick);
				State->KickStart = Now;
			}
			return false;
		case EPhase::Kick:
			State->KickPeakSlip = FMath::Max(State->KickPeakSlip, FMath::Abs(Slip));
			if (InPhase >= KickSeconds)
			{
				Move->SetHandbrakeInput(false);
				Move->SetThrottleInput(HoldThrottle);
				Enter(EPhase::Hold);
			}
			return false;
		case EPhase::Hold:
			Countersteer(*Move, Slip);
			if (Now - State->KickStart <= InitiationWindowSeconds)
			{
				State->KickPeakSlip = FMath::Max(State->KickPeakSlip, FMath::Abs(Slip));
			}
			State->HoldPeakSlip = FMath::Max(State->HoldPeakSlip, FMath::Abs(Slip));
			State->DriftHeldSeconds += FMath::Abs(Slip) > DriftSlipDegrees ? Dt : 0.f;
			if (InPhase >= HoldSeconds)
			{
				Move->SetThrottleInput(0.f);
				Enter(EPhase::Recover);
			}
			return false;
		case EPhase::Recover:
			Countersteer(*Move, Slip);
			if (FMath::Abs(Slip) > GripSlipDegrees && InPhase < RecoverTimeoutSeconds)
			{
				return false;
			}
			State->RecoverSeconds = FMath::Abs(Slip) <= GripSlipDegrees ? InPhase : -1.f;
			break;
		}

		UE_LOG(LogTemp, Display, TEXT("BN drift test: kick peak %.1f deg, held %.2fs above %.0f deg (peak %.1f), grip back in %.2fs, %.1f km/h"),
			State->KickPeakSlip, State->DriftHeldSeconds, DriftSlipDegrees, State->HoldPeakSlip, State->RecoverSeconds, Car->GetSpeedKmh());
		TestTrue(TEXT("steering + handbrake starts a drift"), State->KickPeakSlip > MinKickSlipDegrees);
		TestTrue(TEXT("throttle + countersteer holds the drift"), State->DriftHeldSeconds >= MinDriftHeldSeconds);
		TestTrue(TEXT("drift stays controllable (no spin)"), State->HoldPeakSlip < SpinSlipDegrees);
		TestTrue(TEXT("car grips up after lifting off"), State->RecoverSeconds >= 0.f);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
	constexpr float ReverseSeconds = 4.f;
	constexpr float MinReverseKmh = 10.f;
}

// Holding brake at a standstill must engage reverse and back the car up (auto-reverse).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleReverseTest, "BlackNumbers.Vehicle.Reverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleReverseTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<double> Start = MakeShared<double>(-1.0);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Start]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared"));
			return true;
		}
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (*Start < 0.0)
		{
			PlaceStopped(*Car, SkidpadStart, SkidpadYaw);
			Car->GetChaosMovement()->SetBrakeInput(1.f);
			*Start = Now;
			return false;
		}
		if (Now - *Start < ReverseSeconds)
		{
			return false;
		}
		const float Speed = Car->GetSpeedKmh();
		const int32 Gear = Car->GetChaosMovement()->GetCurrentGear();
		UE_LOG(LogTemp, Display, TEXT("BN reverse test: %.1f km/h in gear %d after holding brake %.0fs"), Speed, Gear, ReverseSeconds);
		TestTrue(TEXT("reverse gear engaged"), Gear < 0);
		TestTrue(TEXT("car backs up"), Speed < -MinReverseKmh);
		Car->GetChaosMovement()->SetBrakeInput(0.f);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
