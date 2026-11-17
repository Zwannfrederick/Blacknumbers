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

	// NFS-style drift script and pass bars: kick with handbrake + steering, hold with the wheel into the turn
	// (half lock, then full lock) on full throttle, then let go of the wheel.
	constexpr float EntrySpeedKmh = 70.f;
	constexpr float EntryTimeoutSeconds = 10.f;
	constexpr float KickSeconds = 0.5f;
	constexpr float KickThrottle = 0.5f;
	constexpr float HoldSeconds = 3.f;
	constexpr float HalfLock = 0.5f;
	constexpr float FullLock = 1.f;
	constexpr float RecoverTimeoutSeconds = 2.f;
	constexpr float MinKickSlipDegrees = 15.f;
	constexpr float DriftMinSlipDegrees = 10.f;
	constexpr float DriftMaxSlipDegrees = 60.f;
	constexpr float MinSustainedFraction = 0.9f;
	constexpr float MinAngleGainDegrees = 5.f;
	constexpr float MinDriftSpeedKmh = 40.f;
	constexpr float SpinSlipDegrees = 75.f;
	constexpr float GripSlipDegrees = 5.f;
	constexpr float MinSpeedForSlipKmh = 15.f;

	struct FDriftState
	{
		enum class EPhase { Place, Settle, Accelerate, Kick, HoldHalf, HoldFull, Release } Phase = EPhase::Place;
		double PhaseStart = 0.0;
		double LastTime = 0.0;
		float KickPeakSlip = 0.f;
		float Held = 0.f;
		float InBand = 0.f;
		float SlipSum[2] = { 0.f, 0.f };
		float Time[2] = { 0.f, 0.f };
		float PeakSlip = 0.f;
		float HoldEndKmh = 0.f;
		float RecoverSeconds = -1.f;
	};
}

// Free-driving drift on the skidpad the way a player does it, checked for what makes it feel right: it starts,
// it holds for as long as the wheel is held into the turn, more lock = more angle, it never spins, it keeps its
// speed, and letting go of the wheel ends it cleanly.
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
		const float Slip = FMath::Abs(Car->GetSpeedKmh()) > MinSpeedForSlipKmh ? FMath::Abs(SignedSlipDegrees(*Car)) : 0.f;
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
				// Regression: ground contact once came from IsMovingOnGround(), always false for Chaos vehicles,
				// which silently disabled the drift assist, the skid sound and the airborne-only damping.
				TestTrue(TEXT("car driving on asphalt reads as on the ground"), Car->IsOnGround());
				Move->SetThrottleInput(KickThrottle);
				Move->SetSteeringInput(FullLock);
				Move->SetHandbrakeInput(true);
				Enter(EPhase::Kick);
			}
			return false;
		case EPhase::Kick:
			State->KickPeakSlip = FMath::Max(State->KickPeakSlip, Slip);
			if (InPhase >= KickSeconds)
			{
				Move->SetHandbrakeInput(false);
				Move->SetThrottleInput(1.f);
				Move->SetSteeringInput(HalfLock);
				Enter(EPhase::HoldHalf);
			}
			return false;
		case EPhase::HoldHalf:
		case EPhase::HoldFull:
		{
			const int32 Lock = State->Phase == EPhase::HoldFull ? 1 : 0;
			State->Held += Dt;
			State->InBand += (Slip >= DriftMinSlipDegrees && Slip <= DriftMaxSlipDegrees) ? Dt : 0.f;
			State->SlipSum[Lock] += Slip * Dt;
			State->Time[Lock] += Dt;
			State->PeakSlip = FMath::Max(State->PeakSlip, Slip);
			if (InPhase < HoldSeconds)
			{
				return false;
			}
			if (State->Phase == EPhase::HoldHalf)
			{
				Move->SetSteeringInput(FullLock);
				Enter(EPhase::HoldFull);
				return false;
			}
			State->HoldEndKmh = Car->GetSpeedKmh();
			Move->SetSteeringInput(0.f);
			Enter(EPhase::Release);
			return false;
		}
		case EPhase::Release:
			if ((Slip > GripSlipDegrees || Car->IsDrifting()) && InPhase < RecoverTimeoutSeconds)
			{
				return false;
			}
			State->RecoverSeconds = InPhase < RecoverTimeoutSeconds ? InPhase : -1.f;
			break;
		}

		const float Sustained = State->InBand / FMath::Max(State->Held, KINDA_SMALL_NUMBER);
		const float MeanHalf = State->SlipSum[0] / FMath::Max(State->Time[0], KINDA_SMALL_NUMBER);
		const float MeanFull = State->SlipSum[1] / FMath::Max(State->Time[1], KINDA_SMALL_NUMBER);
		UE_LOG(LogTemp, Display, TEXT("BN drift test: kick %.1f deg, %.0f%% of %.1fs in %.0f-%.0f deg, mean %.1f deg at half lock, %.1f at full, peak %.1f, %.1f km/h at end of hold, grip back in %.2fs, %.1f km/h"),
			State->KickPeakSlip, Sustained * 100.f, State->Held, DriftMinSlipDegrees, DriftMaxSlipDegrees, MeanHalf, MeanFull,
			State->PeakSlip, State->HoldEndKmh, State->RecoverSeconds, Car->GetSpeedKmh());
		TestTrue(TEXT("steering + handbrake starts a drift"), State->KickPeakSlip > MinKickSlipDegrees);
		TestTrue(TEXT("drift holds while the wheel is held into the turn"), Sustained >= MinSustainedFraction);
		TestTrue(TEXT("more lock, deeper drift"), MeanFull >= MeanHalf + MinAngleGainDegrees);
		TestTrue(TEXT("drift never spins"), State->PeakSlip < SpinSlipDegrees);
		TestTrue(TEXT("throttle keeps the drift's speed"), State->HoldEndKmh >= MinDriftSpeedKmh);
		TestTrue(TEXT("letting go of the wheel ends the drift"), State->RecoverSeconds >= 0.f);
		Move->SetThrottleInput(0.f);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
	// Handbrake abuse: what a player does when mashing Space at speed with full lock and W held down.
	constexpr float AbuseEntrySpeedKmh = 80.f;
	constexpr float AbuseHoldSeconds = 1.5f;
	constexpr float AbuseRecoverSeconds = 2.5f;
	constexpr float SpunSlipDegrees = 90.f;
	constexpr float SettledSlipDegrees = 15.f;

	struct FAbuseState
	{
		enum class EPhase { Place, Settle, Accelerate, Hold, Recover } Phase = EPhase::Place;
		double PhaseStart = 0.0;
		float MaxSlip = 0.f;
		float MaxSkid = 0.f;
		float MaxWheelSlip = 0.f;
	};
}

// Holding the handbrake with full lock must give a big slide, not a spin the player cannot catch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleHandbrakeTest, "BlackNumbers.Vehicle.HandbrakeNoSpin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleHandbrakeTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FAbuseState> State = MakeShared<FAbuseState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FAbuseState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared"));
			return true;
		}
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		const float Slip = FMath::Abs(SignedSlipDegrees(*Car)) * (FMath::Abs(Car->GetSpeedKmh()) > MinSpeedForSlipKmh);
		auto Enter = [&](EPhase Phase) { State->Phase = Phase; State->PhaseStart = Now; };
		if (State->Phase == EPhase::Hold || State->Phase == EPhase::Recover)
		{
			State->MaxSlip = FMath::Max(State->MaxSlip, Slip);
			for (int32 i = 0; i < Move->GetNumWheels(); ++i)
			{
				State->MaxSkid = FMath::Max(State->MaxSkid, Move->GetWheelState(i).SkidMagnitude);
				State->MaxWheelSlip = FMath::Max(State->MaxWheelSlip, Move->GetWheelState(i).SlipMagnitude);
			}
		}

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
			if (Car->GetSpeedKmh() >= AbuseEntrySpeedKmh || InPhase > EntryTimeoutSeconds)
			{
				// Throttle stays pinned: players hold W while mashing Space.
				Move->SetSteeringInput(1.f);
				Move->SetHandbrakeInput(true);
				Enter(EPhase::Hold);
			}
			return false;
		case EPhase::Hold:
			if (InPhase >= AbuseHoldSeconds)
			{
				Move->SetHandbrakeInput(false);
				Move->SetSteeringInput(0.f);
				Enter(EPhase::Recover);
			}
			return false;
		case EPhase::Recover:
			if (InPhase < AbuseRecoverSeconds)
			{
				return false;
			}
			break;
		}

		UE_LOG(LogTemp, Display, TEXT("BN handbrake test: max slip %.1f deg, settled at %.1f deg, %.1f km/h | max wheel skid %.2f, max wheel slip %.2f"),
			State->MaxSlip, Slip, Car->GetSpeedKmh(), State->MaxSkid, State->MaxWheelSlip);
		TestTrue(TEXT("car does not spin round under handbrake + full lock"), State->MaxSlip < SpunSlipDegrees);
		TestTrue(TEXT("car settles once the handbrake is released"), Slip < SettledSlipDegrees);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
	// Drift exit: kick a drift, release the handbrake, straighten and floor it.
	constexpr float ExitKickSeconds = 0.7f;
	constexpr float ExitPowerSeconds = 2.5f;
	constexpr float MinExitSpeedGainKmh = 10.f;

	struct FExitState
	{
		enum class EPhase { Place, Settle, Accelerate, Kick, Power } Phase = EPhase::Place;
		double PhaseStart = 0.0;
		float ReleaseSpeed = 0.f;
	};
}

// Releasing the handbrake and flooring it must pull the car out of the slide and accelerate it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNVehicleDriftExitTest, "BlackNumbers.Vehicle.DriftExitAccelerates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNVehicleDriftExitTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FExitState> State = MakeShared<FExitState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FExitState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car)
		{
			AddError(TEXT("Player car disappeared"));
			return true;
		}
		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
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
			}
			return false;
		case EPhase::Kick:
			if (InPhase >= ExitKickSeconds)
			{
				Move->SetHandbrakeInput(false);
				Move->SetSteeringInput(0.f);
				Move->SetThrottleInput(1.f);
				State->ReleaseSpeed = Car->GetSpeedKmh();
				Enter(EPhase::Power);
			}
			return false;
		case EPhase::Power:
			if (InPhase < ExitPowerSeconds)
			{
				return false;
			}
			break;
		}

		const float Gain = Car->GetSpeedKmh() - State->ReleaseSpeed;
		UE_LOG(LogTemp, Display, TEXT("BN drift exit test: %.1f km/h at release, %.1f km/h after %.1fs full throttle (%+.1f), slip %.1f deg, drifting %d"),
			State->ReleaseSpeed, Car->GetSpeedKmh(), ExitPowerSeconds, Gain, Car->GetSlipAngleDegrees(), Car->IsDrifting());
		TestTrue(TEXT("car accelerates out of a drift once the handbrake is released"), Gain >= MinExitSpeedGainKmh);
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
