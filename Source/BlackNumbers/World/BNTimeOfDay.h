#pragma once

#include "CoreMinimal.h"
#include "BNTimeOfDay.generated.h"

/** One look-key on the 24h clock. Values between keys are interpolated linearly, wrapping at midnight. */
USTRUCT(BlueprintType)
struct FBNTimeOfDayKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "24"))
	float Hour = 12.f;

	/** Lux at full elevation; faded to zero automatically as the sun crosses the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SunIntensity = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor SunColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MoonIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SkyLightIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FogColor = FLinearColor(0.8f, 0.78f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float FogDensity = 0.02f;

	/** Colour-grading tint (post process ColorGain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor ColorGain = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Saturation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ExposureBias = 0.f;

	static FBNTimeOfDayKey Lerp(const FBNTimeOfDayKey& A, const FBNTimeOfDayKey& B, float Alpha);
};

/** Pure time-of-day math, kept free of actors so it can be unit tested. */
namespace BNTimeOfDay
{
	float WrapHour(float Hour);

	/** Directional-light pitch for the sun: 6h east horizon, 12h straight down, 18h west horizon. */
	float SunPitchDegrees(float Hour);

	/** Degrees above the horizon: 90 at noon, 0 at 6h/18h, -90 at midnight. */
	float SunElevationDegrees(float Hour);

	/** Night = sun below civil twilight (-6 degrees). */
	bool IsNight(float Hour);

	/** 0 below the horizon, 1 once the body is a few degrees up; avoids lighting from under the ground. */
	float HorizonFade(float ElevationDegrees);

	/** Keys must be sorted by Hour. Empty input returns a default key. */
	FBNTimeOfDayKey Evaluate(TConstArrayView<FBNTimeOfDayKey> SortedKeys, float Hour);

	/** Warm hazy days, golden dusk, cool sodium-lit nights. Original look, tune in the editor. */
	TArray<FBNTimeOfDayKey> MakeDefaultKeys();
}
