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
	constexpr float SkidSmoothSeconds = 0.08f;
	constexpr float EngineMix = 0.28f;
	constexpr float SkidMix = 0.13f;
	constexpr float NosMix = 0.10f;
	constexpr float BackfireMix = 0.18f;
	constexpr float BovMix = 0.11f;
	constexpr float NosBaseHz = 1120.f;
	constexpr float BackfirePulseHz = 78.f;
	constexpr float BovMinRpm = 4500.f;
	constexpr float BovDurationSeconds = 0.34f;
}

namespace BNEngineSound
{
	float FiringsPerRevForProfile(EBNEngineProfile Profile)
	{
		switch (Profile)
		{
		case EBNEngineProfile::TurboI4:   return 2.0f;
		case EBNEngineProfile::SportI6:   return 3.0f;
		case EBNEngineProfile::MuscleV8:  return 4.0f;
		case EBNEngineProfile::RallyV6:   return 3.0f;
		case EBNEngineProfile::ExoticV10: return 5.0f;
		default:                          return FiringsPerRevolution;
		}
	}

	EBNEngineProfile ProfileForCar(EBNCarId CarId)
	{
		switch (CarId)
		{
		case EBNCarId::ValkenRS4:     return EBNEngineProfile::TurboI4;
		case EBNCarId::Vandal440R:    return EBNEngineProfile::MuscleV8;
		case EBNCarId::TauronV6Rally: return EBNEngineProfile::RallyV6;
		case EBNCarId::NocturneBN01:  return EBNEngineProfile::ExoticV10;
		case EBNCarId::DracoRST:
		case EBNCarId::KuroganeMSpec:
		default:                      return EBNEngineProfile::SportI6;
		}
	}

	FBNEngineSoundTargets Evaluate(float Rpm, float MaxRpm, float Throttle, float SlipDegrees, float SpeedKmh,
		bool bOnGround, bool bNosActive, float TimeDilation, bool bBackfire, EBNEngineProfile Profile)
	{
		FBNEngineSoundTargets Out;
		Out.FiringHz = FMath::Max(Rpm, 0.f) / 60.f * FiringsPerRevForProfile(Profile);
		const float RpmFraction = MaxRpm > 0.f ? FMath::Clamp(Rpm / MaxRpm, 0.f, 1.f) : 0.f;
		// Load: throttle dominates, revs add a little on top so coasting at high rpm is not silent.
		Out.EngineGain = FMath::Clamp(FMath::Lerp(IdleGain, 1.f, FMath::Clamp(Throttle, 0.f, 1.f)) * (0.75f + 0.25f * RpmFraction), 0.f, 1.f);
		const float SlipAmount = FMath::GetMappedRangeValueClamped(
			FVector2f(SkidStartSlipDegrees, SkidFullSlipDegrees), FVector2f(0.f, 1.f), FMath::Abs(SlipDegrees));
		const float SpeedAmount = FMath::GetMappedRangeValueClamped(
			FVector2f(SkidMinSpeedKmh, SkidFullSpeedKmh), FVector2f(0.f, 1.f), FMath::Abs(SpeedKmh));
		Out.SkidGain = bOnGround ? SlipAmount * SpeedAmount : 0.f;
		Out.NosGain = bNosActive ? (0.65f + 0.35f * RpmFraction) : 0.f;
		Out.PitchScale = FMath::Lerp(0.62f, 1.f, FMath::Clamp((TimeDilation - 0.35f) / 0.65f, 0.f, 1.f));
		Out.BackfireGain = bBackfire ? 1.f : 0.f;
		return Out;
	}
}

namespace
{
	constexpr float FullVolumeRadius = 1500.f;
	constexpr float FalloffDistance = 8000.f;
}

UBNEngineAudioComponent::UBNEngineAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	NumChannels = 1;
	bAutoActivate = true;
	// Positional, so rival cars are heard where they are; the own car (camera within the inner radius) stays at full volume.
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShapeExtents = FVector(FullVolumeRadius, 0.f, 0.f);
	AttenuationOverrides.FalloffDistance = FalloffDistance;
}

void UBNEngineAudioComponent::SetEngineProfile(EBNEngineProfile Profile)
{
	ActiveProfile = Profile;
	AtomicProfile.store(static_cast<uint8>(Profile), std::memory_order_relaxed);
}

void UBNEngineAudioComponent::SetEngineProfileForCar(EBNCarId CarId)
{
	SetEngineProfile(BNEngineSound::ProfileForCar(CarId));
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
	const float Rpm = Move->GetEngineRotationSpeed();
	const float Throttle = Move->GetThrottleInput();
	const int32 Gear = Move->GetCurrentGear();
	const bool bNosActive = Car->IsNosActive();
	const float EffectiveDilation = Car->IsSpeedbreakerActive() ? Car->Boost.SpeedbreakerTimeDilation : 1.f;
	Targets = BNEngineSound::Evaluate(Rpm, Move->GetEngineMaxRotationSpeed(),
		Throttle, Car->GetSlipAngleDegrees(), Car->GetSpeedKmh(), Car->IsOnGround(),
		bNosActive, EffectiveDilation, Car->IsBackfireActive(), ActiveProfile);

	// Track turbo boost spool and trigger subtle BOV flutter chirp on high-RPM throttle lift-off or upshift.
	const float SafeDt = FMath::Max(DeltaTime, 0.f);
	const float TargetSpool = bNosActive ? 1.f
		: (Throttle > 0.45f && Rpm > 3200.f)
			? FMath::GetMappedRangeValueClamped(FVector2f(3200.f, 6500.f), FVector2f(0.25f, 1.f), Rpm) * Throttle
			: 0.f;
	TurboSpool = FMath::FInterpTo(TurboSpool, TargetSpool, SafeDt, TargetSpool > TurboSpool ? 3.5f : 2.2f);

	const bool bHighRpmLiftOff = PrevThrottle >= 0.55f && Throttle <= 0.25f && Rpm > BovMinRpm;
	const bool bHighRpmUpshift = Gear > PrevGear && Gear > 1 && PrevGear >= 1 && Rpm > BovMinRpm;
	if ((bHighRpmLiftOff || bHighRpmUpshift) && TurboSpool > 0.30f && BovRemaining <= 0.05f)
	{
		const float RpmScale = FMath::GetMappedRangeValueClamped(FVector2f(BovMinRpm, 7500.f), FVector2f(0.55f, 1.f), Rpm);
		BovPeak = FMath::Clamp(TurboSpool * RpmScale, 0.35f, 1.f);
		BovRemaining = BovDurationSeconds;
		TurboSpool *= 0.25f;
		TargetBovGain.store(BovPeak * MasterGain, std::memory_order_relaxed);
		BovTriggerSeq.fetch_add(1u, std::memory_order_release);
	}
	BovRemaining = FMath::Max(0.f, BovRemaining - SafeDt);
	Targets.BovGain = BovRemaining > 0.f ? BovPeak * (BovRemaining / BovDurationSeconds) : 0.f;
	PrevThrottle = Throttle;
	PrevGear = Gear;

	const float TurboWhineScale = (ActiveProfile == EBNEngineProfile::TurboI4) ? 0.16f
		: (ActiveProfile == EBNEngineProfile::RallyV6) ? 0.12f
		: (ActiveProfile == EBNEngineProfile::ExoticV10) ? 0.09f
		: 0.05f;
	TargetHz.store(Targets.FiringHz, std::memory_order_relaxed);
	TargetEngineGain.store(Targets.EngineGain * MasterGain, std::memory_order_relaxed);
	TargetSkidGain.store(Targets.SkidGain * MasterGain, std::memory_order_relaxed);
	TargetNosGain.store(Targets.NosGain * MasterGain, std::memory_order_relaxed);
	TargetPitchScale.store(Targets.PitchScale, std::memory_order_relaxed);
	TargetBackfireGain.store(Targets.BackfireGain * MasterGain, std::memory_order_relaxed);
	TargetTurboWhine.store(TurboSpool * TurboWhineScale * MasterGain, std::memory_order_relaxed);
}

bool UBNEngineAudioComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateHz = FMath::Max(static_cast<float>(SampleRate), 22050.f);
	SmoothCoefficient = 1.f - FMath::Exp(-1.f / (SmoothSeconds * SampleRateHz));
	SkidSmoothCoeff = 1.f - FMath::Exp(-1.f / (SkidSmoothSeconds * SampleRateHz));
	return true;
}

int32 UBNEngineAudioComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float GoalHz = TargetHz.load(std::memory_order_relaxed);
	const float GoalEngine = TargetEngineGain.load(std::memory_order_relaxed);
	const float GoalSkid = TargetSkidGain.load(std::memory_order_relaxed);
	const float GoalNos = TargetNosGain.load(std::memory_order_relaxed);
	const float GoalPitchScale = TargetPitchScale.load(std::memory_order_relaxed);
	const float GoalBackfire = TargetBackfireGain.load(std::memory_order_relaxed);
	const float GoalTurboWhine = TargetTurboWhine.load(std::memory_order_relaxed);
	const EBNEngineProfile Profile = static_cast<EBNEngineProfile>(AtomicProfile.load(std::memory_order_relaxed));
	const uint32 BovSeq = BovTriggerSeq.load(std::memory_order_acquire);
	if (BovSeq != LastBovTriggerSeq)
	{
		LastBovTriggerSeq = BovSeq;
		BovTime = 0.f;
		BovActiveGain = TargetBovGain.load(std::memory_order_relaxed);
		BovPhase = 0.f;
	}
	const float Fs = FMath::Max(SampleRateHz, 22050.f);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		CurrentHz += (GoalHz - CurrentHz) * SmoothCoefficient;
		CurrentEngineGain += (GoalEngine - CurrentEngineGain) * SmoothCoefficient;
		CurrentSkidGain += (GoalSkid - CurrentSkidGain) * SkidSmoothCoeff;
		CurrentNosGain += (GoalNos - CurrentNosGain) * SmoothCoefficient;
		CurrentPitchScale += (GoalPitchScale - CurrentPitchScale) * SmoothCoefficient;
		CurrentBackfireGain += (GoalBackfire - CurrentBackfireGain) * SmoothCoefficient;
		CurrentTurboWhine += (GoalTurboWhine - CurrentTurboWhine) * SmoothCoefficient;

		// Deterministic audio-thread white noise for tire scrub, NOS rush, and exhaust pop.
		uint32 X = NoiseState;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		NoiseState = X;
		const float White = (static_cast<float>(X & 0x00FFFFFFu) / 8388608.f) - 1.f;

		// Per-car cylinder/harmonic voice synthesis (Turbo I4, Sport I6, Muscle V8, Rally V6, Exotic V10).
		Phase = FMath::Frac(Phase + (CurrentHz * CurrentPitchScale) / Fs);
		const float S1 = FMath::Sin(UE_TWO_PI * Phase);
		const float S2 = FMath::Sin(UE_TWO_PI * 2.f * Phase);
		const float S3 = FMath::Sin(UE_TWO_PI * 3.f * Phase);
		const float S4 = FMath::Sin(UE_TWO_PI * 4.f * Phase);
		const float Sub = FMath::Sin(UE_PI * Phase);
		float RawEngine = 0.f;
		float CutoffLo = 650.f;
		float CutoffHi = 2400.f;
		switch (Profile)
		{
		case EBNEngineProfile::TurboI4:
			RawEngine = FMath::Tanh((0.50f * S1 + 0.48f * S2 + 0.30f * S3 + 0.22f * S4 + 0.14f * Sub) * 0.92f);
			CutoffLo = 750.f;
			CutoffHi = 2800.f;
			break;
		case EBNEngineProfile::MuscleV8:
			// Deep cross-plane V8 rumble: strong 0.5x & 1.5x lope + warm heavy saturation
			RawEngine = FMath::Tanh((0.52f * Sub + 0.58f * S1 + 0.36f * FMath::Sin(UE_TWO_PI * 1.5f * Phase) + 0.26f * S2 + 0.14f * S3) * 1.15f);
			CutoffLo = 420.f;
			CutoffHi = 1850.f;
			break;
		case EBNEngineProfile::RallyV6:
			// Gritty off-beat rally V6 burble
			RawEngine = FMath::Tanh((0.46f * S1 + 0.38f * FMath::Sin(UE_TWO_PI * 1.5f * Phase) + 0.32f * S2 + 0.22f * S3 + 0.34f * Sub) * 1.02f);
			CutoffLo = 560.f;
			CutoffHi = 2350.f;
			break;
		case EBNEngineProfile::ExoticV10:
			// High-revving exotic V10 scream: crisp upper harmonics & wide open top-end
			RawEngine = FMath::Tanh((0.48f * S1 + 0.44f * S2 + 0.34f * S3 + 0.24f * S4 + 0.18f * FMath::Sin(UE_TWO_PI * 5.f * Phase) + 0.10f * Sub) * 0.90f);
			CutoffLo = 880.f;
			CutoffHi = 3450.f;
			break;
		default: // SportI6
			RawEngine = FMath::Tanh((0.55f * S1 + 0.40f * S2 + 0.22f * S3 + 0.12f * S4 + 0.28f * Sub) * 0.85f);
			break;
		}
		const float CutoffHz = FMath::GetMappedRangeValueClamped(
			FVector2f(40.f, 450.f), FVector2f(CutoffLo, CutoffHi), CurrentHz) * CurrentPitchScale;
		const float EngineLpAlpha = FMath::Clamp(UE_TWO_PI * CutoffHz / (Fs + UE_TWO_PI * CutoffHz), 0.02f, 0.80f);
		EngineLp += EngineLpAlpha * (RawEngine - EngineLp);

		// Subtle turbocharger spool whine rising with firing Hz
		const float TurboHz = FMath::Clamp((1550.f + CurrentHz * 4.2f) * CurrentPitchScale, 1200.f, 5400.f);
		TurboWhinePhase = FMath::Frac(TurboWhinePhase + TurboHz / Fs);
		const float TurboWhine = FMath::Sin(UE_TWO_PI * TurboWhinePhase) * CurrentTurboWhine;

		// Tire scrub: dual SVF bandpass over asphalt noise (grit 1450 Hz + rubber bite 760 Hz), zero sine squeal.
		WobblePhase = FMath::Frac(WobblePhase + 4.5f / Fs);
		const float ScrubMod = 1.f + 0.06f * FMath::Sin(UE_TWO_PI * WobblePhase);
		const float F1 = FMath::Min(2.f * FMath::Sin(UE_PI * (1450.f * CurrentPitchScale * ScrubMod) / Fs), 0.8f);
		SkidBp1Low += F1 * SkidBp1Band;
		const float High1 = White - SkidBp1Low - (1.f / 1.4f) * SkidBp1Band;
		SkidBp1Band += F1 * High1;

		const float F2 = FMath::Min(2.f * FMath::Sin(UE_PI * (760.f * CurrentPitchScale * ScrubMod) / Fs), 0.8f);
		SkidBp2Low += F2 * SkidBp2Band;
		const float High2 = White - SkidBp2Low - (1.f / 3.0f) * SkidBp2Band;
		SkidBp2Band += F2 * High2;
		const float TireScrub = FMath::Clamp(0.68f * SkidBp1Band + 0.38f * SkidBp2Band, -1.f, 1.f);

		// Pressurised NOS air rush + gentle spool.
		const float NosBandF = FMath::Min(2.f * FMath::Sin(UE_PI * (2100.f * CurrentPitchScale) / Fs), 0.8f);
		NosBpLow += NosBandF * NosBpBand;
		const float NosHigh = White - NosBpLow - (1.f / 1.5f) * NosBpBand;
		NosBpBand += NosBandF * NosHigh;
		NosPhase = FMath::Frac(NosPhase + (NosBaseHz * CurrentPitchScale) / Fs);
		const float NosRush = FMath::Clamp(0.85f * NosBpBand + 0.10f * FMath::Sin(UE_TWO_PI * NosPhase), -1.f, 1.f);

		// Exhaust backfire pop: crisp decaying low-passed thump.
		BackfireEnv = GoalBackfire > 0.1f ? FMath::Max(BackfireEnv, 1.f) : BackfireEnv * FMath::Exp(-14.f / Fs);
		BackfirePhase = FMath::Frac(BackfirePhase + (BackfirePulseHz * CurrentPitchScale) / Fs);
		const float RawPop = 0.65f * White + 0.55f * FMath::Sin(UE_TWO_PI * BackfirePhase);
		const float PopLpAlpha = FMath::Clamp(UE_TWO_PI * 420.f / (Fs + UE_TWO_PI * 420.f), 0.02f, 0.6f);
		BackfireLp += PopLpAlpha * (RawPop - BackfireLp);

		// Turbo blow-off valve flutter chirp: descending resonant bandpass air + soft compressor chop.
		float BovSample = 0.f;
		if (BovTime < BovDurationSeconds && BovActiveGain > 0.001f)
		{
			const float T = FMath::Clamp(BovTime / BovDurationSeconds, 0.f, 1.f);
			BovTime += 1.f / Fs;
			const float Attack = FMath::Min(BovTime / 0.012f, 1.f);
			const float Env = FMath::Exp(-BovTime * 7.8f) * Attack;
			const float FlutterHz = FMath::Lerp(19.f, 13.f, T);
			const float FlutterWave = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * FlutterHz * BovTime);
			const float Flutter = 0.30f + 0.70f * FlutterWave * FlutterWave;
			const float SweepHz = FMath::Lerp(3000.f, 1450.f, T) * CurrentPitchScale;
			const float BovF = FMath::Min(2.f * FMath::Sin(UE_PI * SweepHz / Fs), 0.8f);
			BovBpLow += BovF * BovBpBand;
			const float BovHigh = White - BovBpLow - (1.f / 4.2f) * BovBpBand;
			BovBpBand += BovF * BovHigh;
			BovPhase = FMath::Frac(BovPhase + (SweepHz * 0.90f) / Fs);
			BovSample = FMath::Clamp(0.80f * BovBpBand + 0.20f * FMath::Sin(UE_TWO_PI * BovPhase), -1.f, 1.f)
				* Env * Flutter * BovActiveGain;
		}

		OutAudio[i] = FMath::Clamp(
			EngineMix * CurrentEngineGain * EngineLp
			+ 0.14f * TurboWhine
			+ SkidMix * CurrentSkidGain * TireScrub
			+ NosMix * CurrentNosGain * NosRush
			+ BackfireMix * BackfireEnv * BackfireLp
			+ BovMix * BovSample,
			-0.95f, 0.95f);
	}
	return NumSamples;
}
