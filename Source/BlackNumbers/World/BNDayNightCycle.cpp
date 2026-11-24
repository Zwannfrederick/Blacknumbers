#include "World/BNDayNightCycle.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"

ABNDayNightCycle::ABNDayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;
	Keys = BNTimeOfDay::MakeDefaultKeys();
}

void ABNDayNightCycle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Keys.Sort([](const FBNTimeOfDayKey& A, const FBNTimeOfDayKey& B) { return A.Hour < B.Hour; });
	LastCaptureHour = -100.f;
	bIsNight = BNTimeOfDay::IsNight(TimeOfDay);
	ApplyLook();
}

void ABNDayNightCycle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPaused)
	{
		SetTimeOfDay(TimeOfDay + DeltaSeconds * 24.f / (DayLengthMinutes * 60.f));
	}
}

void ABNDayNightCycle::SetTimeOfDay(float Hour)
{
	TimeOfDay = BNTimeOfDay::WrapHour(Hour);
	ApplyLook();

	const bool bNowNight = BNTimeOfDay::IsNight(TimeOfDay);
	if (bNowNight != bIsNight)
	{
		bIsNight = bNowNight;
		OnNightChanged.Broadcast(bIsNight);
	}
}

void ABNDayNightCycle::ApplyLook()
{
	const FBNTimeOfDayKey Look = BNTimeOfDay::Evaluate(Keys, TimeOfDay);
	const float SunPitch = BNTimeOfDay::SunPitchDegrees(TimeOfDay);
	const float SunElevation = BNTimeOfDay::SunElevationDegrees(TimeOfDay);
	LastNightFactor = FMath::GetMappedRangeValueClamped(FVector2f(6.f, -6.f), FVector2f(0.f, 1.f), SunElevation);

	if (UWorld* World = GetWorld())
	{
		if (!WorldMPC && FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MPC_BN_World")))
		{
			WorldMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/World/Materials/MPC_BN_World.MPC_BN_World"));
		}
		if (WorldMPC)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, WorldMPC, TEXT("NightFactor"), LastNightFactor);
		}
	}

	if (Sun)
	{
		Sun->SetActorRotation(FRotator(SunPitch, SunYaw, 0.f));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetAtmosphereSunLight(true);
			Light->SetAtmosphereSunLightIndex(0);
			// Fixed priorities: otherwise the engine picks the brighter of sun and moon as the main light, and the
			// switch at dusk changes translucency/fog lighting in one frame (and logs a warning).
			Light->SetForwardShadingPriority(1);
			Light->SetIntensity(Look.SunIntensity * BNTimeOfDay::HorizonFade(SunElevation));
			Light->SetLightColor(Look.SunColor);
		}
	}

	if (Moon)
	{
		Moon->SetActorRotation(FRotator(SunPitch + 180.f, SunYaw, 0.f));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			Light->SetAtmosphereSunLight(true);
			Light->SetAtmosphereSunLightIndex(1);
			Light->SetForwardShadingPriority(0);
			Light->SetIntensity(Look.MoonIntensity * BNTimeOfDay::HorizonFade(-SunElevation));
		}
	}

	if (SkyLight && SkyLight->GetLightComponent())
	{
		USkyLightComponent* Sky = SkyLight->GetLightComponent();
		Sky->SetIntensity(Look.SkyLightIntensity);

		float HoursSinceCapture = FMath::Abs(TimeOfDay - LastCaptureHour);
		HoursSinceCapture = FMath::Min(HoursSinceCapture, 24.f - HoursSinceCapture);
		if (HoursSinceCapture >= SkyRecaptureHours)
		{
			Sky->RecaptureSky();
			LastCaptureHour = TimeOfDay;
		}
	}

	if (Fog && Fog->GetComponent())
	{
		Fog->GetComponent()->SetFogDensity(Look.FogDensity);
		Fog->GetComponent()->SetFogInscatteringColor(Look.FogColor);
	}

	if (PostProcess)
	{
		FPostProcessSettings& Settings = PostProcess->Settings;
		Settings.bOverride_ColorGain = true;
		Settings.ColorGain = FVector4(Look.ColorGain.R, Look.ColorGain.G, Look.ColorGain.B, 1.f);
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(Look.Saturation, Look.Saturation, Look.Saturation, 1.f);
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = Look.ExposureBias;
		// EV100 values: the project enables r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange.
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = BNTimeOfDay::ExposureFloorEV100(SunElevation, DayMinExposureEV100, NightMinExposureEV100);
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = MaxExposureEV100;
		Settings.bOverride_AutoExposureSpeedUp = true;
		Settings.AutoExposureSpeedUp = 1.2f;
		Settings.bOverride_AutoExposureSpeedDown = true;
		Settings.AutoExposureSpeedDown = 1.2f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 0.15f;
		Settings.bOverride_BloomThreshold = true;
		Settings.BloomThreshold = 2.0f;
		Settings.bOverride_LensFlareIntensity = true;
		Settings.LensFlareIntensity = 0.f;
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.2f;
		Settings.bOverride_AmbientOcclusionIntensity = true;
		Settings.AmbientOcclusionIntensity = 0.45f;
		Settings.bOverride_ScreenSpaceReflectionIntensity = true;
		Settings.ScreenSpaceReflectionIntensity = 55.f;
		Settings.bOverride_ScreenSpaceReflectionQuality = true;
		Settings.ScreenSpaceReflectionQuality = 65.f;
		Settings.bOverride_ScreenSpaceReflectionMaxRoughness = true;
		Settings.ScreenSpaceReflectionMaxRoughness = 0.6f;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBNSetTimeCommand(
	TEXT("bn.SetTime"),
	TEXT("bn.SetTime <0-24>  Set time of day on every day/night cycle in the world."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() != 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogTemp, Warning, TEXT("Usage: bn.SetTime <0-24>"));
			return;
		}
		const float Hour = FCString::Atof(*Args[0]);
		for (TActorIterator<ABNDayNightCycle> It(World); It; ++It)
		{
			It->SetTimeOfDay(Hour);
		}
	}));
