#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "BNRivalController.generated.h"

class ABNRaceEvent;
class ABNVehiclePawn;
class USplineComponent;
enum class EBNRivalStyle : uint8;

/**
 * Drives a rival car along its race's racing line: pure-pursuit steering toward a point ahead, and the
 * fastest speed that still makes every corner in braking distance. Skill scales the grip and braking it
 * trusts and how quickly it launches. A Chaos vehicle only takes inputs from a local controller, hence
 * a controller rather than a component.
 */
UCLASS()
class BLACKNUMBERS_API ABNRivalController : public AController
{
	GENERATED_BODY()

public:
	ABNRivalController();

	virtual void Tick(float DeltaSeconds) override;

	void Setup(ABNRaceEvent* InRace, float InSkill, float InLaneOffset, EBNRivalStyle InStyle);

	/** Times the car was put back on the line after getting stuck or flipped. */
	int32 GetRespawnCount() const { return Respawns; }

	/**
	 * Fastest speed (cm/s) at Distance along Line from which every corner in the look-ahead can still be
	 * made at GripG lateral grip, braking at BrakeG. An open line's end counts as a stop.
	 */
	static float SafeSpeed(const USplineComponent& Line, float Distance, float GripG, float BrakeG);
	/**
	 * Arcade rubber band: >1 when the rival trails the player (GapCm < 0), <1 when it runs far ahead.
	 * Skilled rivals ease off less. Scales planned grip/target speed.
	 */
	static float CatchUpScale(float GapCm, float Skill);

private:
	void Drive(ABNVehiclePawn& Car, const USplineComponent& Line, float DeltaSeconds);
	/** Local sliding-window projection along Line so city loops never snap onto adjacent parallel streets. */
	float FindTrackedDistance(const USplineComponent& Line, const FVector& Location) const;
	/** Signed gap to the player along the racing line (cm, + = rival ahead). 0 when unknown. */
	float GapToPlayer(const USplineComponent& Line, float MyDistance) const;
	/** Removes sideways slide (understeer) and gently guides chassis yaw toward HeadingErrorDeg on ground. */
	void ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds, float HeadingErrorDeg) const;
	/** Short left/right forward whisker traces against static walls returning steering bias (-1..+1) and proximity brake flag. */
	float ComputeWallAvoidance(const ABNVehiclePawn& Car, float SpeedCms, bool& bOutBlockedAhead) const;
	void Respawn(ABNVehiclePawn& Car, const USplineComponent& Line, float Distance);
	/**
	 * Slowest forward speed of cars (rivals, player, civilian traffic) in a box from FromX to the follow
	 * distance ahead, centred LateralShift to the side (car frame); the max speed if the box is clear.
	 */
	float LaneSpeed(const ABNVehiclePawn& Car, float LateralShift, float FromX) const;

	TWeakObjectPtr<ABNRaceEvent> Race;
	float Skill = 0.5f;
	EBNRivalStyle Style{};
	/** Sideways offset from the racing line (right positive), so rivals do not all queue on one line. */
	float LaneOffset = 0.f;
	/** Lane being moved to while passing; LaneOffset slides toward it. */
	float TargetLaneOffset = 0.f;
	float LaunchDelay = 0.f;
	float SecondsUnlocked = 0.f;
	float StuckSeconds = 0.f;
	float ProgressStuckSeconds = 0.f;
	float TrackedDistance = -1.f;
	float LastProgressDistance = -1.f;
	float ReverseUnstuckSeconds = 0.f;
	float ReverseUnstuckSteer = 0.f;
	float ReverseCooldownSeconds = 0.f;
	float LaneChangeCooldown = 0.f;
	int32 Respawns = 0;
};
