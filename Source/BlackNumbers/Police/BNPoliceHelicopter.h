#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "GameFramework/Actor.h"
#include <atomic>
#include "BNPoliceHelicopter.generated.h"

class ABNVehiclePawn;
class USpotLightComponent;
class UStaticMeshComponent;

/** Procedural spatialized rotor thump (blade-pass pulses over filtered noise), no WAV asset. */
UCLASS(ClassGroup = (BlackNumbers))
class BLACKNUMBERS_API UBNHeliRotorAudioComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UBNHeliRotorAudioComponent(const FObjectInitializer& ObjectInitializer);
	void SetGain(float InGain) { TargetGain.store(InGain, std::memory_order_relaxed); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<float> TargetGain{ 0.f };
	float SampleRateHz = 48000.f;
	float CurrentGain = 0.f;
	float BladePhase = 0.f;
	float NoiseLowpass = 0.f;
	uint32 NoiseSeed = 0x1234567u;
};

/**
 * Heat 4+ police helicopter: hovers above and ahead of the suspect, sweeps a searchlight,
 * and keeps the pursuit alive while it has clear line of sight. Tunnels, bridges and tall
 * buildings break its view; it returns to base after HeliOnStationSeconds.
 */
UCLASS()
class BLACKNUMBERS_API ABNPoliceHelicopter : public AActor
{
	GENERATED_BODY()

public:
	ABNPoliceHelicopter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetTarget(ABNVehiclePawn* InTarget) { Target = InTarget; }
	bool HasSightOfTarget() const { return bHasSight && !bLeaving; }
	void Leave();
	bool IsLeaving() const { return bLeaving; }

private:
	void UpdateSight();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Rotor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpotLightComponent> Searchlight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBNHeliRotorAudioComponent> RotorAudio;

	TWeakObjectPtr<ABNVehiclePawn> Target;
	FVector Velocity = FVector::ZeroVector;
	bool bHasSight = false;
	bool bLeaving = false;
	float SightCheckTimer = 0.f;
	float LeaveSeconds = 0.f;
	float RotorAngle = 0.f;
};
