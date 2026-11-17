#include "Vehicle/BNEngineAudioComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float IdleGain = 0.3f;
	/** Below this the body slip angle is noise (parking, bumps), not a slide. */
	constexpr float SkidStartSlipDegrees = 10.f;
	constexpr float SkidFullSlipDegrees = 30.f;
	constexpr float SkidMinSpeedKmh = 15.f;
	constexpr float SkidFullSpeedKmh = 50.f;
	/** Pitch/volume glide time; short enough to hear gear changes as a drop. */
	constexpr float SmoothSeconds = 0.03f;
	constexpr int32 Harmonics = 6;
	constexpr float EngineMix = 0.32f;
	constexpr float SkidMix = 0.25f;
	/** Tyre squeal: a tone that bends a little, not a noise bed (noise read as rustling grass). */
	constexpr float SquealHz = 820.f;
	constexpr float SquealWobbleHz = 7.f;
	constexpr float SquealWobbleDepth = 0.04f;
}

namespace BNEngineSound
{
	FBNEngineSoundTargets Evaluate(float Rpm, float MaxRpm, float Throttle, float SlipDegrees, float SpeedKmh, bool bOnGround)
	{
		FBNEngineSoundTargets Out;
		Out.FiringHz = FMath::Max(Rpm, 0.f) / 60.f * FiringsPerRevolution;
		const float RpmFraction = MaxRpm > 0.f ? FMath::Clamp(Rpm / MaxRpm, 0.f, 1.f) : 0.f;
		// Load: throttle dominates, revs add a little on top so coasting at high rpm is not silent.
		Out.EngineGain = FMath::Clamp(FMath::Lerp(IdleGain, 1.f, FMath::Clamp(Throttle, 0.f, 1.f)) * (0.75f + 0.25f * RpmFraction), 0.f, 1.f);
		const float SlipAmount = FMath::GetMappedRangeValueClamped(
			FVector2f(SkidStartSlipDegrees, SkidFullSlipDegrees), FVector2f(0.f, 1.f), FMath::Abs(SlipDegrees));
		const float SpeedAmount = FMath::GetMappedRangeValueClamped(
			FVector2f(SkidMinSpeedKmh, SkidFullSpeedKmh), FVector2f(0.f, 1.f), FMath::Abs(SpeedKmh));
		Out.SkidGain = bOnGround ? SlipAmount * SpeedAmount : 0.f;
		return Out;
	}
}

UBNEngineAudioComponent::UBNEngineAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	NumChannels = 1;
	bAutoActivate = true;
}

void UBNEngineAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwner());
	if (!Car)
	{
		return;
	}
	UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
	Targets = BNEngineSound::Evaluate(Move->GetEngineRotationSpeed(), Move->GetEngineMaxRotationSpeed(),
		Move->GetThrottleInput(), Car->GetSlipAngleDegrees(), Car->GetSpeedKmh(), Move->IsMovingOnGround());
	TargetHz.store(Targets.FiringHz, std::memory_order_relaxed);
	TargetEngineGain.store(Targets.EngineGain * MasterGain, std::memory_order_relaxed);
	TargetSkidGain.store(Targets.SkidGain * MasterGain, std::memory_order_relaxed);
}

bool UBNEngineAudioComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateHz = static_cast<float>(SampleRate);
	SmoothCoefficient = 1.f - FMath::Exp(-1.f / (SmoothSeconds * SampleRateHz));
	return true;
}

int32 UBNEngineAudioComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float GoalHz = TargetHz.load(std::memory_order_relaxed);
	const float GoalEngine = TargetEngineGain.load(std::memory_order_relaxed);
	const float GoalSkid = TargetSkidGain.load(std::memory_order_relaxed);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		CurrentHz += (GoalHz - CurrentHz) * SmoothCoefficient;
		CurrentEngineGain += (GoalEngine - CurrentEngineGain) * SmoothCoefficient;
		CurrentSkidGain += (GoalSkid - CurrentSkidGain) * SmoothCoefficient;

		Phase = FMath::Frac(Phase + CurrentHz / SampleRateHz);
		float Engine = 0.5f * FMath::Sin(UE_PI * Phase); // half-order rumble
		for (int32 k = 1; k <= Harmonics; ++k)
		{
			Engine += FMath::Sin(UE_TWO_PI * k * Phase) / k;
		}

		WobblePhase = FMath::Frac(WobblePhase + SquealWobbleHz / SampleRateHz);
		SquealPhase = FMath::Frac(SquealPhase + SquealHz * (1.f + SquealWobbleDepth * FMath::Sin(UE_TWO_PI * WobblePhase)) / SampleRateHz);
		const float Squeal = FMath::Sin(UE_TWO_PI * SquealPhase) + 0.3f * FMath::Sin(UE_TWO_PI * 2.f * SquealPhase);

		OutAudio[i] = EngineMix * CurrentEngineGain * Engine * 0.5f + SkidMix * CurrentSkidGain * Squeal * 0.7f;
	}
	return NumSamples;
}
