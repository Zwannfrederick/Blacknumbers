#include "Vehicle/BNVehicleBoost.h"

namespace
{
	constexpr float BlendInterpSpeed = 10.f;
	constexpr float LaunchBannerDuration = 2.0f;
	constexpr float NearMissBannerDuration = 1.6f;
	constexpr float MinSpeedForNosActivateKmh = 5.f;
	constexpr float MinSpeedForSpeedbreakerKmh = 20.f;
}

void FBNBoostState::UpdateNos(float RealDt, bool bWanted, bool bInputLocked, bool bOnGround, float SpeedKmh, bool bDrifting, const FBNBoostTuning& Tuning)
{
	const float Dt = FMath::Max(RealDt, 0.f);
	const float AbsSpeed = FMath::Abs(SpeedKmh);
	const bool bCanStart = NosFraction >= Tuning.NosMinActivateFraction;
	const bool bCanContinue = bNosActive && NosFraction > 0.f;

	if (bWanted && !bInputLocked && bOnGround && AbsSpeed >= MinSpeedForNosActivateKmh && (bCanStart || bCanContinue))
	{
		bNosActive = true;
		const float DrainPerSec = 1.f / FMath::Max(Tuning.NosMaxSeconds, 0.1f);
		NosFraction = FMath::Max(0.f, NosFraction - DrainPerSec * Dt);
		if (NosFraction <= 0.f)
		{
			bNosActive = false;
		}
		return;
	}

	bNosActive = false;
	if (!bInputLocked && bOnGround)
	{
		float Recharge = Tuning.NosRechargePassivePerSecond;
		if (bDrifting)
		{
			Recharge += Tuning.NosRechargeDriftPerSecond;
		}
		if (AbsSpeed >= Tuning.NosRechargeMinSpeedKmh)
		{
			Recharge += Tuning.NosRechargeHighSpeedPerSecond;
		}
		NosFraction = FMath::Clamp(NosFraction + Recharge * Dt, 0.f, 1.f);
	}
}

void FBNBoostState::UpdateSpeedbreaker(float RealDt, bool bWanted, bool bInputLocked, float SpeedKmh, const FBNBoostTuning& Tuning)
{
	const float Dt = FMath::Max(RealDt, 0.f);
	const float AbsSpeed = FMath::Abs(SpeedKmh);
	const bool bCanStart = SpeedbreakerFraction >= Tuning.SpeedbreakerMinActivateFraction;
	const bool bCanContinue = bSpeedbreakerActive && SpeedbreakerFraction > 0.f;

	if (bWanted && !bInputLocked && AbsSpeed >= MinSpeedForSpeedbreakerKmh && (bCanStart || bCanContinue))
	{
		bSpeedbreakerActive = true;
		const float DrainPerSec = 1.f / FMath::Max(Tuning.SpeedbreakerMaxSeconds, 0.1f);
		SpeedbreakerFraction = FMath::Max(0.f, SpeedbreakerFraction - DrainPerSec * Dt);
		if (SpeedbreakerFraction <= 0.f)
		{
			bSpeedbreakerActive = false;
		}
		return;
	}

	bSpeedbreakerActive = false;
	if (!bInputLocked && AbsSpeed >= Tuning.SpeedbreakerMinRechargeSpeedKmh)
	{
		SpeedbreakerFraction = FMath::Clamp(SpeedbreakerFraction + Tuning.SpeedbreakerRechargePerSecond * Dt, 0.f, 1.f);
	}
}

void FBNBoostState::UpdateSlipstream(float RealDt, bool bInDraftPocket, bool bInputLocked, bool bOnGround, float SpeedKmh, const FBNBoostTuning& Tuning)
{
	const float Dt = FMath::Max(RealDt, 0.f);
	if (bInDraftPocket && !bInputLocked && bOnGround && SpeedKmh >= Tuning.SlipstreamMinSpeedKmh)
	{
		bSlipstreaming = true;
		SlipstreamCharge = FMath::Clamp(SlipstreamCharge + Tuning.SlipstreamBuildRate * Dt, 0.f, 1.f);
		if (!bNosActive)
		{
			NosFraction = FMath::Clamp(NosFraction + Tuning.SlipstreamNosBonusPerSecond * SlipstreamCharge * Dt, 0.f, 1.f);
		}
	}
	else
	{
		bSlipstreaming = false;
		SlipstreamCharge = FMath::Max(0.f, SlipstreamCharge - Tuning.SlipstreamDecayRate * Dt);
	}
}

float FBNBoostState::GetSlipstreamAccelCmPerSec2(const FBNBoostTuning& Tuning) const
{
	return SlipstreamCharge > 0.05f ? Tuning.SlipstreamMaxAccelCmPerSec2 * SlipstreamCharge : 0.f;
}

void FBNBoostState::UpdateCountdownLaunch(float RealDt, float PlayerThrottle, const FBNBoostTuning& Tuning)
{
	const float Dt = FMath::Max(RealDt, 0.f);
	const float Throttle = FMath::Clamp(PlayerThrottle, 0.f, 1.f);
	if (Throttle > 0.05f)
	{
		LaunchRevFraction = FMath::Clamp(LaunchRevFraction + Throttle * Tuning.LaunchRevRiseRate * Dt, 0.f, 1.f);
	}
	else
	{
		LaunchRevFraction = FMath::Clamp(LaunchRevFraction - Tuning.LaunchRevFallRate * Dt, 0.f, 1.f);
	}
}

bool FBNBoostState::TriggerLaunchRelease(const FBNBoostTuning& Tuning)
{
	const bool bPerfect = IsInLaunchSweetSpot(Tuning);
	LaunchRevFraction = 0.f;
	if (bPerfect)
	{
		LaunchBoostRemaining = Tuning.LaunchBoostDuration;
		LaunchBannerRemaining = LaunchBannerDuration;
		return true;
	}
	return false;
}

void FBNBoostState::AddNearMissBonus(const FBNBoostTuning& Tuning)
{
	NosFraction = FMath::Clamp(NosFraction + Tuning.NosRechargeNearMissBonus, 0.f, 1.f);
	NearMissBannerRemaining = NearMissBannerDuration;
}

void FBNBoostState::TriggerBackfire(float Duration)
{
	BackfireFlashRemaining = FMath::Max(BackfireFlashRemaining, Duration);
}

void FBNBoostState::TickTimers(float RealDt)
{
	const float Dt = FMath::Max(RealDt, 0.f);
	LaunchBoostRemaining = FMath::Max(0.f, LaunchBoostRemaining - Dt);
	LaunchBannerRemaining = FMath::Max(0.f, LaunchBannerRemaining - Dt);
	NearMissBannerRemaining = FMath::Max(0.f, NearMissBannerRemaining - Dt);
	BackfireFlashRemaining = FMath::Max(0.f, BackfireFlashRemaining - Dt);

	NosBlend = FMath::FInterpTo(NosBlend, bNosActive ? 1.f : 0.f, Dt, BlendInterpSpeed);
	SpeedbreakerBlend = FMath::FInterpTo(SpeedbreakerBlend, bSpeedbreakerActive ? 1.f : 0.f, Dt, BlendInterpSpeed);
}

bool FBNBoostState::IsInLaunchSweetSpot(const FBNBoostTuning& Tuning) const
{
	return LaunchRevFraction >= Tuning.LaunchSweetSpotMin && LaunchRevFraction <= Tuning.LaunchSweetSpotMax;
}
