#include "Police/BNPoliceTactics.h"

namespace BNPoliceTactics
{
	bool CanAttemptPit(float Aggression, const FVector& LocalPos, float TargetSpeedKmh)
	{
		const float Speed = FMath::Abs(TargetSpeedKmh);
		const float Side = FMath::Abs(LocalPos.Y);
		return Aggression >= PitMinAggression
			&& Speed >= PitMinTargetSpeedKmh && Speed <= PitMaxTargetSpeedKmh
			&& LocalPos.Size2D() <= PitMaxDistanceCm
			&& LocalPos.X <= 60.f && LocalPos.X >= -650.f
			&& Side >= 110.f && Side <= 460.f;
	}

	FVector PitAimLocal(float SideSign)
	{
		return FVector(PitAimBehindCm, FMath::Sign(SideSign) * PitAimInsideCm, 0.f);
	}

	float PitSpinYawRate(float SideSign, float TargetSpeedKmh)
	{
		// Unit on the suspect's right pushes the tail left -> nose swings right (positive yaw in UE).
		const float Scale = FMath::GetMappedRangeValueClamped(FVector2f(50.f, 160.f), FVector2f(0.6f, 1.f), FMath::Abs(TargetSpeedKmh));
		return FMath::Sign(SideSign) * PitSpinYawRateDeg * Scale;
	}
}
