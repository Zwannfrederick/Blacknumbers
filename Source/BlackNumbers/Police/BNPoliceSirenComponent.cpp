#include "Police/BNPoliceSirenComponent.h"

namespace
{
	constexpr float WailMinHz = 620.f;
	constexpr float WailMaxHz = 1180.f;
	constexpr float WailPeriodSeconds = 2.2f;

	constexpr float YelpMinHz = 660.f;
	constexpr float YelpMaxHz = 1320.f;
	constexpr float YelpPeriodSeconds = 0.36f;

	constexpr float SmoothSeconds = 0.04f;
	constexpr float FullVolumeRadius = 1800.f;
	constexpr float FalloffDistance = 12000.f;
}

namespace BNPoliceSiren
{
	float EvaluateFrequency(float TimeSeconds, bool bYelpMode, float TimeDilation)
	{
		const float MinHz = bYelpMode ? YelpMinHz : WailMinHz;
		const float MaxHz = bYelpMode ? YelpMaxHz : WailMaxHz;
		const float Period = bYelpMode ? YelpPeriodSeconds : WailPeriodSeconds;
		const float Alpha = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * (TimeSeconds / Period));
		const float PitchScale = FMath::Lerp(0.62f, 1.f, FMath::Clamp((TimeDilation - 0.35f) / 0.65f, 0.f, 1.f));
		return FMath::Lerp(MinHz, MaxHz, Alpha) * PitchScale;
	}
}

UBNPoliceSirenComponent::UBNPoliceSirenComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	NumChannels = 1;
	bAutoActivate = true;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShapeExtents = FVector(FullVolumeRadius, 0.f, 0.f);
	AttenuationOverrides.FalloffDistance = FalloffDistance;
}

void UBNPoliceSirenComponent::SetSirenState(bool bInActive, bool bInYelp, float InTimeDilation)
{
	bDesiredActive = bInActive;
	TargetGain.store(bInActive ? MasterGain : 0.f, std::memory_order_relaxed);
	bYelpMode.store(bInYelp, std::memory_order_relaxed);
	TargetDilation.store(InTimeDilation, std::memory_order_relaxed);
}

bool UBNPoliceSirenComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateHz = static_cast<float>(SampleRate);
	SmoothCoefficient = 1.f - FMath::Exp(-1.f / (SmoothSeconds * SampleRateHz));
	return true;
}

int32 UBNPoliceSirenComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float GoalGain = TargetGain.load(std::memory_order_relaxed);
	const bool bYelp = bYelpMode.load(std::memory_order_relaxed);
	const float Dilation = TargetDilation.load(std::memory_order_relaxed);
	const float SampleDt = FMath::Max(Dilation, 0.25f) / SampleRateHz;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		CurrentGain += (GoalGain - CurrentGain) * SmoothCoefficient;
		if (CurrentGain < 0.0005f && GoalGain == 0.f)
		{
			OutAudio[i] = 0.f;
			continue;
		}
		SweepSeconds += SampleDt;
		const float Hz = BNPoliceSiren::EvaluateFrequency(SweepSeconds, bYelp, Dilation);
		Phase = FMath::Frac(Phase + Hz / SampleRateHz);
		const float Tone = 0.7f * FMath::Sin(UE_TWO_PI * Phase) + 0.25f * FMath::Sin(UE_TWO_PI * 2.f * Phase);
		OutAudio[i] = CurrentGain * Tone;
	}
	return NumSamples;
}
