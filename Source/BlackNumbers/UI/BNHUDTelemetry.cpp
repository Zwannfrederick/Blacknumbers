#include "UI/BNHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Garage/BNGarageZone.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNDayNightCycle.h"

namespace BNTelemetry
{
	const FLinearColor PanelBg(0.035f, 0.042f, 0.055f, 0.84f);
	const FLinearColor Amber(1.f, 0.72f, 0.14f, 1.f);
	const FLinearColor Cyan(0.22f, 0.88f, 1.f, 1.f);
	const FLinearColor Green(0.24f, 0.94f, 0.50f, 1.f);
	const FLinearColor Red(0.96f, 0.22f, 0.18f, 1.f);
	const FLinearColor Chalk(0.95f, 0.96f, 0.98f, 1.f);
	const FLinearColor RainStreak(0.72f, 0.84f, 0.96f, 0.24f);
}

void ABNHUD::NotifySpeedTrapTriggered(const FString& TrapName, float SpeedKmh, bool bNewRecord, int32 CashGain)
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	SpeedTrapFlashUntil = Now + 0.24;
	ActivityBannerUntil = Now + 3.4;
	bActivityBannerRecord = bNewRecord;
	ActivityBannerTitle = bNewRecord
		? FString::Printf(TEXT("HIZ TUZAĞI: %.0f KM/H  —  YENİ REKOR!"), SpeedKmh)
		: FString::Printf(TEXT("HIZ TUZAĞI: %.0f KM/H"), SpeedKmh);
	ActivityBannerDetail = bNewRecord
		? FString::Printf(TEXT("%s   •   +$%s NAKİT"), *TrapName.ToUpper(), *FString::FormatAsNumber(CashGain))
		: TrapName.ToUpper();
}

void ABNHUD::NotifyStuntJumpLanded(const FString& JumpName, float DistanceMeters, bool bNewRecord, int32 CashGain)
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	ActivityBannerUntil = Now + 3.6;
	bActivityBannerRecord = bNewRecord;
	ActivityBannerTitle = bNewRecord
		? FString::Printf(TEXT("SIÇRAMA: %.0f METRE  —  YENİ REKOR!"), DistanceMeters)
		: FString::Printf(TEXT("SIÇRAMA TAMAMLANDI: %.0f METRE"), DistanceMeters);
	ActivityBannerDetail = bNewRecord
		? FString::Printf(TEXT("%s   •   +$%s NAKİT"), *JumpName.ToUpper(), *FString::FormatAsNumber(CashGain))
		: JumpName.ToUpper();
}

void ABNHUD::DrawProTelemetry(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, bool bPursuitVisible)
{
	if (!Canvas || !GEngine || !GetWorld())
	{
		return;
	}
	UFont* Font = GEngine->GetLargeFont();
	const double Now = GetWorld()->GetRealTimeSeconds();

	// 1. Lightweight screen-space rain streaks when LightRain mode is active (0 actors, 0 particles, <0.05 ms GPU)
	float RainIntensity = 0.f;
	if (TActorIterator<ABNDayNightCycle> It(GetWorld()); It)
	{
		RainIntensity = It->GetRainIntensity();
	}
	if (RainIntensity > 0.05f)
	{
		const float SpeedBoost = FMath::Clamp(FMath::Abs(Car.GetSpeedKmh()) / 180.f, 0.f, 1.f);
		const int32 StreakCount = FMath::RoundToInt(42.f * RainIntensity);
		const float SlantX = -14.f - SpeedBoost * 18.f;
		const float LenY = 26.f + SpeedBoost * 24.f;
		FLinearColor Col = BNTelemetry::RainStreak;
		Col.A = 0.22f * RainIntensity;
		for (int32 i = 0; i < StreakCount; ++i)
		{
			const float SeedX = FMath::Fmod((i * 197.3f) + static_cast<float>(Now) * (35.f + (i % 5) * 14.f), 1.f);
			const float SeedY = FMath::Fmod((i * 0.618f) + static_cast<float>(Now) * (1.85f + (i % 4) * 0.42f + SpeedBoost), 1.f);
			const float X = SeedX * Canvas->ClipX;
			const float Y = SeedY * Canvas->ClipY;
			DrawLine(X, Y, X + SlantX, Y + LenY, Col, 1.2f);
		}
	}

	// 2. Speed Trap white camera strobe flash overlay
	if (Now < SpeedTrapFlashUntil)
	{
		const float Alpha = FMath::Clamp(static_cast<float>(SpeedTrapFlashUntil - Now) / 0.24f, 0.f, 1.f) * 0.48f;
		DrawRect(FLinearColor(1.f, 0.98f, 0.92f, Alpha), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}

	// 3. Upcoming sharp turn cue during active Sprint/Circuit races (high-speed pro racing readability)
	if (ActiveRace && ActiveRace->GetState() == EBNRaceState::Racing)
	{
		float TurnDeg = 0.f;
		float DistM = 0.f;
		if (ActiveRace->GetUpcomingTurnCue(&Car, TurnDeg, DistM))
		{
			const bool bRight = TurnDeg > 0.f;
			const float AbsDeg = FMath::Abs(TurnDeg);
			const FString TurnText = AbsDeg >= 85.f
				? (bRight ? FString::Printf(TEXT("SAĞA U-DÖNÜŞ  >>>   %dm"), FMath::RoundToInt(DistM))
				          : FString::Printf(TEXT("%dm   <<<  SOLA U-DÖNÜŞ"), FMath::RoundToInt(DistM)))
				: AbsDeg >= 52.f
					? (bRight ? FString::Printf(TEXT("KESKİN SAĞ  >>>   %dm"), FMath::RoundToInt(DistM))
					          : FString::Printf(TEXT("%dm   <<<  KESKİN SOL"), FMath::RoundToInt(DistM)))
					: (bRight ? FString::Printf(TEXT("SAĞA VİRAJ  >>   %dm"), FMath::RoundToInt(DistM))
					          : FString::Printf(TEXT("%dm   <<  SOLA VİRAJ"), FMath::RoundToInt(DistM)));

			const FLinearColor CueCol = AbsDeg >= 75.f ? BNTelemetry::Red : BNTelemetry::Amber;
			float TW = 0.f, TH = 0.f;
			GetTextSize(TurnText, TW, TH, Font, 1.08f);
			const float CueY = bPursuitVisible ? 194.f : 118.f;
			const float BoxX = (Canvas->ClipX - TW) * 0.5f - 18.f;
			DrawRect(BNTelemetry::PanelBg, BoxX, CueY - 6.f, TW + 36.f, TH + 12.f);
			DrawRect(CueCol, BoxX, CueY - 6.f, TW + 36.f, 3.f);
			DrawCentred(TurnText, CueY, 1.08f, CueCol);
		}

		// Live Checkpoint Split Delta vs Personal Best
		if (ActiveRace->HasSplitDelta())
		{
			const float Delta = ActiveRace->GetLastSplitDeltaSeconds();
			const bool bAhead = Delta <= 0.f;
			const FString SplitText = FString::Printf(TEXT("REKOR FARKI:  %s%.2fs"), bAhead ? TEXT("") : TEXT("+"), Delta);
			const FLinearColor SplitCol = bAhead ? BNTelemetry::Green : BNTelemetry::Red;
			const float SplitY = bPursuitVisible ? 236.f : 158.f;
			DrawCentred(SplitText, SplitY, 0.96f, SplitCol);
		}
	}

	// 4. Slipstream (Drafting) charge pill near lower center when drafting rivals/traffic
	if (Car.GetSlipstreamCharge() > 0.08f)
	{
		const float Charge = FMath::Clamp(Car.GetSlipstreamCharge(), 0.f, 1.f);
		const float BarW = 210.f;
		const float BarX = (Canvas->ClipX - BarW) * 0.5f;
		const float BarY = Canvas->ClipY - 122.f;
		DrawRect(BNTelemetry::PanelBg, BarX - 10.f, BarY - 22.f, BarW + 20.f, 36.f);
		DrawCentred(TEXT("HAVA KORİDORU  //  SLIPSTREAM"), BarY - 18.f, 0.72f, BNTelemetry::Cyan);
		DrawRect(FLinearColor(0.08f, 0.10f, 0.14f, 0.9f), BarX, BarY, BarW, 8.f);
		DrawRect(BNTelemetry::Cyan, BarX, BarY, BarW * Charge, 8.f);
	}

	// 5. Look Back mirror indicator
	if (Car.IsLookingBack())
	{
		DrawCentred(TEXT("<< ARKA BAKIŞ >>"), 76.f, 0.85f, BNTelemetry::Amber);
	}

	// 6. Interactive Garage Bay prompt (replaces forced menu yank when driving near Selin'in Garaji)
	if (!ActiveRace && !bPursuitVisible)
	{
		if (const ABNGarageZone* Garage = ABNGarageZone::FindNearbyGarageZone(&Car))
		{
			if (ABNRaceEvent::FindNearbyIdleRace(GetWorld(), &Car) == nullptr)
			{
				const FString Prompt = FString::Printf(TEXT("[ENTER / G]  %s  —  MODİFİYE & ARAÇ DEĞİŞTİR"),
					*Garage->GarageName.ToUpper());
				float PW = 0.f, PH = 0.f;
				GetTextSize(Prompt, PW, PH, Font, 0.98f);
				const float BoxW = PW + 36.f;
				const float BoxH = PH + 18.f;
				const float X = (Canvas->ClipX - BoxW) * 0.5f;
				const float Y = Canvas->ClipY - 154.f;
				DrawRect(BNTelemetry::PanelBg, X, Y, BoxW, BoxH);
				DrawRect(BNTelemetry::Green, X, Y, BoxW, 3.f);
				DrawText(Prompt, BNTelemetry::Green, X + 18.f, Y + 9.f, Font, 0.98f);
			}
		}
	}

	// 7. Speed Trap / Stunt Jump result toast
	if (Now < ActivityBannerUntil && !ActivityBannerTitle.IsEmpty())
	{
		const float Rem = static_cast<float>(ActivityBannerUntil - Now);
		const float Alpha = FMath::Clamp(Rem * 2.f, 0.f, 1.f);
		FLinearColor MainCol = bActivityBannerRecord ? BNTelemetry::Green : BNTelemetry::Amber;
		MainCol.A = Alpha;
		FLinearColor SubCol = BNTelemetry::Chalk;
		SubCol.A = Alpha * 0.92f;
		const float Y = bPursuitVisible ? 204.f : 156.f;
		DrawCentred(ActivityBannerTitle, Y, 1.24f, MainCol);
		DrawCentred(ActivityBannerDetail, Y + 30.f, 0.82f, SubCol);
	}
}
