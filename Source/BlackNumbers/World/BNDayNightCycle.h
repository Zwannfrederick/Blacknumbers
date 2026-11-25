#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/BNRoadGraph.h"
#include "World/BNTimeOfDay.h"
#include "BNDayNightCycle.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class UMaterialParameterCollection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBNNightChangedSignature, bool, bIsNight);

UENUM(BlueprintType)
enum class EBNWeatherMode : uint8
{
	AutoDistrict = 0,
	WetDamp = 1,
	LightRain = 2,
	Count UMETA(Hidden)
};

/** Per-district regional wetness, sun azimuth shift, and atmospheric tint profile. */
struct BLACKNUMBERS_API FBNDistrictAtmosphereProfile
{
	float Wetness = 0.35f;
	float SunYawOffsetDeg = 0.f;
	float FogDensityScale = 1.f;
	FLinearColor FogTintScale = FLinearColor(1.f, 1.f, 1.f, 1.f);
	FLinearColor SunColorScale = FLinearColor(1.f, 1.f, 1.f, 1.f);
};

/**
 * Drives sun, moon, sky light, fog and colour grading from a 24h clock.
 * Place once per level and point it at the scene's lights (all must be Movable).
 * The SkyAtmosphere follows the sun automatically. Listeners such as street lamps
 * should bind OnNightChanged and read IsNight() once when they bind.
 */
UCLASS()
class BLACKNUMBERS_API ABNDayNightCycle : public AActor
{
	GENERATED_BODY()

public:
	ABNDayNightCycle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetTimeOfDay(float Hour);

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetTimeOfDay() const { return TimeOfDay; }

	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsNight() const { return BNTimeOfDay::IsNight(TimeOfDay); }

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetNightFactor() const { return LastNightFactor; }

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeatherMode(EBNWeatherMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Weather")
	EBNWeatherMode GetWeatherMode() const { return WeatherMode; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetCurrentWetness() const { return CurrentWetness; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetRainIntensity() const { return CurrentRainIntensity; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	EBNDistrict GetActiveDistrict() const { return ActiveDistrict; }

	static const TCHAR* WeatherModeLabel(EBNWeatherMode Mode);
	static FBNDistrictAtmosphereProfile GetDistrictAtmosphere(EBNDistrict District);
	static float ComputeEffectiveWetness(EBNDistrict District, EBNWeatherMode Mode);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EBNWeatherMode WeatherMode = EBNWeatherMode::AutoDistrict;

	UPROPERTY(BlueprintAssignable, Category = "Time")
	FBNNightChangedSignature OnNightChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "0", ClampMax = "24"))
	float TimeOfDay = 17.f;

	/** Real minutes for one full in-game day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin = "0.1"))
	float DayLengthMinutes = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	bool bPaused = false;

	/** Compass direction of the sun path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	float SunYaw = -30.f;

	/** Sorted by Hour on construction. */
	UPROPERTY(EditAnywhere, Category = "Look")
	TArray<FBNTimeOfDayKey> Keys;

	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<ADirectionalLight> Moon;

	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<AExponentialHeightFog> Fog;

	/** Should be Unbound so grading applies everywhere. */
	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<APostProcessVolume> PostProcess;

	/**
	 * Auto-exposure floor (EV100) while the sun is up; -10 leaves eye adaptation free.
	 * Through twilight it rises to NightMinExposureEV100, which stops eye adaptation from lifting a moonlit
	 * street to daylight brightness, so nights stay dark and the sodium lamps read as pools of light.
	 */
	UPROPERTY(EditAnywhere, Category = "Look")
	float DayMinExposureEV100 = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Look")
	float NightMinExposureEV100 = 3.f;

	UPROPERTY(EditAnywhere, Category = "Look")
	float MaxExposureEV100 = 4.5f;

	/** Sky light recapture is expensive without Lumen; refresh only every N in-game hours. */
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (ClampMin = "0.05"))
	float SkyRecaptureHours = 2.0f;

private:
	void UpdateRegionalAtmosphere(float DeltaSeconds);
	void ApplyLook();

	float LastCaptureHour = -100.f;
	float LastNightFactor = 0.f;
	EBNDistrict ActiveDistrict = EBNDistrict::MerkezBulvar;
	float CurrentWetness = 0.42f;
	float CurrentRainIntensity = 0.f;
	float CurrentSunYawOffset = 0.f;
	float CurrentFogDensityScale = 1.f;
	FLinearColor CurrentFogTintScale = FLinearColor(1.f, 1.f, 1.f, 1.f);
	FLinearColor CurrentSunColorScale = FLinearColor(1.f, 1.f, 1.f, 1.f);
	/** Last broadcast state, only for change detection: not saved, so PIE copies start false. Read IsNight() instead. */
	bool bIsNight = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> WorldMPC;
};
