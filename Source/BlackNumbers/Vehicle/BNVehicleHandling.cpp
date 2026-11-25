#include "Vehicle/BNVehiclePawn.h"

#include "Audio/BNAudioSubsystem.h"
#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

// Arcade handling: wheel contact, slip angle, handbrake drift assist and rear grip blending.
namespace
{
	constexpr float MinSpeedForSlipKmh = 5.f;
	/** Steering needed to start a drift with the handbrake, and to count as "into the turn" while holding one. */
	constexpr float DriftStartMinSteer = 0.3f;
	constexpr float DriftHoldMinSteer = 0.1f;
	constexpr float DriftEndSpeedFraction = 0.66f;
	/** Fastest the car is turned by the assist (deg/s), so drifts steer smoothly without snapping. */
	constexpr float MaxAssistYawRate = 125.f;
	constexpr float PlayerGripAssistDegPerSec = 55.f;
	constexpr float MinNormalAssistSpeedKmh = 25.f;
}

bool ABNVehiclePawn::IsOnGround() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	for (int32 i = 0; i < Move->Wheels.Num(); ++i)
	{
		if (Move->GetWheelState(i).bInContact)
		{
			return true;
		}
	}
	return false;
}

float ABNVehiclePawn::GetSlipAngleDegrees() const
{
	const FVector Velocity = GetVelocity().GetSafeNormal2D();
	if (FMath::Abs(GetSpeedKmh()) < MinSpeedForSlipKmh || Velocity.IsNearlyZero())
	{
		return 0.f;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, Velocity).Z, FVector::DotProduct(Forward, Velocity)));
}

void ABNVehiclePawn::UpdateDrift(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const bool bHandbrake = Move->GetHandbrakeInput();
	if (bHandbrake != bRearAbsBypassed)
	{
		bRearAbsBypassed = bHandbrake;
		for (int32 i = FirstRearWheelIndex; i < Move->Wheels.Num(); ++i)
		{
			Move->SetABSEnabled(i, !bHandbrake && Move->Wheels[i]->bABSEnabled);
		}
	}

	const float Speed = FMath::Abs(GetSpeedKmh());
	const float Steer = Move->GetSteeringInput();
	const float Throttle = Move->GetThrottleInput();
	if (!bDrifting && bHandbrake && Speed >= Drift.MinSpeedKmh && FMath::Abs(Steer) >= DriftStartMinSteer)
	{
		bDrifting = true;
		DriftHoldSeconds = 0.f;
		DriftDirection = Steer > 0.f ? -1.f : 1.f;
		DriftEntrySpeed = GetMesh()->GetPhysicsLinearVelocity().Size2D();
		CurrentDriftTargetSlip = GetSlipAngleDegrees();
	}

	if (bDrifting)
	{
		if (Speed >= Drift.MinSpeedKmh && FMath::Abs(GetSlipAngleDegrees()) >= 10.f)
		{
			DriftHoldSeconds += DeltaSeconds;
		}
		const float Into = -Steer * DriftDirection;
		const float TargetAngle = Into > DriftHoldMinSteer
			? FMath::GetMappedRangeValueClamped(FVector2f(DriftHoldMinSteer, 1.f), FVector2f(Drift.MinAngleDegrees, Drift.MaxAngleDegrees), Into)
			: 0.f;
		const float RawTarget = TargetAngle * DriftDirection;
		const float InterpRate = (TargetAngle == 0.f || RawTarget * CurrentDriftTargetSlip < 0.f) ? 8.5f : 6.5f;
		CurrentDriftTargetSlip = FMath::FInterpTo(CurrentDriftTargetSlip, RawTarget, DeltaSeconds, InterpRate);

		const bool bUnwound = TargetAngle == 0.f && FMath::Abs(GetSlipAngleDegrees()) < Drift.ExitAngleDegrees;
		if (bUnwound || Speed < Drift.MinSpeedKmh * DriftEndSpeedFraction)
		{
			// Reward a sustained clean drift exit with a punchy arcade turbo surge & NOS drip!
			if (bUnwound && IsPlayerControlled() && DriftHoldSeconds >= 1.1f && Speed >= 45.f)
			{
				DriftExitBoostRemaining = 0.85f;
				DriftExitBannerRemaining = 1.6f;
				GearShiftKick = FMath::Max(GearShiftKick, 0.75f);
				BoostState.TriggerBackfire(0.32f);
				BoostState.NosFraction = FMath::Min(1.f, BoostState.NosFraction + 0.08f);
				if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
				{
					Audio->PlaySfx2D(EBNSfxId::NosIgnite, 0.52f);
				}
			}
			bDrifting = false;
			DriftHoldSeconds = 0.f;
			CurrentDriftTargetSlip = 0.f;
		}
		else if (IsOnGround())
		{
			ApplyDriftAssist(DeltaSeconds, CurrentDriftTargetSlip, Throttle);
		}
	}
	SetRearGrip(bDrifting ? 0.f : FMath::Min(1.f, RearGripBlend + DeltaSeconds / Drift.GripRecoverSeconds));
}

void ABNVehiclePawn::UpdateNormalGripAssist(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const float SpeedKmh = FMath::Abs(GetSpeedKmh());
	if (bDrifting || bInputLocked || !IsPlayerControlled() || !IsOnGround()
		|| Move->GetHandbrakeInput() || SpeedKmh < MinNormalAssistSpeedKmh)
	{
		return;
	}
	USkeletalMeshComponent* Body = GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (Velocity.Size2D() < 600.f)
	{
		return;
	}
	const float Slip = GetSlipAngleDegrees();
	const float AbsSlip = FMath::Abs(Slip);
	if (AbsSlip > 0.5f && AbsSlip < 35.f)
	{
		const float MaxTurn = PlayerGripAssistDegPerSec * DeltaSeconds;
		const float Turn = -FMath::Clamp(Slip, -MaxTurn, MaxTurn);
		const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
		Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
	}
	if (FMath::Abs(Move->GetSteeringInput()) < 0.08f)
	{
		const FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
		Body->SetPhysicsAngularVelocityInDegrees(FVector(Spin.X, Spin.Y, FMath::FInterpTo(Spin.Z, 0.f, DeltaSeconds, 5.f)));
	}
}

void ABNVehiclePawn::ApplyDriftAssist(float DeltaSeconds, float TargetSlip, float Throttle)
{
	USkeletalMeshComponent* Body = GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Slip = GetSlipAngleDegrees();

	if (Throttle < 0.15f || GetChaosMovement()->GetBrakeInput() > 0.1f)
	{
		DriftEntrySpeed = FMath::Max(0.f, DriftEntrySpeed - 650.f * DeltaSeconds);
	}

	const float SpeedScale = FMath::GetMappedRangeValueClamped(FVector2f(80.f, 200.f), FVector2f(1.0f, 0.55f), FMath::Abs(GetSpeedKmh()));
	const float TravelYawRate = -Drift.LineTightness * SpeedScale * Slip;
	const float DesiredYawRate = FMath::Clamp(TravelYawRate - Drift.AngleGain * (TargetSlip - Slip), -MaxAssistYawRate, MaxAssistYawRate);

	const float Push = FMath::Max(Throttle, 0.f) * Drift.Thrust * DeltaSeconds;
	const float Speed2D = FMath::Max(Velocity.Size2D(), FMath::Min(Velocity.Size2D() + Push, DriftEntrySpeed));
	const float TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)) + TravelYawRate * DeltaSeconds;
	const FVector Travel = FRotator(0.f, TravelYaw, 0.f).Vector() * Speed2D;
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
	const FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
	Body->SetPhysicsAngularVelocityInDegrees(FVector(Spin.X, Spin.Y, FMath::FInterpTo(Spin.Z, DesiredYawRate, DeltaSeconds, 14.f)));
}

void ABNVehiclePawn::SetRearGrip(float Blend)
{
	const bool bTractionControl = !bDrifting;
	if (Blend == RearGripBlend && bTractionControl == bRearTractionControl && LastAppliedTireGripScale == ActiveTireGripScale)
	{
		return;
	}
	RearGripBlend = Blend;
	bRearTractionControl = bTractionControl;
	LastAppliedTireGripScale = ActiveTireGripScale;
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	for (int32 i = FirstRearWheelIndex; i < Move->Wheels.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[i];
		Move->SetWheelFrictionMultiplier(i, Wheel->FrictionForceMultiplier * ActiveTireGripScale * FMath::Lerp(Drift.RearGripScale, 1.f, Blend));
		Move->SetTractionControlEnabled(i, bTractionControl && Wheel->bTractionControlEnabled);
	}
}
