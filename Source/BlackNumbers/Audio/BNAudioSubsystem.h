#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BNAudioSubsystem.generated.h"

class UAudioComponent;
class USoundWave;

UENUM(BlueprintType)
enum class EBNMusicState : uint8
{
	Silent = 0,
	Menu,
	FreeRoam,
	Race,
	Pursuit,
};

UENUM(BlueprintType)
enum class EBNSfxId : uint8
{
	CrashLight = 0,
	CrashHeavy,
	BreakWood,
	BreakMetal,
	BreakerCollapse,
	UiHover,
	UiClick,
	UiConfirm,
	UiError,
	CountdownBeep,
	CountdownGo,
	RadioSquelch,
	NosIgnite,
	NearMissWhoosh,
	Count UMETA(Hidden)
};

/**
 * Manages BLACK NUMBERS adaptive music crossfades (FreeRoam / Race / Pursuit / Menu)
 * and one-shot gameplay + UI sound effects imported under /Game/Audio/.
 */
UCLASS()
class BLACKNUMBERS_API UBNAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySfx2D(EBNSfxId Id, float VolumeScale = 1.f, float Pitch = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySfxAtLocation(EBNSfxId Id, const FVector& Location, float VolumeScale = 1.f, float Pitch = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetMusicStateOverride(EBNMusicState StateOverride, bool bUseOverride);

	UFUNCTION(BlueprintPure, Category = "Audio")
	EBNMusicState GetActiveMusicState() const { return ActiveMusicState; }

	/** Convenience helper for Slate widgets without a direct UWorld pointer. */
	static void PlayUiSfx(EBNSfxId Id, float VolumeScale = 1.f);

	static float GetMusicVolume();
	static void SetMusicVolume(float NewVolume);
	static float GetSfxVolume();
	static void SetSfxVolume(float NewVolume);

	static const TCHAR* GetSfxAssetPath(EBNSfxId Id);
	static const TCHAR* GetMusicAssetPath(EBNMusicState State);

private:
	USoundWave* GetOrLoadSfx(EBNSfxId Id);
	USoundWave* GetOrLoadMusic(EBNMusicState State);
	EBNMusicState DetermineDesiredMusicState() const;
	void UpdateMusicCrossfade(float DeltaTime, EBNMusicState Desired);
	bool CanEmitAudio() const;

	UPROPERTY(Transient)
	TMap<EBNSfxId, TObjectPtr<USoundWave>> SfxCache;

	UPROPERTY(Transient)
	TMap<EBNMusicState, TObjectPtr<USoundWave>> MusicCache;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveMusicComp;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FadingMusicComp;

	EBNMusicState ActiveMusicState = EBNMusicState::Silent;
	EBNMusicState ActiveTrackKey = EBNMusicState::Silent;
	EBNMusicState OverrideState = EBNMusicState::Silent;
	bool bHasStateOverride = false;
	float ActiveTrackGain = 0.f;
	float FadingTrackGain = 0.f;
	float StatePollAccum = 0.f;
	float LastSfxTimes[static_cast<int32>(EBNSfxId::Count)] = {};
};
