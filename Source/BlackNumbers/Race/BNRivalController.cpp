#include "Race/BNRivalController.h"
#include "Story/BNStoryTypes.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float Gravity = 980.f;
	constexpr float MaxSpeed = 7000.f; // cm/s, ~250 km/h: nothing on the line limits a straight
	constexpr float SampleStep = 500.f;
	constexpr float LookAheadHorizon = 20000.f;
	/** Pure-pursuit aim point: SteerLookAheadSeconds of travel ahead, clamped. */
	constexpr float SteerLookAheadSeconds = 0.6f;
	constexpr float MinSteerLookAhead = 700.f;
	constexpr float MaxSteerLookAhead = 2500.f;
	/** Speed error (cm/s) that gives full throttle / full brake. */
	constexpr float FullThrottleError = 280.f;
	constexpr float FullBrakeError = 560.f;
	/** Below this the brake pedal would select reverse (reverse-as-brake), so it is not used. */
	constexpr float MinBrakingSpeed = 200.f;
	/** A car this close ahead and this close sideways is "in my lane". */
	constexpr float FollowDistance = 2500.f;
	constexpr float LaneHalfWidth = 300.f;
	/** Pass when the car ahead holds us this much (cm/s) under our safe speed. */
	constexpr float OvertakeMargin = 150.f;
	/** The other lane must be clear from this far behind our nose. */
	constexpr float BesideDistance = 700.f;
	constexpr float LaneChangeSeconds = 3.f;
	/** Sideways speed of a lane change along the line (cm/s): a drift across, not a swerve. */
	constexpr float LaneShiftSpeed = 250.f;
	/** Only change lanes where the line allows this speed (cm/s, ~110 km/h), i.e. not into or through a corner. */
	constexpr float MinLaneChangeSafeSpeed = 3000.f;
	constexpr float StuckSpeed = 300.f;
	constexpr float UpsideDownZ = 0.3f;
	constexpr float StuckSecondsToRespawn = 2.8f;
	constexpr float ProgressStuckSecondsToRespawn = 3.2f;
	constexpr float CmPerSecToMph = 0.022369f;
	/** Stanley-style correction: steer back toward the line at this rate (1/s) of the sideways error. */
	constexpr float CrossTrackGain = 1.f;
	constexpr float MinCrossTrackSpeed = 500.f;

	// Skill 0 -> 1.
	/**
	 * Cornering the driver plans for, in g. The tyres alone hold ~0.7 g; the arcade grip assist below carries
	 * the rest, as the drift assist does for the player (a drifting player corners at well over 1 g).
	 */
	const FVector2f GripRange(0.95f, 1.45f);
	/** The car brakes at ~1.35 g with ABS. */
	const FVector2f BrakeRange(0.9f, 1.2f);
	/** Arcade grip: travel direction swings toward the nose at up to this rate (deg/s), so rivals go where they point. */
	constexpr float GripAssistDegreesPerSecond = 120.f;
	constexpr float MinGripAssistSpeed = 500.f;
	const FVector2f LaunchDelayRange(0.7f, 0.1f);

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
	LaneOffset = InLaneOffset;
	TargetLaneOffset = InLaneOffset;
	LaunchDelay = FMath::Lerp(LaunchDelayRange.X, LaunchDelayRange.Y, Skill);
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

void ABNRivalController::Drive(ABNVehiclePawn& Car, const USplineComponent& Line, float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	const FVector Location = Car.GetActorLocation();
	const float Speed = Move->GetForwardSpeed();
	const float Distance = Line.GetDistanceAlongSplineAtLocation(Location, ESplineCoordinateSpace::World);

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

	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || Car.GetActorUpVector().Z < UpsideDownZ;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;
	if (StuckSeconds >= StuckSecondsToRespawn || ProgressStuckSeconds >= ProgressStuckSecondsToRespawn)
	{
		Respawn(Car, Line, Distance);
		return;
	}

	const float CatchUp = CatchUpScale(GapToPlayer(Line, Distance), Skill);
	const float GripG = FMath::Lerp(GripRange.X, GripRange.Y, Skill) * FMath::Max(CatchUp, 1.f);
	const float BrakeG = FMath::Lerp(BrakeRange.X, BrakeRange.Y, Skill);
	const float Safe = SafeSpeed(Line, Distance, GripG, BrakeG * (Style == EBNRivalStyle::LateBrake ? 1.08f : 1.f));

	// Center toward the apex in tight corners so rivals never clip inside/outside barriers at ±300..400 cm offset.
	const float CornerCenterScale = FMath::GetMappedRangeValueClamped(FVector2f(1400.f, 3400.f), FVector2f(0.15f, 1.f), Safe);
	const float EffectiveLane = LaneOffset * CornerCenterScale;

	// Steering: pure pursuit with adaptive lookahead tightened on slow corners + full-lock recovery if facing off-line.
	const float CornerLookCap = FMath::Clamp(Safe * 0.55f, MinSteerLookAhead, MaxSteerLookAhead);
	const float AimDistance = WrapDistance(Line, Distance + FMath::Clamp(FMath::Abs(Speed) * SteerLookAheadSeconds, MinSteerLookAhead, CornerLookCap));
	const FVector Aim = Line.GetLocationAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World)
		+ Line.GetRightVectorAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World) * EffectiveLane;
	const FVector Local = Car.GetActorTransform().InverseTransformPosition(Aim);
	if (Local.X < 120.f)
	{
		// Facing nearly perpendicular or backwards relative to the spline: steer full lock back toward the track!
		Move->SetSteeringInput(Local.Y >= 0.f ? 1.f : -1.f);
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
		Move->SetSteeringInput(FMath::Clamp(WheelAngle / MaxWheelAngle, -1.f, 1.f));
	}

	// Pedals: chase the safe speed, never faster than a car close ahead in the same lane. Held up by a
	// slower car with the other lane clear (beside and ahead), move over to pass.
	const float Ahead = LaneSpeed(Car, 0.f, 0.f);
	LaneChangeCooldown = FMath::Max(LaneChangeCooldown - DeltaSeconds, 0.f);
	const float PassMargin = OvertakeMargin * (Style == EBNRivalStyle::DefendLine ? 1.55f : 1.f);
	if (Ahead < Safe - PassMargin && Safe >= MinLaneChangeSafeSpeed && LaneChangeCooldown <= 0.f && LaneOffset == TargetLaneOffset
		&& LaneSpeed(Car, -2.f * LaneOffset, -BesideDistance) >= MaxSpeed)
	{
		TargetLaneOffset = -TargetLaneOffset;
		LaneChangeCooldown = LaneChangeSeconds;
	}
	LaneOffset = FMath::FInterpConstantTo(LaneOffset, TargetLaneOffset, DeltaSeconds, LaneShiftSpeed);
	const float Target = FMath::Min(Safe * FMath::Min(CatchUp, 1.f), Ahead);
	if (CatchUp > 1.f && Car.IsOnGround())
	{
		Car.GetMesh()->AddForce(Car.GetActorForwardVector() * (CatchUp - 1.f) / 0.16f * CatchUpAccelCms2, NAME_None, true);
	}
	const float Error = Target - Speed;
	const float ThrottleError = FullThrottleError * (Style == EBNRivalStyle::FastExit ? 0.72f : 1.f);
	Move->SetThrottleInput(FMath::Clamp(Error / ThrottleError, 0.f, 1.f));
	Move->SetBrakeInput(Speed > MinBrakingSpeed ? FMath::Clamp(-Error / FullBrakeError, 0.f, 1.f) : 0.f);
	Move->SetHandbrakeInput(false);
	ApplyGripAssist(Car, DeltaSeconds);
}

void ABNRivalController::ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds) const
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (!Car.IsOnGround() || Velocity.Size2D() < MinGripAssistSpeed)
	{
		return;
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
		if (Rel.X > FromX && Rel.X < FollowDistance && FMath::Abs(Rel.Y - LateralShift) < LaneHalfWidth)
		{
			Slowest = FMath::Min(Slowest, FMath::Max(FVector::DotProduct(It->GetVelocity(), Frame.GetUnitAxis(EAxis::X)), 0.f));
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
	LastProgressDistance = ForwardDist;
	++Respawns;
	UE_LOG(LogTemp, Display, TEXT("BN rival %s put back on the racing line (%d)"), *Car.GetName(), Respawns);
}
