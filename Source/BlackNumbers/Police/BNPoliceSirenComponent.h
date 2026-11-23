#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "BNPoliceSirenComponent.generated.h"

namespace BNPoliceSiren
{
	/** Dual-mode police siren pitch in Hz (Wail or rapid Yelp), scaled down by Speedbreaker dilation. */
	BLACKNUMBERS_API float EvaluateFrequency(float TimeSeconds, bool bYelpMode, float TimeDilation = 1.f);
}

/**
 * Spatialized 3D procedural police siren (no external WAV assets).
 * Pitch tracks Wail/Yelp cycle and bends down smoothly during Speedbreaker slow motion.
 */
UCLASS(ClassGroup = (BlackNumbers), meta = (BlueprintSpawnableComponent))
class BLACKNUMBERS_API UBNPoliceSirenComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UBNPoliceSirenComponent(const FObjectInitializer& ObjectInitializer);

	void SetSirenState(bool bInActive, bool bInYelp, float InTimeDilation);
	bool IsSirenActive() const { return bDesiredActive; }

	UPROPERTY(EditAnywhere, Category = "Siren", meta = (ClampMin = "0", ClampMax = "1"))
	float MasterGain = 0.28f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	bool bDesiredActive = false;

	// Game thread -> audio thread.
	std::atomic<float> TargetGain{ 0.f };
	std::atomic<bool> bYelpMode{ false };
	std::atomic<float> TargetDilation{ 1.f };

	// Audio thread only.
	float SampleRateHz = 48000.f;
	float SmoothCoefficient = 0.f;
	float CurrentGain = 0.f;
	float Phase = 0.f;
	float SweepSeconds = 0.f;
};
