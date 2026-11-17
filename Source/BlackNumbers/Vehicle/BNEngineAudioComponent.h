#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "BNEngineAudioComponent.generated.h"

/** What the engine and tyres should sound like right now; derived purely from vehicle state. */
struct FBNEngineSoundTargets
{
	/** Combustion pulses per second; the synth's fundamental. */
	float FiringHz = 0.f;
	/** 0..1 */
	float EngineGain = 0.f;
	/** 0..1, tyre scrub/skid layer. */
	float SkidGain = 0.f;
};

namespace BNEngineSound
{
	/** Straight six, four-stroke: three firings per crank revolution. */
	constexpr float FiringsPerRevolution = 3.f;

	FBNEngineSoundTargets Evaluate(float Rpm, float MaxRpm, float Throttle, float SlipDegrees, float SpeedKmh);
}

/**
 * Original procedural engine + tyre sound: no recorded or extracted samples.
 * Game thread reads the owning ABNVehiclePawn each tick; the audio thread synthesises from smoothed targets.
 */
UCLASS(ClassGroup = (BlackNumbers), meta = (BlueprintSpawnableComponent))
class BLACKNUMBERS_API UBNEngineAudioComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UBNEngineAudioComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const FBNEngineSoundTargets& GetTargets() const { return Targets; }

	UPROPERTY(EditAnywhere, Category = "Engine Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float MasterGain = 0.5f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	FBNEngineSoundTargets Targets;

	// Game thread -> audio thread.
	std::atomic<float> TargetHz{ 0.f };
	std::atomic<float> TargetEngineGain{ 0.f };
	std::atomic<float> TargetSkidGain{ 0.f };

	// Audio thread only.
	float SampleRateHz = 48000.f;
	float SmoothCoefficient = 0.f;
	float CurrentHz = 0.f;
	float CurrentEngineGain = 0.f;
	float CurrentSkidGain = 0.f;
	float Phase = 0.f;
	float SkidLowpass = 0.f;
	FRandomStream Noise{ 1234 };
};
