#include "World/BNTimeOfDay.h"

namespace
{
	constexpr float CivilTwilightDegrees = -6.f;
	constexpr float HorizonFadeStartDegrees = -2.f;
	constexpr float HorizonFadeRangeDegrees = 6.f;
	/** Exposure floor blends from day to night between these sun elevations (end of nautical twilight). */
	constexpr float ExposureDayElevation = 0.f;
	constexpr float ExposureNightElevation = -12.f;
}

FBNTimeOfDayKey FBNTimeOfDayKey::Lerp(const FBNTimeOfDayKey& A, const FBNTimeOfDayKey& B, float Alpha)
{
	FBNTimeOfDayKey Out;
	Out.Hour = FMath::Lerp(A.Hour, B.Hour, Alpha);
	Out.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, Alpha);
	Out.SunColor = FMath::Lerp(A.SunColor, B.SunColor, Alpha);
	Out.MoonIntensity = FMath::Lerp(A.MoonIntensity, B.MoonIntensity, Alpha);
	Out.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, Alpha);
	Out.FogColor = FMath::Lerp(A.FogColor, B.FogColor, Alpha);
	Out.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
	Out.ColorGain = FMath::Lerp(A.ColorGain, B.ColorGain, Alpha);
	Out.Saturation = FMath::Lerp(A.Saturation, B.Saturation, Alpha);
	Out.ExposureBias = FMath::Lerp(A.ExposureBias, B.ExposureBias, Alpha);
	return Out;
}

namespace BNTimeOfDay
{
	float WrapHour(float Hour)
	{
		const float Wrapped = FMath::Fmod(Hour, 24.f);
		return Wrapped < 0.f ? Wrapped + 24.f : Wrapped;
	}

	float SunPitchDegrees(float Hour)
	{
		return WrapHour(Hour) * 15.f + 90.f;
	}

	float SunElevationDegrees(float Hour)
	{
		return 90.f - 15.f * FMath::Abs(WrapHour(Hour) - 12.f);
	}

	bool IsNight(float Hour)
	{
		return SunElevationDegrees(Hour) < CivilTwilightDegrees;
	}

	float HorizonFade(float ElevationDegrees)
	{
		return FMath::Clamp((ElevationDegrees - HorizonFadeStartDegrees) / HorizonFadeRangeDegrees, 0.f, 1.f);
	}

	float ExposureFloorEV100(float SunElevationDegrees, float DayFloor, float NightFloor)
	{
		const float NightAmount = FMath::GetMappedRangeValueClamped(
			FVector2f(ExposureDayElevation, ExposureNightElevation), FVector2f(0.f, 1.f), SunElevationDegrees);
		return FMath::Lerp(DayFloor, NightFloor, NightAmount);
	}

	FBNTimeOfDayKey Evaluate(TConstArrayView<FBNTimeOfDayKey> SortedKeys, float Hour)
	{
		if (SortedKeys.Num() == 0)
		{
			return FBNTimeOfDayKey();
		}
		if (SortedKeys.Num() == 1)
		{
			return SortedKeys[0];
		}

		const float H = WrapHour(Hour);
		int32 Next = SortedKeys.IndexOfByPredicate([H](const FBNTimeOfDayKey& Key) { return Key.Hour > H; });
		if (Next == INDEX_NONE)
		{
			Next = 0;
		}
		const int32 Prev = (Next - 1 + SortedKeys.Num()) % SortedKeys.Num();

		float Span = SortedKeys[Next].Hour - SortedKeys[Prev].Hour;
		if (Span <= 0.f)
		{
			Span += 24.f;
		}
		float Offset = H - SortedKeys[Prev].Hour;
		if (Offset < 0.f)
		{
			Offset += 24.f;
		}

		FBNTimeOfDayKey Out = FBNTimeOfDayKey::Lerp(SortedKeys[Prev], SortedKeys[Next], Offset / Span);
		Out.Hour = H;
		return Out;
	}

	TArray<FBNTimeOfDayKey> MakeDefaultKeys()
	{
		auto Key = [](float Hour, float Sun, FLinearColor SunColor, float Moon, float Sky,
			FLinearColor Fog, float FogDensity, FLinearColor Gain, float Saturation, float Exposure)
		{
			FBNTimeOfDayKey K;
			K.Hour = Hour;
			K.SunIntensity = Sun;
			K.SunColor = SunColor;
			K.MoonIntensity = Moon;
			K.SkyLightIntensity = Sky;
			K.FogColor = Fog;
			K.FogDensity = FogDensity;
			K.ColorGain = Gain;
			K.Saturation = Saturation;
			K.ExposureBias = Exposure;
			return K;
		};

		return {
			// Night: cool blue-green shadows, room left for orange street lamps.
			Key(0.f, 0.f, FLinearColor(1.f, 0.6f, 0.4f), 0.4f, 0.15f,
				FLinearColor(0.05f, 0.07f, 0.12f), 0.03f, FLinearColor(0.85f, 0.95f, 1.1f), 0.85f, 1.5f),
			// Dawn
			Key(5.5f, 2.f, FLinearColor(1.f, 0.55f, 0.35f), 0.1f, 0.4f,
				FLinearColor(0.6f, 0.45f, 0.4f), 0.035f, FLinearColor(1.05f, 0.95f, 0.9f), 0.95f, 0.5f),
			// Morning
			Key(8.f, 8.f, FLinearColor(1.f, 0.9f, 0.75f), 0.f, 0.9f,
				FLinearColor(0.75f, 0.72f, 0.6f), 0.025f, FLinearColor(1.05f, 1.f, 0.88f), 1.05f, 0.f),
			// Noon: warm, hazy, slightly bleached.
			Key(13.f, 10.f, FLinearColor(1.f, 0.95f, 0.85f), 0.f, 1.f,
				FLinearColor(0.8f, 0.78f, 0.65f), 0.02f, FLinearColor(1.08f, 1.02f, 0.85f), 1.1f, 0.f),
			// Golden hour
			Key(18.f, 5.f, FLinearColor(1.f, 0.6f, 0.3f), 0.f, 0.6f,
				FLinearColor(0.9f, 0.6f, 0.4f), 0.03f, FLinearColor(1.12f, 0.98f, 0.8f), 1.15f, 0.3f),
			// Dusk
			Key(20.f, 0.3f, FLinearColor(0.9f, 0.4f, 0.3f), 0.2f, 0.25f,
				FLinearColor(0.3f, 0.25f, 0.35f), 0.035f, FLinearColor(0.95f, 0.92f, 1.05f), 0.9f, 1.f),
		};
	}
}
