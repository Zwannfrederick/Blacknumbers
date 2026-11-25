#pragma once

#include "CoreMinimal.h"

/**
 * Pure police tactic decisions (no actors) so they can be unit tested.
 * PIT: a unit tucked behind the suspect's rear quarter steers into it and, on contact,
 * spins the suspect's tail out.
 */
namespace BNPoliceTactics
{
	/** Minimum Heat aggression before units try a PIT (Heat 2+). */
	constexpr float PitMinAggression = 0.45f;
	constexpr float PitMaxDistanceCm = 900.f;
	constexpr float PitMinTargetSpeedKmh = 50.f;
	constexpr float PitMaxTargetSpeedKmh = 190.f;
	/** Suspect-local aim point on the rear quarter (cm): behind the rear axle, pushed into the body. */
	constexpr float PitAimBehindCm = -170.f;
	constexpr float PitAimInsideCm = 70.f;
	constexpr float PitAttemptSeconds = 2.2f;
	constexpr float PitCooldownSeconds = 9.f;
	/** Yaw rate (deg/s) kicked into the suspect on a successful PIT, scaled by speed. */
	constexpr float PitSpinYawRateDeg = 140.f;

	/**
	 * @param LocalPos unit position in the suspect's frame (X forward, Y right), cm.
	 * True when the unit sits beside/behind the rear quarter, close enough to hook it.
	 */
	BLACKNUMBERS_API bool CanAttemptPit(float Aggression, const FVector& LocalPos, float TargetSpeedKmh);

	/** Suspect-local point to steer at while attempting a PIT from the given side (+1 right, -1 left). */
	BLACKNUMBERS_API FVector PitAimLocal(float SideSign);

	/** Yaw rate to add to the suspect: rear swings away from the unit; faster suspects spin harder (capped). */
	BLACKNUMBERS_API float PitSpinYawRate(float SideSign, float TargetSpeedKmh);
}
