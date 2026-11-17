#pragma once

#include "CoreMinimal.h"

/**
 * Drift points: angle x speed x time while sliding, multiplied by a combo that grows the longer a drift is
 * held. A drift banks when the car stops sliding for a moment; hitting something or spinning out loses it.
 * No actors, so it is unit tested directly.
 */
struct BLACKNUMBERS_API FBNDriftScorer
{
	void Update(float DeltaSeconds, float SlipDegrees, float SpeedKmh);

	/** Wall or car contact: the running (unbanked) drift is lost. */
	void OnImpact();

	/** Race over: a drift still running is banked. */
	void Finish();

	void Reset() { *this = FBNDriftScorer(); }

	int32 Total() const { return TotalPoints; }
	float Current() const { return CurrentPoints; }
	int32 Multiplier() const;
	bool IsDrifting() const { return bInDrift; }

private:
	void Bank();
	void Lose();

	int32 TotalPoints = 0;
	float CurrentPoints = 0.f;
	float DriftSeconds = 0.f;
	float CalmSeconds = 0.f;
	bool bInDrift = false;
};
