#include "Audio/BNAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "Sound/SoundWave.h"
#include "UI/BNHUD.h"

namespace
{
	float GMusicVolume = 0.45f;
	float GSfxVolume = 0.75f;
	constexpr float CrossfadeRate = 0.85f;
	constexpr float MinSameSfxGapSeconds = 0.06f;

	EBNMusicState TrackKeyForState(EBNMusicState State)
	{
		switch (State)
		{
		case EBNMusicState::Pursuit: return EBNMusicState::Pursuit;
		case EBNMusicState::Race: return EBNMusicState::Race;
		case EBNMusicState::Menu:
		case EBNMusicState::FreeRoam: return EBNMusicState::FreeRoam;
		default: return EBNMusicState::Silent;
		}
	}

	float StateDuckMultiplier(EBNMusicState State)
	{
		switch (State)
		{
		case EBNMusicState::Menu: return 0.58f;
		case EBNMusicState::FreeRoam: return 0.78f;
		case EBNMusicState::Race: return 0.95f;
		case EBNMusicState::Pursuit: return 1.00f;
		default: return 0.f;
		}
	}
}

float UBNAudioSubsystem::GetMusicVolume() { return GMusicVolume; }
void UBNAudioSubsystem::SetMusicVolume(float NewVolume) { GMusicVolume = FMath::Clamp(NewVolume, 0.f, 1.f); }
float UBNAudioSubsystem::GetSfxVolume() { return GSfxVolume; }
void UBNAudioSubsystem::SetSfxVolume(float NewVolume) { GSfxVolume = FMath::Clamp(NewVolume, 0.f, 1.f); }

const TCHAR* UBNAudioSubsystem::GetSfxAssetPath(EBNSfxId Id)
{
	switch (Id)
	{
	case EBNSfxId::CrashLight: return TEXT("/Game/Audio/SFX/SFX_Crash_Light.SFX_Crash_Light");
	case EBNSfxId::CrashHeavy: return TEXT("/Game/Audio/SFX/SFX_Crash_Heavy.SFX_Crash_Heavy");
	case EBNSfxId::BreakWood: return TEXT("/Game/Audio/SFX/SFX_Break_Wood.SFX_Break_Wood");
	case EBNSfxId::BreakMetal: return TEXT("/Game/Audio/SFX/SFX_Break_Metal.SFX_Break_Metal");
	case EBNSfxId::BreakerCollapse: return TEXT("/Game/Audio/SFX/SFX_Breaker_Collapse.SFX_Breaker_Collapse");
	case EBNSfxId::UiHover: return TEXT("/Game/Audio/SFX/SFX_UI_Hover.SFX_UI_Hover");
	case EBNSfxId::UiClick: return TEXT("/Game/Audio/SFX/SFX_UI_Click.SFX_UI_Click");
	case EBNSfxId::UiConfirm: return TEXT("/Game/Audio/SFX/SFX_UI_Confirm.SFX_UI_Confirm");
	case EBNSfxId::UiError: return TEXT("/Game/Audio/SFX/SFX_UI_Error.SFX_UI_Error");
	case EBNSfxId::CountdownBeep: return TEXT("/Game/Audio/SFX/SFX_Countdown_Beep.SFX_Countdown_Beep");
	case EBNSfxId::CountdownGo: return TEXT("/Game/Audio/SFX/SFX_Countdown_Go.SFX_Countdown_Go");
	case EBNSfxId::RadioSquelch: return TEXT("/Game/Audio/SFX/SFX_Radio_Squelch.SFX_Radio_Squelch");
	case EBNSfxId::NosIgnite: return TEXT("/Game/Audio/SFX/SFX_Nos_Ignite.SFX_Nos_Ignite");
	case EBNSfxId::NearMissWhoosh: return TEXT("/Game/Audio/SFX/SFX_NearMiss_Whoosh.SFX_NearMiss_Whoosh");
	case EBNSfxId::ShiftPop: return TEXT("/Game/Audio/SFX/SFX_Shift_Pop.SFX_Shift_Pop");
	case EBNSfxId::EngineBlown: return TEXT("/Game/Audio/SFX/SFX_Engine_Blown.SFX_Engine_Blown");
	case EBNSfxId::GpsArrive: return TEXT("/Game/Audio/SFX/SFX_Gps_Arrive.SFX_Gps_Arrive");
	default: return TEXT("");
	}
}

const TCHAR* UBNAudioSubsystem::GetMusicAssetPath(EBNMusicState State)
{
	switch (TrackKeyForState(State))
	{
	case EBNMusicState::FreeRoam: return TEXT("/Game/Audio/Music/MUS_FreeRoam_Karamara.MUS_FreeRoam_Karamara");
	case EBNMusicState::Race: return TEXT("/Game/Audio/Music/MUS_Race_Adrenaline.MUS_Race_Adrenaline");
	case EBNMusicState::Pursuit: return TEXT("/Game/Audio/Music/MUS_Pursuit_Heat.MUS_Pursuit_Heat");
	default: return TEXT("");
	}
}

bool UBNAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UBNAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBNAudioSubsystem, STATGROUP_Tickables);
}

void UBNAudioSubsystem::Deinitialize()
{
	if (IsValid(ActiveMusicComp))
	{
		ActiveMusicComp->Stop();
		ActiveMusicComp->DestroyComponent();
	}
	if (IsValid(FadingMusicComp))
	{
		FadingMusicComp->Stop();
		FadingMusicComp->DestroyComponent();
	}
	ActiveMusicComp = nullptr;
	FadingMusicComp = nullptr;
	SfxCache.Reset();
	MusicCache.Reset();
	Super::Deinitialize();
}

bool UBNAudioSubsystem::CanEmitAudio() const
{
	const UWorld* World = GetWorld();
	return World && !FApp::IsUnattended() && !GIsAutomationTesting && FApp::CanEverRenderAudio();
}

USoundWave* UBNAudioSubsystem::GetOrLoadSfx(EBNSfxId Id)
{
	if (TObjectPtr<USoundWave>* Found = SfxCache.Find(Id))
	{
		return Found->Get();
	}
	const FString AssetPath(GetSfxAssetPath(Id));
	const FString PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
	USoundWave* Wave = (!PackagePath.IsEmpty() && FPackageName::DoesPackageExist(PackagePath))
		? LoadObject<USoundWave>(nullptr, *AssetPath)
		: nullptr;
	SfxCache.Add(Id, Wave);
	return Wave;
}

USoundWave* UBNAudioSubsystem::GetOrLoadMusic(EBNMusicState State)
{
	const EBNMusicState Key = TrackKeyForState(State);
	if (Key == EBNMusicState::Silent)
	{
		return nullptr;
	}
	if (TObjectPtr<USoundWave>* Found = MusicCache.Find(Key))
	{
		return Found->Get();
	}
	const FString AssetPath(GetMusicAssetPath(Key));
	const FString PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
	USoundWave* Wave = (!PackagePath.IsEmpty() && FPackageName::DoesPackageExist(PackagePath))
		? LoadObject<USoundWave>(nullptr, *AssetPath)
		: nullptr;
	if (Wave)
	{
		Wave->bLooping = true;
	}
	MusicCache.Add(Key, Wave);
	return Wave;
}

void UBNAudioSubsystem::PlaySfx2D(EBNSfxId Id, float VolumeScale, float Pitch)
{
	const int32 Idx = static_cast<int32>(Id);
	if (!CanEmitAudio() || GSfxVolume <= 0.005f || VolumeScale <= 0.005f || Idx < 0 || Idx >= static_cast<int32>(EBNSfxId::Count))
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World ? World->GetRealTimeSeconds() : 0.f;
	if (Now - LastSfxTimes[Idx] < MinSameSfxGapSeconds)
	{
		return;
	}
	if (USoundWave* Wave = GetOrLoadSfx(Id))
	{
		LastSfxTimes[Idx] = Now;
		UGameplayStatics::PlaySound2D(World, Wave, FMath::Clamp(GSfxVolume * VolumeScale, 0.f, 1.5f), FMath::Clamp(Pitch, 0.5f, 2.f));
	}
}

void UBNAudioSubsystem::PlaySfxAtLocation(EBNSfxId Id, const FVector& Location, float VolumeScale, float Pitch)
{
	const int32 Idx = static_cast<int32>(Id);
	if (!CanEmitAudio() || GSfxVolume <= 0.005f || VolumeScale <= 0.005f || Idx < 0 || Idx >= static_cast<int32>(EBNSfxId::Count))
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World ? World->GetRealTimeSeconds() : 0.f;
	if (Now - LastSfxTimes[Idx] < MinSameSfxGapSeconds)
	{
		return;
	}
	if (USoundWave* Wave = GetOrLoadSfx(Id))
	{
		LastSfxTimes[Idx] = Now;
		UGameplayStatics::PlaySoundAtLocation(World, Wave, Location, FRotator::ZeroRotator,
			FMath::Clamp(GSfxVolume * VolumeScale, 0.f, 1.5f), FMath::Clamp(Pitch, 0.5f, 2.f));
	}
}

void UBNAudioSubsystem::PlayUiSfx(EBNSfxId Id, float VolumeScale)
{
	if (!GEngine || FApp::IsUnattended() || GIsAutomationTesting)
	{
		return;
	}
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (UWorld* W = Ctx.World(); W && W->IsGameWorld())
		{
			if (UBNAudioSubsystem* Audio = W->GetSubsystem<UBNAudioSubsystem>())
			{
				Audio->PlaySfx2D(Id, VolumeScale);
				return;
			}
		}
	}
}

void UBNAudioSubsystem::SetMusicStateOverride(EBNMusicState StateOverride, bool bUseOverride)
{
	OverrideState = StateOverride;
	bHasStateOverride = bUseOverride;
}

EBNMusicState UBNAudioSubsystem::DetermineDesiredMusicState() const
{
	if (bHasStateOverride)
	{
		return OverrideState;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return EBNMusicState::Silent;
	}
	if (const UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>())
	{
		if (Pursuit->GetState().IsPursuitRunning())
		{
			return EBNMusicState::Pursuit;
		}
	}
	for (TActorIterator<ABNRaceEvent> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->GetState() == EBNRaceState::Countdown || It->GetState() == EBNRaceState::Racing)
		{
			return EBNMusicState::Race;
		}
	}
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const ABNHUD* HUD = PC->GetHUD<ABNHUD>(); HUD && HUD->IsMenuOpen())
		{
			return EBNMusicState::Menu;
		}
	}
	return EBNMusicState::FreeRoam;
}

void UBNAudioSubsystem::UpdateMusicCrossfade(float DeltaTime, EBNMusicState Desired)
{
	ActiveMusicState = Desired;
	if (!CanEmitAudio())
	{
		return;
	}

	const EBNMusicState DesiredTrack = TrackKeyForState(Desired);
	if (DesiredTrack != ActiveTrackKey)
	{
		// Move current active track to fading slot
		if (IsValid(FadingMusicComp))
		{
			FadingMusicComp->Stop();
			FadingMusicComp->DestroyComponent();
			FadingMusicComp = nullptr;
		}
		FadingMusicComp = ActiveMusicComp;
		FadingTrackGain = ActiveTrackGain;
		ActiveMusicComp = nullptr;
		ActiveTrackGain = 0.f;
		ActiveTrackKey = DesiredTrack;

		if (USoundWave* NextWave = GetOrLoadMusic(DesiredTrack))
		{
			ActiveMusicComp = UGameplayStatics::SpawnSound2D(GetWorld(), NextWave, 0.001f, 1.f, 0.f, nullptr, false, false);
			if (IsValid(ActiveMusicComp))
			{
				ActiveMusicComp->bIsUISound = true; // Keep music smooth during Speedbreaker / menus
			}
		}
	}

	const float TargetVol = FMath::Max(GMusicVolume * StateDuckMultiplier(Desired), 0.f);
	if (IsValid(ActiveMusicComp))
	{
		ActiveTrackGain = FMath::FInterpConstantTo(ActiveTrackGain, TargetVol, DeltaTime, CrossfadeRate);
		ActiveMusicComp->SetVolumeMultiplier(FMath::Max(ActiveTrackGain, 0.0001f));
		if (!ActiveMusicComp->IsPlaying() && TargetVol > 0.005f)
		{
			ActiveMusicComp->Play();
		}
	}
	if (IsValid(FadingMusicComp))
	{
		FadingTrackGain = FMath::FInterpConstantTo(FadingTrackGain, 0.f, DeltaTime, CrossfadeRate * 1.15f);
		if (FadingTrackGain <= 0.002f)
		{
			FadingMusicComp->Stop();
			FadingMusicComp->DestroyComponent();
			FadingMusicComp = nullptr;
			FadingTrackGain = 0.f;
		}
		else
		{
			FadingMusicComp->SetVolumeMultiplier(FadingTrackGain);
		}
	}
}

void UBNAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float SafeDt = FMath::Clamp(DeltaTime, 0.f, 0.1f);
	StatePollAccum += SafeDt;
	EBNMusicState Desired = ActiveMusicState;
	if (StatePollAccum >= 0.10f || ActiveMusicState == EBNMusicState::Silent)
	{
		StatePollAccum = 0.f;
		Desired = DetermineDesiredMusicState();
	}
	UpdateMusicCrossfade(SafeDt, Desired);
}
