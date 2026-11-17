#include "Race/BNRivalController.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SplineComponent.h"
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
	constexpr float StuckSecondsToRespawn = 4.f;
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

void ABNRivalController::Setup(ABNRaceEvent* InRace, float InSkill, float InLaneOffset)
{
	Race = InRace;
	Skill = FMath::Clamp(InSkill, 0.f, 1.f);
	LaneOffset = InLaneOffset;
	TargetLaneOffset = InLaneOffset;
	LaunchDelay = FMath::Lerp(LaunchDelayRange.X, LaunchDelayRange.Y, Skill);
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

	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || Car.GetActorUpVector().Z < UpsideDownZ;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;
	if (StuckSeconds >= StuckSecondsToRespawn)
	{
		Respawn(Car, Line, Distance);
		return;
	}

	// Steering: pure pursuit. Wheel angle for the arc through the aim point, as a fraction of the lock the
	// car's speed-sensitive steering allows right now.
	const float AimDistance = WrapDistance(Line, Distance + FMath::Clamp(FMath::Abs(Speed) * SteerLookAheadSeconds, MinSteerLookAhead, MaxSteerLookAhead));
	const FVector Aim = Line.GetLocationAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World)
		+ Line.GetRightVectorAtDistanceAlongSpline(AimDistance, ESplineCoordinateSpace::World) * LaneOffset;
	const FVector Local = Car.GetActorTransform().InverseTransformPosition(Aim);
	const USkeletalMeshComponent* Mesh = Car.GetMesh();
	const float Wheelbase = FVector::Dist(Mesh->GetSocketLocation(TEXT("Phys_Wheel_FL")), Mesh->GetSocketLocation(TEXT("Phys_Wheel_BL")));
	// Tyres slip, so the arc alone runs wide in long corners: add a term for the sideways error to the lane.
	const FVector OnLine = Line.GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
		+ Line.GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) * LaneOffset;
	const float CrossTrack = Car.GetActorTransform().InverseTransformPosition(OnLine).Y;
	const float WheelAngle = FMath::RadiansToDegrees(FMath::Atan(Wheelbase * 2.f * Local.Y / FMath::Max(Local.SizeSquared2D(), 1.f))
		+ FMath::Atan2(CrossTrackGain * CrossTrack, FMath::Max(FMath::Abs(Speed), MinCrossTrackSpeed)));
	const FRichCurve* SteerCurve = Move->SteeringSetup.SteeringCurve.GetRichCurveConst();
	const float LockScale = SteerCurve && SteerCurve->GetNumKeys() > 0 ? SteerCurve->Eval(FMath::Abs(Speed) * CmPerSecToMph) : 1.f;
	const float MaxWheelAngle = FMath::Max(Move->Wheels[0]->MaxSteerAngle * LockScale, 1.f);
	Move->SetSteeringInput(FMath::Clamp(WheelAngle / MaxWheelAngle, -1.f, 1.f));

	// Pedals: chase the safe speed, never faster than a car close ahead in the same lane. Held up by a
	// slower car with the other lane clear (beside and ahead), move over to pass.
	const float GripG = FMath::Lerp(GripRange.X, GripRange.Y, Skill);
	const float BrakeG = FMath::Lerp(BrakeRange.X, BrakeRange.Y, Skill);
	const float Safe = SafeSpeed(Line, Distance, GripG, BrakeG);
	const float Ahead = LaneSpeed(Car, 0.f, 0.f);
	LaneChangeCooldown = FMath::Max(LaneChangeCooldown - DeltaSeconds, 0.f);
	if (Ahead < Safe - OvertakeMargin && Safe >= MinLaneChangeSafeSpeed && LaneChangeCooldown <= 0.f && LaneOffset == TargetLaneOffset
		&& LaneSpeed(Car, -2.f * LaneOffset, -BesideDistance) >= MaxSpeed)
	{
		TargetLaneOffset = -TargetLaneOffset;
		LaneChangeCooldown = LaneChangeSeconds;
	}
	LaneOffset = FMath::FInterpConstantTo(LaneOffset, TargetLaneOffset, DeltaSeconds, LaneShiftSpeed);
	const float Target = FMath::Min(Safe, Ahead);
	const float Error = Target - Speed;
	Move->SetThrottleInput(FMath::Clamp(Error / FullThrottleError, 0.f, 1.f));
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
	const FVector Location = Line.GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
		+ Line.GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) * LaneOffset + FVector(0.f, 0.f, 100.f);
	const float Yaw = Line.GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).Rotation().Yaw;
	Car.SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	StuckSeconds = 0.f;
	++Respawns;
	UE_LOG(LogTemp, Display, TEXT("BN rival %s put back on the racing line (%d)"), *Car.GetName(), Respawns);
}
