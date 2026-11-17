#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/BNTimeOfDay.h"
#include "BNDayNightCycle.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBNNightChangedSignature, bool, bIsNight);

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
	float DayMinExposureEV100 = -10.f;

	UPROPERTY(EditAnywhere, Category = "Look")
	float NightMinExposureEV100 = 3.f;

	UPROPERTY(EditAnywhere, Category = "Look")
	float MaxExposureEV100 = 14.f;

	/** Sky light recapture is expensive without Lumen; refresh only every N in-game hours. */
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (ClampMin = "0.05"))
	float SkyRecaptureHours = 0.25f;

private:
	void ApplyLook();

	float LastCaptureHour = -100.f;
	/** Last broadcast state, only for change detection: not saved, so PIE copies start false. Read IsNight() instead. */
	bool bIsNight = false;
};
