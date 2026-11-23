#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "BNEngineAudioComponent.generated.h"

/** What the engine, tyres, NOS and backfire should sound like right now; derived purely from vehicle state. */
struct FBNEngineSoundTargets
{
	/** Combustion pulses per second; the synth's fundamental. */
	float FiringHz = 0.f;
	/** 0..1 */
	float EngineGain = 0.f;
	/** 0..1, tyre scrub/skid layer. */
	float SkidGain = 0.f;
	/** 0..1, pressurized NOS/turbo turbine layer. */
	float NosGain = 0.f;
	/** Pitch multiplier from Speedbreaker time dilation (1.0 at normal time). */
	float PitchScale = 1.f;
	/** 0..1, low-end exhaust backfire pop on upshift/launch. */
	float BackfireGain = 0.f;
	/** 0..1, turbo blow-off valve flutter chirp on high-RPM lift-off or upshift. */
	float BovGain = 0.f;
};

namespace BNEngineSound
{
	/** Straight six, four-stroke: three firings per crank revolution. */
	constexpr float FiringsPerRevolution = 3.f;

	/** bOnGround: no tyre sound while airborne. */
	BLACKNUMBERS_API FBNEngineSoundTargets Evaluate(float Rpm, float MaxRpm, float Throttle, float SlipDegrees, float SpeedKmh,
		bool bOnGround = true, bool bNosActive = false, float TimeDilation = 1.f, bool bBackfire = false);
}

/**
 * Original procedural engine + tyre + NOS + backfire sound: no recorded or extracted samples.
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

	// Game thread only (turbo spool & BOV lift-off/upshift detection).
	float PrevThrottle = 0.f;
	int32 PrevGear = 0;
	float TurboSpool = 0.f;
	float BovRemaining = 0.f;
	float BovPeak = 0.f;

	// Game thread -> audio thread.
	std::atomic<float> TargetHz{ 0.f };
	std::atomic<float> TargetEngineGain{ 0.f };
	std::atomic<float> TargetSkidGain{ 0.f };
	std::atomic<float> TargetNosGain{ 0.f };
	std::atomic<float> TargetPitchScale{ 1.f };
	std::atomic<float> TargetBackfireGain{ 0.f };
	std::atomic<float> TargetBovGain{ 0.f };
	std::atomic<uint32> BovTriggerSeq{ 0 };

	// Audio thread only.
	float SampleRateHz = 48000.f;
	float SmoothCoefficient = 0.f;
	float SkidSmoothCoeff = 0.f;
	float CurrentHz = 0.f;
	float CurrentEngineGain = 0.f;
	float CurrentSkidGain = 0.f;
	float CurrentNosGain = 0.f;
	float CurrentPitchScale = 1.f;
	float CurrentBackfireGain = 0.f;
	float Phase = 0.f;
	float SquealPhase = 0.f;
	float WobblePhase = 0.f;
	float NosPhase = 0.f;
	float BackfirePhase = 0.f;
	uint32 NoiseState = 0x12345678u;
	float EngineLp = 0.f;
	float SkidBp1Low = 0.f;
	float SkidBp1Band = 0.f;
	float SkidBp2Low = 0.f;
	float SkidBp2Band = 0.f;
	float NosBpLow = 0.f;
	float NosBpBand = 0.f;
	float BackfireEnv = 0.f;
	float BackfireLp = 0.f;
	uint32 LastBovTriggerSeq = 0;
	float BovTime = 1.f;
	float BovActiveGain = 0.f;
	float BovPhase = 0.f;
	float BovBpLow = 0.f;
	float BovBpBand = 0.f;
};
