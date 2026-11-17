#include "Race/BNDriftScorer.h"

namespace
{
	constexpr float MinSlipDegrees = 12.f;
	constexpr float MaxScoredSlipDegrees = 60.f;
	/** Beyond this the car is spinning, not drifting. */
	constexpr float SpinSlipDegrees = 100.f;
	constexpr float MinSpeedKmh = 30.f;
	/** A drift ends (and banks) after this long without sliding, so linked drifts keep their combo. */
	constexpr float BankAfterCalmSeconds = 1.f;
	constexpr float SecondsPerMultiplierStep = 2.f;
	constexpr int32 MaxMultiplier = 5;
	/** Points per second at full scored angle and 100 km/h. */
	constexpr float PointsPerSecond = 500.f;
}

void FBNDriftScorer::Update(float DeltaSeconds, float SlipDegrees, float SpeedKmh)
{
	const float Slip = FMath::Abs(SlipDegrees);
	const float Speed = FMath::Abs(SpeedKmh);
	if (bInDrift && Slip > SpinSlipDegrees)
	{
		Lose();
		return;
	}

	if (Slip >= MinSlipDegrees && Slip <= SpinSlipDegrees && Speed >= MinSpeedKmh)
	{
		bInDrift = true;
		CalmSeconds = 0.f;
		DriftSeconds += DeltaSeconds;
		const float Angle = FMath::Min(Slip, MaxScoredSlipDegrees) / MaxScoredSlipDegrees;
		CurrentPoints += PointsPerSecond * Angle * (Speed / 100.f) * DeltaSeconds;
		return;
	}

	if (bInDrift)
	{
		CalmSeconds += DeltaSeconds;
		if (CalmSeconds >= BankAfterCalmSeconds)
		{
			Bank();
		}
	}
}

int32 FBNDriftScorer::Multiplier() const
{
	return FMath::Min(1 + FMath::FloorToInt(DriftSeconds / SecondsPerMultiplierStep), MaxMultiplier);
}

void FBNDriftScorer::OnImpact()
{
	if (bInDrift)
	{
		Lose();
	}
}

void FBNDriftScorer::Finish()
{
	if (bInDrift)
	{
		Bank();
	}
}

void FBNDriftScorer::Bank()
{
	TotalPoints += FMath::RoundToInt(CurrentPoints * Multiplier());
	Lose();
}

void FBNDriftScorer::Lose()
{
	CurrentPoints = 0.f;
	DriftSeconds = 0.f;
	CalmSeconds = 0.f;
	bInDrift = false;
}
