#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "BNRivalController.generated.h"

class ABNRaceEvent;
class ABNVehiclePawn;
class USplineComponent;

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

	void Setup(ABNRaceEvent* InRace, float InSkill, float InLaneOffset);

	/** Times the car was put back on the line after getting stuck or flipped. */
	int32 GetRespawnCount() const { return Respawns; }

	/**
	 * Fastest speed (cm/s) at Distance along Line from which every corner in the look-ahead can still be
	 * made at GripG lateral grip, braking at BrakeG. An open line's end counts as a stop.
	 */
	static float SafeSpeed(const USplineComponent& Line, float Distance, float GripG, float BrakeG);

private:
	void Drive(ABNVehiclePawn& Car, const USplineComponent& Line, float DeltaSeconds);
	void Respawn(ABNVehiclePawn& Car, const USplineComponent& Line, float Distance);
	/**
	 * Slowest forward speed of cars in a box from FromX to the follow distance ahead, centred LateralShift
	 * to the side (car frame); the max speed if the box is clear.
	 */
	float LaneSpeed(const ABNVehiclePawn& Car, float LateralShift, float FromX) const;

	TWeakObjectPtr<ABNRaceEvent> Race;
	float Skill = 0.5f;
	/** Sideways offset from the racing line (right positive), so rivals do not all queue on one line. */
	float LaneOffset = 0.f;
	/** Lane being moved to while passing; LaneOffset slides toward it. */
	float TargetLaneOffset = 0.f;
	float LaunchDelay = 0.f;
	float SecondsUnlocked = 0.f;
	float StuckSeconds = 0.f;
	float LaneChangeCooldown = 0.f;
	int32 Respawns = 0;
};
