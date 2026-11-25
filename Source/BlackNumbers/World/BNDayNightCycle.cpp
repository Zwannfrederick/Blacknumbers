#include "World/BNDayNightCycle.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"

const TCHAR* ABNDayNightCycle::WeatherModeLabel(EBNWeatherMode Mode)
{
	switch (Mode)
	{
	case EBNWeatherMode::WetDamp:   return TEXT("ISLAK ASFALT");
	case EBNWeatherMode::LightRain: return TEXT("HAFİF YAĞMUR");
	default:                        return TEXT("BÖLGESEL DİNAMİK");
	}
}

FBNDistrictAtmosphereProfile ABNDayNightCycle::GetDistrictAtmosphere(EBNDistrict District)
{
	FBNDistrictAtmosphereProfile P;
	switch (District)
	{
	case EBNDistrict::DoguSanayi:
		P.Wetness = 0.52f;
		P.SunYawOffsetDeg = -14.f;
		P.FogDensityScale = 1.18f;
		P.FogTintScale = FLinearColor(1.10f, 0.95f, 0.84f, 1.f);
		P.SunColorScale = FLinearColor(1.07f, 0.95f, 0.86f, 1.f);
		break;
	case EBNDistrict::EskiLiman:
		P.Wetness = 0.78f;
		P.SunYawOffsetDeg = -22.f;
		P.FogDensityScale = 1.24f;
		P.FogTintScale = FLinearColor(0.86f, 1.02f, 1.15f, 1.f);
		P.SunColorScale = FLinearColor(0.92f, 1.00f, 1.08f, 1.f);
		break;
	case EBNDistrict::KuzeyViyadugu:
		P.Wetness = 0.22f;
		P.SunYawOffsetDeg = 8.f;
		P.FogDensityScale = 0.86f;
		P.FogTintScale = FLinearColor(1.02f, 1.02f, 1.05f, 1.f);
		P.SunColorScale = FLinearColor(1.02f, 1.02f, 1.02f, 1.f);
		break;
	case EBNDistrict::YamacMahallesi:
		P.Wetness = 0.30f;
		P.SunYawOffsetDeg = 12.f;
		P.FogDensityScale = 0.95f;
		P.FogTintScale = FLinearColor(1.08f, 0.98f, 0.90f, 1.f);
		P.SunColorScale = FLinearColor(1.06f, 0.98f, 0.92f, 1.f);
		break;
	case EBNDistrict::KuzeyGecidi:
		P.Wetness = 0.58f;
		P.SunYawOffsetDeg = 24.f;
		P.FogDensityScale = 1.14f;
		P.FogTintScale = FLinearColor(0.90f, 1.00f, 1.10f, 1.f);
		P.SunColorScale = FLinearColor(0.96f, 1.02f, 1.08f, 1.f);
		break;
	default: // MerkezBulvar
		P.Wetness = 0.42f;
		P.SunYawOffsetDeg = 0.f;
		P.FogDensityScale = 1.00f;
		P.FogTintScale = FLinearColor(1.04f, 1.00f, 0.96f, 1.f);
		P.SunColorScale = FLinearColor(1.02f, 1.00f, 0.97f, 1.f);
		break;
	}
	return P;
}

float ABNDayNightCycle::ComputeEffectiveWetness(EBNDistrict District, EBNWeatherMode Mode)
{
	const float Base = GetDistrictAtmosphere(District).Wetness;
	switch (Mode)
	{
	case EBNWeatherMode::WetDamp:   return FMath::Max(Base, 0.74f);
	case EBNWeatherMode::LightRain: return 0.92f;
	default:                        return Base;
	}
}

ABNDayNightCycle::ABNDayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	Keys = BNTimeOfDay::MakeDefaultKeys();
}

void ABNDayNightCycle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Keys.Sort([](const FBNTimeOfDayKey& A, const FBNTimeOfDayKey& B) { return A.Hour < B.Hour; });
	LastCaptureHour = -100.f;
	bIsNight = BNTimeOfDay::IsNight(TimeOfDay);
	CurrentWetness = ComputeEffectiveWetness(ActiveDistrict, WeatherMode);
	CurrentRainIntensity = WeatherMode == EBNWeatherMode::LightRain ? 1.f : 0.f;
	ApplyLook();
}

void ABNDayNightCycle::SetWeatherMode(EBNWeatherMode NewMode)
{
	WeatherMode = NewMode;
	CurrentWetness = ComputeEffectiveWetness(ActiveDistrict, WeatherMode);
	CurrentRainIntensity = WeatherMode == EBNWeatherMode::LightRain ? 1.f : 0.f;
	ApplyLook();
}

void ABNDayNightCycle::UpdateRegionalAtmosphere(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	if (World)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const UBNRoadGraphSubsystem* Graph = World->GetSubsystem<UBNRoadGraphSubsystem>();
		if (Pawn && Graph && Graph->HasGraph())
		{
			ActiveDistrict = Graph->GetDistrictAt(Pawn->GetActorLocation());
		}
	}

	const FBNDistrictAtmosphereProfile Target = GetDistrictAtmosphere(ActiveDistrict);
	const float TargetWetness = ComputeEffectiveWetness(ActiveDistrict, WeatherMode);
	const float TargetRain = WeatherMode == EBNWeatherMode::LightRain ? 1.f : 0.f;
	const float TargetFogScale = Target.FogDensityScale * (WeatherMode == EBNWeatherMode::LightRain ? 1.25f : 1.f);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.5f);
	const float Rate = 1.4f;

	const bool bChanged = !FMath::IsNearlyEqual(CurrentWetness, TargetWetness, 0.003f)
		|| !FMath::IsNearlyEqual(CurrentRainIntensity, TargetRain, 0.003f)
		|| !FMath::IsNearlyEqual(CurrentSunYawOffset, Target.SunYawOffsetDeg, 0.05f)
		|| !FMath::IsNearlyEqual(CurrentFogDensityScale, TargetFogScale, 0.003f);

	CurrentWetness = FMath::FInterpTo(CurrentWetness, TargetWetness, Dt, Rate);
	CurrentRainIntensity = FMath::FInterpTo(CurrentRainIntensity, TargetRain, Dt, Rate);
	CurrentSunYawOffset = FMath::FInterpTo(CurrentSunYawOffset, Target.SunYawOffsetDeg, Dt, Rate * 0.8f);
	CurrentFogDensityScale = FMath::FInterpTo(CurrentFogDensityScale, TargetFogScale, Dt, Rate);
	CurrentFogTintScale = FMath::CInterpTo(CurrentFogTintScale, Target.FogTintScale, Dt, Rate);
	CurrentSunColorScale = FMath::CInterpTo(CurrentSunColorScale, Target.SunColorScale, Dt, Rate);

	if (bChanged && bPaused)
	{
		ApplyLook();
	}
}

void ABNDayNightCycle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRegionalAtmosphere(DeltaSeconds);
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
	const float EffectiveSunYaw = SunYaw + CurrentSunYawOffset;
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
			UKismetMaterialLibrary::SetScalarParameterValue(World, WorldMPC, TEXT("Wetness"), CurrentWetness);
			UKismetMaterialLibrary::SetScalarParameterValue(World, WorldMPC, TEXT("RainIntensity"), CurrentRainIntensity);
		}
	}

	if (Sun)
	{
		Sun->SetActorRotation(FRotator(SunPitch, EffectiveSunYaw, 0.f));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetAtmosphereSunLight(true);
			Light->SetAtmosphereSunLightIndex(0);
			Light->SetForwardShadingPriority(1);
			const float RainDim = FMath::Lerp(1.f, 0.72f, CurrentRainIntensity);
			Light->SetIntensity(Look.SunIntensity * BNTimeOfDay::HorizonFade(SunElevation) * RainDim);
			const FLinearColor TintedSun(
				FMath::Clamp(Look.SunColor.R * CurrentSunColorScale.R, 0.f, 1.f),
				FMath::Clamp(Look.SunColor.G * CurrentSunColorScale.G, 0.f, 1.f),
				FMath::Clamp(Look.SunColor.B * CurrentSunColorScale.B, 0.f, 1.f), 1.f);
			Light->SetLightColor(TintedSun);
		}
	}

	if (Moon)
	{
		Moon->SetActorRotation(FRotator(SunPitch + 180.f, EffectiveSunYaw, 0.f));
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
		Fog->GetComponent()->SetFogDensity(Look.FogDensity * CurrentFogDensityScale);
		const FLinearColor RegionalFog(
			FMath::Clamp(Look.FogColor.R * CurrentFogTintScale.R, 0.f, 1.f),
			FMath::Clamp(Look.FogColor.G * CurrentFogTintScale.G, 0.f, 1.f),
			FMath::Clamp(Look.FogColor.B * CurrentFogTintScale.B, 0.f, 1.f), 1.f);
		Fog->GetComponent()->SetFogInscatteringColor(RegionalFog);
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
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = BNTimeOfDay::ExposureFloorEV100(SunElevation, DayMinExposureEV100, NightMinExposureEV100);
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = MaxExposureEV100;
		Settings.bOverride_AutoExposureSpeedUp = true;
		Settings.AutoExposureSpeedUp = 1.2f;
		Settings.bOverride_AutoExposureSpeedDown = true;
		Settings.AutoExposureSpeedDown = 1.2f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = FMath::Lerp(0.15f, 0.24f, CurrentWetness);
		Settings.bOverride_BloomThreshold = true;
		Settings.BloomThreshold = 1.85f;
		Settings.bOverride_LensFlareIntensity = true;
		Settings.LensFlareIntensity = 0.f;
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.2f;
		Settings.bOverride_AmbientOcclusionIntensity = true;
		Settings.AmbientOcclusionIntensity = 0.45f;
		Settings.bOverride_ScreenSpaceReflectionIntensity = true;
		Settings.ScreenSpaceReflectionIntensity = FMath::Lerp(55.f, 88.f, CurrentWetness);
		Settings.bOverride_ScreenSpaceReflectionQuality = true;
		Settings.ScreenSpaceReflectionQuality = 65.f;
		Settings.bOverride_ScreenSpaceReflectionMaxRoughness = true;
		Settings.ScreenSpaceReflectionMaxRoughness = FMath::Lerp(0.58f, 0.72f, CurrentWetness);
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

static FAutoConsoleCommandWithWorldAndArgs GBNWeatherCommand(
	TEXT("bn.Weather"),
	TEXT("bn.Weather <Auto|Wet|Rain>  Sets Karamara weather/wetness mode."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Usage: bn.Weather <Auto|Wet|Rain>"));
			return;
		}
		EBNWeatherMode Mode = EBNWeatherMode::AutoDistrict;
		if (Args[0].Equals(TEXT("Wet"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("WetDamp"), ESearchCase::IgnoreCase))
		{
			Mode = EBNWeatherMode::WetDamp;
		}
		else if (Args[0].Equals(TEXT("Rain"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("LightRain"), ESearchCase::IgnoreCase))
		{
			Mode = EBNWeatherMode::LightRain;
		}
		for (TActorIterator<ABNDayNightCycle> It(World); It; ++It)
		{
			It->SetWeatherMode(Mode);
		}
	}));
