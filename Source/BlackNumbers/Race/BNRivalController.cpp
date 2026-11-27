#include "Race/BNRivalController.h"
#include "Story/BNStoryTypes.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNTrafficSubsystem.h"

namespace
{
	constexpr float Gravity = 980.f;
	constexpr float MaxSpeed = 7000.f; // cm/s, ~250 km/h: nothing on the line limits a straight
	constexpr float SampleStep = 500.f;
	constexpr float LookAheadHorizon = 20000.f;
	/** Pure-pursuit aim point: SteerLookAheadSeconds of travel ahead, clamped. */
	constexpr float SteerLookAheadSeconds = 0.55f;
	constexpr float MinSteerLookAhead = 650.f;
	constexpr float MaxSteerLookAhead = 2400.f;
	/** Speed error (cm/s) that gives full throttle / full brake. */
	constexpr float FullThrottleError = 280.f;
	constexpr float FullBrakeError = 520.f;
	/** Below this the brake pedal would select reverse (reverse-as-brake), so it is not used. */
	constexpr float MinBrakingSpeed = 200.f;
	/** A car this close ahead and this close sideways is "in my lane". */
	constexpr float FollowDistance = 2600.f;
	constexpr float LaneHalfWidth = 280.f;
	/** Pass when the car ahead holds us this much (cm/s) under our safe speed. */
	constexpr float OvertakeMargin = 150.f;
	/** The other lane must be clear from this far behind our nose. */
	constexpr float BesideDistance = 700.f;
	constexpr float LaneChangeSeconds = 3.f;
	/** Sideways speed of a lane change along the line (cm/s): a drift across, not a swerve. */
	constexpr float LaneShiftSpeed = 250.f;
	/** Only change lanes where the line allows this speed (cm/s, ~110 km/h), i.e. not into or through a corner. */
	constexpr float MinLaneChangeSafeSpeed = 3000.f;
	constexpr float StuckSpeed = 220.f;
	constexpr float UpsideDownZ = 0.32f;
	constexpr float ReverseTriggerSeconds = 0.85f;
	constexpr float ReverseDurationSeconds = 0.85f;
	constexpr float StuckSecondsToRespawn = 3.8f;
	constexpr float ProgressStuckSecondsToRespawn = 4.6f;
	constexpr float CmPerSecToMph = 0.022369f;
	/** Stanley-style correction: steer back toward the line at this rate (1/s) of the sideways error. */
	constexpr float CrossTrackGain = 1.15f;
	constexpr float MinCrossTrackSpeed = 480.f;

	// Skill 0 -> 1. Realistic Chaos cornering envelope (tyres + critically-damped yaw/velocity grip assist).
	const FVector2f GripRange(0.88f, 1.26f);
	const FVector2f BrakeRange(0.88f, 1.15f);
	/** Arcade grip: travel direction swings toward the nose at up to this rate (deg/s). */
	constexpr float GripAssistDegreesPerSecond = 140.f;
	/** Critically-damped yaw-rate assist proportional to Aim heading error (capped deg/s, zero fishtailing). */
	constexpr float YawAssistGain = 1.85f;
	constexpr float YawAssistMaxDegPerSec = 68.f;
	constexpr float MinGripAssistSpeed = 420.f;
	const FVector2f LaunchDelayRange(0.65f, 0.1f);

	// Rubber band. Inside the dead zone racing is untouched; beyond it the effect ramps over the span.
	constexpr float CatchUpDeadZoneCm = 8000.f;
	constexpr float CatchUpSpanCm = 25000.f;
	constexpr float EaseOffDeadZoneCm = 12000.f;
	constexpr float EaseOffSpanCm = 30000.f;
	/** Extra forward acceleration (cm/s^2) at the full catch-up scale, so trailing rivals gain on straights. */
	constexpr float CatchUpAccelCms2 = 900.f;

	float WrapDistance(const USplineComponent& Line, float Distance)
	{
		const float Length = Line.GetSplineLength();
		return Line.IsClosedLoop() ? FMath::Fmod(FMath::Fmod(Distance, Length) + Length, Length) : FMath::Clamp(Distance, 0.f, Length);
	}
}

ABNRivalController::ABNRivalController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABNRivalController::Setup(ABNRaceEvent* InRace, float InSkill, float InLaneOffset, EBNRivalStyle InStyle)
{
	Style = InStyle;
	Race = InRace;
	Skill = FMath::Clamp(InSkill, 0.f, 1.f);
	LaneOffset = FMath::Clamp(InLaneOffset, -380.f, 380.f);
	TargetLaneOffset = LaneOffset;
	LaunchDelay = FMath::Lerp(LaunchDelayRange.X, LaunchDelayRange.Y, Skill);
	TrackedDistance = -1.f;
	LastProgressDistance = -1.f;
	ReverseUnstuckSeconds = 0.f;
	ReverseCooldownSeconds = 0.f;
}

float ABNRivalController::CatchUpScale(float GapCm, float Skill)
{
	const float S = FMath::Clamp(Skill, 0.f, 1.f);
	if (GapCm < -CatchUpDeadZoneCm)
	{
		const float T = FMath::Clamp((-GapCm - CatchUpDeadZoneCm) / CatchUpSpanCm, 0.f, 1.f);
		return 1.f + T * (0.10f + 0.06f * S);
	}
	if (GapCm > EaseOffDeadZoneCm)
	{
		const float T = FMath::Clamp((GapCm - EaseOffDeadZoneCm) / EaseOffSpanCm, 0.f, 1.f);
		return 1.f - T * (0.14f - 0.10f * S);
	}
	return 1.f;
}

float ABNRivalController::GapToPlayer(const USplineComponent& Line, float MyDistance) const
{
	const ABNRaceEvent* RaceEvent = Race.Get();
	const APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	if (!RaceEvent || !PlayerPawn || !RaceEvent->IsRunningFor(Cast<ABNVehiclePawn>(PlayerPawn)))
	{
		return 0.f;
	}
	const float PlayerDistance = Line.GetDistanceAlongSplineAtLocation(PlayerPawn->GetActorLocation(), ESplineCoordinateSpace::World);
	float Gap = MyDistance - PlayerDistance;
	// ponytail: lap-agnostic on circuits (gap wrapped to +-half a lap); add lap counts if lapping becomes common.
	const float Len = Line.GetSplineLength();
	if (Line.IsClosedLoop() && Len > 1.f)
	{
		Gap = FMath::Fmod(Gap + 1.5f * Len, Len) - 0.5f * Len;
	}
	return Gap;
}

float ABNRivalController::SafeSpeed(const USplineComponent& Line, float Distance, float GripG, float BrakeG)
{
	const float Length = Line.GetSplineLength();
	const float Brake = BrakeG * Gravity;
	float Best = MaxSpeed;
	for (float Ahead = 0.f; Ahead <= LookAheadHorizon; Ahead += SampleStep)
	{
		const float At = Distance + Ahead;
		if (!Line.IsClosedLoop() && At + SampleStep > Length)
		{
			// The line ends here: be able to stop by its end.
			return FMath::Min(Best, FMath::Sqrt(2.f * Brake * FMath::Max(Length - Distance, 0.f)));
		}
		const FVector A = Line.GetDirectionAtDistanceAlongSpline(WrapDistance(Line, At), ESplineCoordinateSpace::World).GetSafeNormal2D();
		const FVector B = Line.GetDirectionAtDistanceAlongSpline(WrapDistance(Line, At + SampleStep), ESplineCoordinateSpace::World).GetSafeNormal2D();
		const float Turn = FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f));
		if (Turn < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float CornerSpeedSq = GripG * Gravity * (SampleStep / Turn); // v^2 = a * R
		Best = FMath::Min(Best, FMath::Sqrt(CornerSpeedSq + 2.f * Brake * Ahead));
	}
	return Best;
}

void ABNRivalController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn());
	const ABNRaceEvent* RaceEvent = Race.Get();
	const USplineComponent* Line = RaceEvent ? RaceEvent->GetRacingLine() : nullptr;
	if (!Car || !Line || Line->GetNumberOfSplinePoints() < 2 || Car->IsInputLocked())
	{
		SecondsUnlocked = 0.f;
		return;
	}
	SecondsUnlocked += DeltaSeconds;
	if (SecondsUnlocked < LaunchDelay)
	{
		return; // reaction time at the green light
	}
	Drive(*Car, *Line, DeltaSeconds);
}

float ABNRivalController::FindTrackedDistance(const USplineComponent& Line, const FVector& Location) const
{
	const float SplineLen = Line.GetSplineLength();
	if (TrackedDistance < 0.f || SplineLen < 200.f)
	{
		return Line.GetDistanceAlongSplineAtLocation(Location, ESplineCoordinateSpace::World);
	}

	auto SampleDistSq = [&](float D)
	{
		const FVector P = Line.GetLocationAtDistanceAlongSpline(WrapDistance(Line, D), ESplineCoordinateSpace::World);
		const FVector Diff = Location - P;
		return Diff.X * Diff.X + Diff.Y * Diff.Y + (Diff.Z * 2.f) * (Diff.Z * 2.f);
	};

	float BestD = TrackedDistance;
	float BestSq = SampleDistSq(BestD);
	for (float Off = -1400.f; Off <= 4200.f; Off += 150.f)
	{
		const float Cand = TrackedDistance + Off;
		if (!Line.IsClosedLoop() && (Cand < 0.f || Cand > SplineLen))
		{
			continue;
		}
		const float Sq = SampleDistSq(Cand);
		if (Sq < BestSq)
		{
			BestSq = Sq;
			BestD = Cand;
		}
	}
	// Refine to +-25 cm accuracy around best coarse sample
	const float CoarseD = BestD;
	for (float Off = -150.f; Off <= 150.f; Off += 25.f)
	{
		const float Cand = CoarseD + Off;
		if (!Line.IsClosedLoop() && (Cand < 0.f || Cand > SplineLen))
		{
			continue;
		}
		const float Sq = SampleDistSq(Cand);
		if (Sq < BestSq)
		{
			BestSq = Sq;
			BestD = Cand;
		}
	}
	// Fallback to global spline query only if genuinely far off-track (>24 m)
	if (BestSq > FMath::Square(2400.f))
	{
		return Line.GetDistanceAlongSplineAtLocation(Location, ESplineCoordinateSpace::World);
	}
	return WrapDistance(Line, BestD);
}

float ABNRivalController::ComputeWallAvoidance(const ABNVehiclePawn& Car, float SpeedCms, bool& bOutBlockedAhead) const
{
	bOutBlockedAhead = false;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const FVector Fwd = Car.GetActorForwardVector().GetSafeNormal();
	const FVector Right = Car.GetActorRightVector().GetSafeNormal();
	const FVector Origin = Car.GetActorLocation() + Car.GetActorUpVector() * 68.f + Fwd * 215.f;
	// Short-range whiskers only (4.5..7.0 m) following 3D road pitch so ramps & curved barriers NEVER false-brake the car
	const float SideReach = FMath::Clamp(460.f + FMath::Abs(SpeedCms) * 0.08f, 460.f, 700.f);
	const float FrontReach = 340.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNRivalWallAvoid), false);
	Params.AddIgnoredActor(&Car);

	auto Probe = [&](float AngleDeg, float Dist) -> float
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Dir = (Fwd * FMath::Cos(Rad) + Right * FMath::Sin(Rad)).GetSafeNormal();
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * Dist, ECC_WorldStatic, Params))
		{
			if (Hit.ImpactNormal.Z < 0.55f)
			{
				return Hit.Time;
			}
		}
		return 1.f;
	};

	const float LeftHit = Probe(-30.f, SideReach);
	const float RightHit = Probe(30.f, SideReach);
	const float CenterHit = Probe(0.f, FrontReach);
	bOutBlockedAhead = (CenterHit < 0.92f);
	if (LeftHit >= 0.99f && RightHit >= 0.99f)
	{
		return 0.f;
	}
	return FMath::Clamp((RightHit - LeftHit) * 0.90f, -0.55f, 0.55f);
}

void ABNRivalController::Drive(ABNVehiclePawn& Car, const USplineComponent& Line, float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	const FVector Location = Car.GetActorLocation();
	const float Speed = Move->GetForwardSpeed();
	const float Distance = FindTrackedDistance(Line, Location);
	TrackedDistance = Distance;
	ReverseCooldownSeconds = FMath::Max(0.f, ReverseCooldownSeconds - DeltaSeconds);

	// Detect both zero-speed jams AND slow wall-grinding / off-track loops where spline progress stalls!
	if (LastProgressDistance < 0.f)
	{
		LastProgressDistance = Distance;
	}
	float DeltaProgress = Distance - LastProgressDistance;
	const float SplineLen = Line.GetSplineLength();
	if (Line.IsClosedLoop() && SplineLen > 100.f && DeltaProgress < -SplineLen * 0.5f)
	{
		DeltaProgress += SplineLen;
	}
	if (DeltaProgress >= 260.f)
	{
		LastProgressDistance = Distance;
		ProgressStuckSeconds = 0.f;
	}
	else
	{
		ProgressStuckSeconds += DeltaSeconds;
	}

	const bool bFlipped = Car.GetActorUpVector().Z < UpsideDownZ;
	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || bFlipped;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;

	// If wedged nose-first against a wall (upright), perform a brief reverse & counter-steer recovery instead of teleporting!
	if (ReverseUnstuckSeconds > 0.f)
	{
		ReverseUnstuckSeconds = FMath::Max(0.f, ReverseUnstuckSeconds - DeltaSeconds);
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(1.f);
		Move->SetSteeringInput(ReverseUnstuckSteer);
		Move->SetHandbrakeInput(false);
		if (StuckSeconds >= StuckSecondsToRespawn || bFlipped)
		{
			ReverseUnstuckSeconds = 0.f;
			Respawn(Car, Line, Distance);
		}
		return;
	}

	const bool bRespawnFlip = bFlipped && StuckSeconds >= 1.6f;
	if (bRespawnFlip || StuckSeconds >= StuckSecondsToRespawn || ProgressStuckSeconds >= ProgressStuckSecondsToRespawn)
	{
		Respawn(Car, Line, Distance);
		return;
	}

	const float CatchUp = CatchUpScale(GapToPlayer(Line, Distance), Skill);
	const float GripG = FMath::Lerp(GripRange.X, GripRange.Y, Skill) * FMath::Max(CatchUp, 1.f);
	const float BrakeG = FMath::Lerp(BrakeRange.X, BrakeRange.Y, Skill);
	float Safe = SafeSpeed(Line, Distance, GripG, BrakeG * (Style == EBNRivalStyle::LateBrake ? 1.06f : 1.f));

	// Center toward the apex in tight corners so rivals never clip inside/outside barriers.
	const float CornerCenterScale = FMath::GetMappedRangeValueClamped(FVector2f(1500.f, 3800.f), FVector2f(0.10f, 1.f), Safe);
	const float EffectiveLane = FMath::Clamp(LaneOffset, -280.f, 280.f) * CornerCenterScale;

	// Steering: pure pursuit with adaptive lookahead tightened on slow corners + full-lock recovery if facing off-line.
	const float CornerLookCap = FMath::Clamp(Safe * 0.52f, MinSteerLookAhead, MaxSteerLookAhead);
	const float AimDistance = WrapDistance(Line, Distance + FMath::Clamp(FMath::Abs(Speed) * SteerLookAheadSeconds, MinSteerLookAhead, CornerLookCap));
	const FVector Aim = Line.GetLocationAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World)
		+ Line.GetRightVectorAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World) * EffectiveLane;
	const FVector Local = Car.GetActorTransform().InverseTransformPosition(Aim);

	bool bWallBlockedAhead = false;
	const float WallAvoidSteer = ComputeWallAvoidance(Car, Speed, bWallBlockedAhead);

	// Enter brief reverse-unstuck only after launch and when genuinely jammed against a wall (not queueing behind cars)
	if (!bFlipped && SecondsUnlocked > 2.5f && ReverseCooldownSeconds <= 0.f && StuckSeconds >= ReverseTriggerSeconds
		&& (bWallBlockedAhead || Local.X < 140.f) && LaneSpeed(Car, 0.f, 0.f) > 500.f)
	{
		ReverseUnstuckSeconds = ReverseDurationSeconds;
		ReverseCooldownSeconds = 3.5f;
		ReverseUnstuckSteer = Local.Y >= 0.f ? -0.9f : 0.9f;
		return;
	}

	float SteerInput = 0.f;
	if (Local.X < 120.f)
	{
		SteerInput = Local.Y >= 0.f ? 1.f : -1.f;
	}
	else
	{
		const USkeletalMeshComponent* Mesh = Car.GetMesh();
		const float Wheelbase = FVector::Dist(Mesh->GetSocketLocation(TEXT("Phys_Wheel_FL")), Mesh->GetSocketLocation(TEXT("Phys_Wheel_BL")));
		const FVector OnLine = Line.GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
			+ Line.GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) * EffectiveLane;
		const float CrossTrack = Car.GetActorTransform().InverseTransformPosition(OnLine).Y;
		const float WheelAngle = FMath::RadiansToDegrees(FMath::Atan(Wheelbase * 2.f * Local.Y / FMath::Max(Local.SizeSquared2D(), 1.f))
			+ FMath::Atan2(CrossTrackGain * CrossTrack, FMath::Max(FMath::Abs(Speed), MinCrossTrackSpeed)));
		const FRichCurve* SteerCurve = Move->SteeringSetup.SteeringCurve.GetRichCurveConst();
		const float LockScale = SteerCurve && SteerCurve->GetNumKeys() > 0 ? SteerCurve->Eval(FMath::Abs(Speed) * CmPerSecToMph) : 1.f;
		const float MaxWheelAngle = FMath::Max(Move->Wheels[0]->MaxSteerAngle * LockScale, 1.f);
		SteerInput = FMath::Clamp(WheelAngle / MaxWheelAngle + WallAvoidSteer, -1.f, 1.f);
	}
	Move->SetSteeringInput(SteerInput);

	// Cap entry speed only on genuinely steep heading errors (>32 deg) or immediate bumper wall block
	const float SignedHeadingErrDeg = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, FMath::Max(Local.X, 100.f)));
	const float HeadingErrDeg = FMath::Abs(SignedHeadingErrDeg);
	if (HeadingErrDeg > 32.f)
	{
		Safe = FMath::Min(Safe, FMath::GetMappedRangeValueClamped(FVector2f(32.f, 75.f), FVector2f(3200.f, 1300.f), HeadingErrDeg));
	}
	if (bWallBlockedAhead)
	{
		Safe = FMath::Min(Safe, 1000.f);
	}

	// Pedals: chase the safe speed, never faster than a car/traffic close ahead in the same lane.
	const float Ahead = LaneSpeed(Car, 0.f, 0.f);
	LaneChangeCooldown = FMath::Max(LaneChangeCooldown - DeltaSeconds, 0.f);
	const float PassMargin = OvertakeMargin * (Style == EBNRivalStyle::DefendLine ? 1.55f : 1.f);
	if (Ahead < Safe - PassMargin && (Safe >= MinLaneChangeSafeSpeed || Ahead < 600.f) && LaneChangeCooldown <= 0.f
		&& FMath::IsNearlyEqual(LaneOffset, TargetLaneOffset, 15.f))
	{
		const float CandA = FMath::IsNearlyZero(TargetLaneOffset, 30.f) ? 260.f : -TargetLaneOffset;
		const float CandB = -CandA;
		if (LaneSpeed(Car, CandA - LaneOffset, -BesideDistance) >= FMath::Min(MaxSpeed, Ahead + 600.f))
		{
			TargetLaneOffset = CandA;
			LaneChangeCooldown = LaneChangeSeconds;
		}
		else if (LaneSpeed(Car, CandB - LaneOffset, -BesideDistance) >= FMath::Min(MaxSpeed, Ahead + 600.f))
		{
			TargetLaneOffset = CandB;
			LaneChangeCooldown = LaneChangeSeconds;
		}
	}
	LaneOffset = FMath::FInterpConstantTo(LaneOffset, TargetLaneOffset, DeltaSeconds, LaneShiftSpeed);
	const float Target = FMath::Min(Safe * FMath::Min(CatchUp, 1.f), Ahead);
	if (CatchUp > 1.f && Car.IsOnGround() && HeadingErrDeg < 22.f)
	{
		Car.GetMesh()->AddForce(Car.GetActorForwardVector() * (CatchUp - 1.f) / 0.16f * CatchUpAccelCms2, NAME_None, true);
	}
	const float Error = Target - Speed;
	const float ThrottleError = FullThrottleError * (Style == EBNRivalStyle::FastExit ? 0.72f : 1.f);
	Move->SetThrottleInput(FMath::Clamp(Error / ThrottleError, 0.f, 1.f));
	Move->SetBrakeInput(Speed > MinBrakingSpeed ? FMath::Clamp(-Error / FullBrakeError, 0.f, 1.f) : 0.f);
	Move->SetHandbrakeInput(false);
	ApplyGripAssist(Car, DeltaSeconds, SignedHeadingErrDeg);
}

void ABNRivalController::ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds, float HeadingErrorDeg) const
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Speed2D = Velocity.Size2D();
	if (!Car.IsOnGround() || Speed2D < MinGripAssistSpeed)
	{
		return;
	}
	// Critically-damped yaw guidance on low/mid-speed city corners; fades smoothly to 0 above 120 km/h for rock-solid oval/straight stability
	const float HighSpeedFade = FMath::GetMappedRangeValueClamped(FVector2f(2400.f, 3600.f), FVector2f(1.f, 0.f), Speed2D);
	if (FMath::Abs(HeadingErrorDeg) > 10.f && HighSpeedFade > 0.01f)
	{
		FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();
		const float LowSpeedRamp = FMath::GetMappedRangeValueClamped(FVector2f(420.f, 1500.f), FVector2f(0.45f, 1.f), Speed2D);
		const float DesiredYawRate = FMath::Clamp(HeadingErrorDeg * YawAssistGain, -YawAssistMaxDegPerSec, YawAssistMaxDegPerSec)
			* LowSpeedRamp * HighSpeedFade;
		AngVel.Z = FMath::FInterpTo(AngVel.Z, DesiredYawRate, DeltaSeconds, 4.2f);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);
	}
	const float Slip = Car.GetSlipAngleDegrees(); // travel minus heading
	const float Turn = -FMath::Clamp(Slip, -GripAssistDegreesPerSecond * DeltaSeconds, GripAssistDegreesPerSecond * DeltaSeconds);
	const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
}

float ABNRivalController::LaneSpeed(const ABNVehiclePawn& Car, float LateralShift, float FromX) const
{
	float Slowest = MaxSpeed;
	const FTransform& Frame = Car.GetActorTransform();
	for (TActorIterator<ABNVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (*It == &Car)
		{
			continue;
		}
		const FVector Rel = Frame.InverseTransformPosition(It->GetActorLocation());
		if (Rel.X > FromX && Rel.X < FollowDistance && FMath::Abs(Rel.Y - LateralShift) < LaneHalfWidth && FMath::Abs(Rel.Z) < 240.f)
		{
			Slowest = FMath::Min(Slowest, FMath::Max(FVector::DotProduct(It->GetVelocity(), Frame.GetUnitAxis(EAxis::X)), 0.f));
		}
	}
	// Also perceive civilian traffic so rivals overtake or brake instead of ramming traffic cars!
	if (const UBNTrafficSubsystem* Traffic = GetWorld() ? GetWorld()->GetSubsystem<UBNTrafficSubsystem>() : nullptr)
	{
		TArray<FVector> TrafficLocs;
		Traffic->GetTrafficLocations(TrafficLocs);
		for (const FVector& TLoc : TrafficLocs)
		{
			const FVector Rel = Frame.InverseTransformPosition(TLoc);
			if (Rel.X > FromX && Rel.X < FollowDistance && FMath::Abs(Rel.Y - LateralShift) < LaneHalfWidth && FMath::Abs(Rel.Z) < 220.f)
			{
				const float ApproxTrafficSpeed = Rel.X < 950.f ? 0.f : 1100.f;
				Slowest = FMath::Min(Slowest, ApproxTrafficSpeed);
			}
		}
	}
	return Slowest;
}

void ABNRivalController::Respawn(ABNVehiclePawn& Car, const USplineComponent& Line, float Distance)
{
	// Always place the recovered bot dead-center on the clean road ahead so it never re-snags the same curb/wall.
	LaneOffset = 0.f;
	TargetLaneOffset = 0.f;
	const float ForwardDist = WrapDistance(Line, Distance + 700.f);
	const FVector Location = Line.GetLocationAtDistanceAlongSpline(ForwardDist, ESplineCoordinateSpace::World) + FVector(0.f, 0.f, 75.f);
	const FVector Dir = Line.GetDirectionAtDistanceAlongSpline(ForwardDist, ESplineCoordinateSpace::World).GetSafeNormal();
	const FRotator Heading(FMath::Clamp(Dir.Rotation().Pitch, -12.f, 12.f), Dir.Rotation().Yaw, 0.f);
	Car.SetActorLocationAndRotation(Location, Heading, false, nullptr, ETeleportType::TeleportPhysics);
	Car.GetMesh()->SetPhysicsLinearVelocity(Dir * 650.f);
	Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	StuckSeconds = 0.f;
	ProgressStuckSeconds = 0.f;
	ReverseUnstuckSeconds = 0.f;
	TrackedDistance = ForwardDist;
	LastProgressDistance = ForwardDist;
	++Respawns;
	UE_LOG(LogTemp, Display, TEXT("BN rival %s put back on the racing line (%d)"), *Car.GetName(), Respawns);
}
