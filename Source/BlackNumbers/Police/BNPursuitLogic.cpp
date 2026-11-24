#include "Police/BNPursuitLogic.h"

namespace
{
	constexpr float ResultBannerSeconds = 4.5f;
	constexpr float BustRadiusCm = 1300.f;
	constexpr float BustMaxSpeedKmh = 20.f;
	constexpr float BustFillSeconds = 4.0f;
	constexpr float BustDecayPerSecond = 0.6f;

	constexpr float BreakSightDistanceCm = 9500.f;
	constexpr float RegainSightDistanceCm = 7000.f;
	constexpr float CooldownBreakDistanceCm = 5500.f;
	constexpr float EvadeFillSeconds = 3.5f;
	constexpr float HidingSpeedKmh = 30.f;
	constexpr float HidingCooldownMultiplier = 1.6f;
	constexpr float CooldownSpotMultiplier = 3.5f;

	constexpr int32 BountyPerSecondPerHeat = 120;
	constexpr int32 BountyPerDisabledPerHeat = 5000;
}

FBNHeatTierSpec FBNHeatTierSpec::ForLevel(int32 HeatLevel)
{
	const int32 Level = FMath::Clamp(HeatLevel, 1, 11);
	FBNHeatTierSpec Spec;
	Spec.Level = Level;
	Spec.bRoadblocksEnabled = Level >= 3;
	Spec.bDemirInvolved = Level >= 6;

	switch (Level)
	{
	case 1:
		Spec.Label = TEXT("HEAT 1");
		Spec.MaxActiveUnits = 1;
		Spec.UnitSkill = 0.60f;
		Spec.MaxPursuitSpeedKmh = 175.f;
		Spec.Aggression = 0.35f;
		Spec.CooldownSeconds = 12.f;
		Spec.HeatThresholdSeconds = 0.f;
		Spec.CollateralThresholdDollars = 0;
		break;
	case 2:
		Spec.Label = TEXT("HEAT 2");
		Spec.MaxActiveUnits = 2;
		Spec.UnitSkill = 0.70f;
		Spec.MaxPursuitSpeedKmh = 192.f;
		Spec.Aggression = 0.48f;
		Spec.CooldownSeconds = 14.f;
		Spec.HeatThresholdSeconds = 25.f;
		Spec.CollateralThresholdDollars = 20000;
		break;
	case 3:
		Spec.Label = TEXT("HEAT 3");
		Spec.MaxActiveUnits = 3;
		Spec.UnitSkill = 0.78f;
		Spec.MaxPursuitSpeedKmh = 208.f;
		Spec.Aggression = 0.62f;
		Spec.CooldownSeconds = 16.f;
		Spec.HeatThresholdSeconds = 55.f;
		Spec.CollateralThresholdDollars = 55000;
		break;
	case 4:
		Spec.Label = TEXT("HEAT 4");
		Spec.MaxActiveUnits = 4;
		Spec.UnitSkill = 0.86f;
		Spec.MaxPursuitSpeedKmh = 222.f;
		Spec.Aggression = 0.76f;
		Spec.CooldownSeconds = 18.f;
		Spec.HeatThresholdSeconds = 95.f;
		Spec.CollateralThresholdDollars = 100000;
		break;
	case 5:
		Spec.Label = TEXT("HEAT 5");
		Spec.MaxActiveUnits = 5;
		Spec.UnitSkill = 0.92f;
		Spec.MaxPursuitSpeedKmh = 236.f;
		Spec.Aggression = 0.88f;
		Spec.CooldownSeconds = 21.f;
		Spec.HeatThresholdSeconds = 140.f;
		Spec.CollateralThresholdDollars = 160000;
		break;
	case 6:
		Spec.Label = TEXT("HEAT 6 - DEMIR");
		Spec.MaxActiveUnits = 5;
		Spec.UnitSkill = 0.95f;
		Spec.MaxPursuitSpeedKmh = 245.f;
		Spec.Aggression = 0.92f;
		Spec.CooldownSeconds = 23.f;
		Spec.HeatThresholdSeconds = 190.f;
		Spec.CollateralThresholdDollars = 250000;
		break;
	case 7:
	case 8:
	case 9:
	case 10:
	{
		const int32 Step = Level - 6;
		Spec.Label = FString::Printf(TEXT("HEAT %d - DEMIR"), Level);
		Spec.MaxActiveUnits = Level >= 9 ? 6 : 5;
		Spec.UnitSkill = FMath::Clamp(0.95f + Step * 0.012f, 0.f, 1.f);
		Spec.MaxPursuitSpeedKmh = 245.f + Step * 3.f;
		Spec.Aggression = FMath::Clamp(0.92f + Step * 0.02f, 0.f, 1.f);
		Spec.CooldownSeconds = 23.f + Step * 0.75f;
		Spec.HeatThresholdSeconds = 190.f + Step * 45.f;
		Spec.CollateralThresholdDollars = 250000 + Step * 75000;
		break;
	}
	default: // 11 = Special Tier
		Spec.Label = TEXT("OZEL KADEME");
		Spec.MaxActiveUnits = 6;
		Spec.UnitSkill = 1.0f;
		Spec.MaxPursuitSpeedKmh = 260.f;
		Spec.Aggression = 1.0f;
		Spec.CooldownSeconds = 26.f;
		Spec.HeatThresholdSeconds = 400.f;
		Spec.CollateralThresholdDollars = 600000;
		break;
	}
	return Spec;
}

void FBNPursuitState::StartPursuit(int32 InitialHeat)
{
	Phase = EBNPursuitPhase::Pursuit;
	HeatLevel = FMath::Clamp(InitialHeat, 1, MaxAllowedHeatLevel);
	PursuitTimeSeconds = 0.f;
	PhaseProgress = 0.f;
	BustMeter = 0.f;
	EvadeMeter = 0.f;
	PursuitCostDollars = 0;
	UnitsDisabled = 0;
	BreakersTriggered = 0;
	Bounty = 0;

	if (HeatLevel >= 11)
	{
		PostDispatch(TEXT("BAŞKOMİSER DEMİR"), TEXT("ONU BANA GETİRİN! ÖLÜ YA DA DİRİ! Tüm birimler Kuzey Viyadüğü ve Liman'ı kapatsın!"), 8.0f);
	}
	else if (HeatLevel >= 6)
	{
		PostDispatch(TEXT("BAŞKOMİSER DEMİR"), TEXT("Kerem Yalçın tespit edildi! Canlı yakalayın, tüm birimler yakınsın!"));
	}
	else
	{
		PostDispatch(TEXT("MERKEZ"), TEXT("Kayıtsız spor araç K. Vance tespit edildi, takip başlıyor."));
	}
}

void FBNPursuitState::SetMaxAllowedHeat(int32 MaxHeat)
{
	MaxAllowedHeatLevel = FMath::Clamp(MaxHeat, 1, 11);
	if (IsPursuitRunning())
	{
		HeatLevel = FMath::Min(HeatLevel, MaxAllowedHeatLevel);
	}
}

bool FBNPursuitState::IsPursuitRunning() const
{
	return Phase == EBNPursuitPhase::Pursuit || Phase == EBNPursuitPhase::Evade || Phase == EBNPursuitPhase::Cooldown;
}

void FBNPursuitState::PostDispatch(const FString& Speaker, const FString& Message, float Duration)
{
	ActiveRadio.Speaker = Speaker;
	ActiveRadio.Message = Message;
	ActiveRadio.RemainingSeconds = FMath::Max(Duration, 0.f);
	++DispatchSerial;
}

void FBNPursuitState::EndPursuitImmediate()
{
	Phase = EBNPursuitPhase::None;
	PhaseProgress = 0.f;
	BustMeter = 0.f;
	EvadeMeter = 0.f;
	bInsideCooldownSpot = false;
	ActiveCooldownSpotName.Reset();
	ActiveRadio = FBNDispatchLine();
}

void FBNPursuitState::TryEscalateHeat()
{
	while (HeatLevel < MaxAllowedHeatLevel)
	{
		const FBNHeatTierSpec Next = FBNHeatTierSpec::ForLevel(HeatLevel + 1);
		if (PursuitTimeSeconds >= Next.HeatThresholdSeconds || PursuitCostDollars >= Next.CollateralThresholdDollars)
		{
			++HeatLevel;
			if (Next.bDemirInvolved)
			{
				PostDispatch(TEXT("BASKOMISER DEMIR"),
					FString::Printf(TEXT("%s! Kordonu daraltin, kacmasina izin vermeyin!"), *Next.Label));
			}
			else if (Next.bRoadblocksEnabled && HeatLevel == 3)
			{
				PostDispatch(TEXT("MERKEZ"), TEXT("Heat 3 aktif! K. Vance icin yol barikatlari kurulsun."));
			}
			else
			{
				PostDispatch(TEXT("MERKEZ"),
					FString::Printf(TEXT("Takip %s seviyesine cikti! Hizli takip birimleri sevk edildi."), *Next.Label));
			}
		}
		else
		{
			break;
		}
	}
}

void FBNPursuitState::RecordCollateralDamage(int32 Dollars, const TCHAR* ReasonTag)
{
	if (Dollars <= 0)
	{
		return;
	}
	CostToStateDollars += Dollars;
	if (IsPursuitRunning())
	{
		PursuitCostDollars += Dollars;
		if (Phase == EBNPursuitPhase::Pursuit)
		{
			TryEscalateHeat();
		}
	}
	if (ReasonTag && *ReasonTag && ActiveRadio.RemainingSeconds <= 0.5f)
	{
		PostDispatch(TEXT("MERKEZ"),
			FString::Printf(TEXT("Hasar tutanagi (%s): +$%d sehir faturasina eklendi."), ReasonTag, Dollars), 3.5f);
	}
}

void FBNPursuitState::RecordUnitDisabled(int32 BonusDollars)
{
	++UnitsDisabled;
	Bounty += HeatLevel * BountyPerDisabledPerHeat;
	RecordCollateralDamage(BonusDollars);
	PostDispatch(TEXT("BIRIM"), TEXT("Devriye araci devre disi kaldi! Takviye ekip isteniyor!"), 3.5f);
}

void FBNPursuitState::RecordPursuitBreaker(int32 CollateralDollars, int32 DisabledCount)
{
	++BreakersTriggered;
	for (int32 i = 0; i < DisabledCount; ++i)
	{
		++UnitsDisabled;
		Bounty += HeatLevel * BountyPerDisabledPerHeat;
	}
	RecordCollateralDamage(CollateralDollars);
	PostDispatch(TEXT("BIRIM-04"),
		TEXT("Supheli kamu yapisini yikti! Sehir hasari artiyor, destek ekip sevk edin!"), 4.5f);
}

void FBNPursuitState::Update(float DeltaSeconds, float PlayerSpeedKmh, float NearestActiveUnitDistCm, int32 ActiveUnitCount, bool bInCooldownSpot)
{
	const float Dt = FMath::Max(DeltaSeconds, 0.f);
	bInsideCooldownSpot = bInCooldownSpot && Phase == EBNPursuitPhase::Cooldown;
	if (ActiveRadio.RemainingSeconds > 0.f)
	{
		ActiveRadio.RemainingSeconds = FMath::Max(0.f, ActiveRadio.RemainingSeconds - Dt);
	}

	if (Phase == EBNPursuitPhase::Busted || Phase == EBNPursuitPhase::Escaped)
	{
		PhaseProgress -= Dt;
		if (PhaseProgress <= 0.f)
		{
			Phase = EBNPursuitPhase::None;
			PhaseProgress = 0.f;
		}
		return;
	}
	if (Phase == EBNPursuitPhase::None)
	{
		return;
	}

	const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(HeatLevel);
	const bool bAnyUnitNear = ActiveUnitCount > 0;

	switch (Phase)
	{
	case EBNPursuitPhase::Pursuit:
		PursuitTimeSeconds += Dt;
		Bounty += FMath::RoundToInt(Dt * HeatLevel * BountyPerSecondPerHeat);
		TryEscalateHeat();

		if (bAnyUnitNear && NearestActiveUnitDistCm <= BustRadiusCm && FMath::Abs(PlayerSpeedKmh) < BustMaxSpeedKmh)
		{
			BustMeter = FMath::Min(1.f, BustMeter + Dt / BustFillSeconds);
			if (BustMeter >= 1.f)
			{
				Phase = EBNPursuitPhase::Busted;
				PhaseProgress = ResultBannerSeconds;
				PostDispatch(TEXT("MERKEZ"), TEXT("Supheli durduruldu. K. Vance gozaltinda."), ResultBannerSeconds);
				return;
			}
		}
		else
		{
			BustMeter = FMath::Max(0.f, BustMeter - Dt * BustDecayPerSecond);
		}

		if (!bAnyUnitNear || NearestActiveUnitDistCm > BreakSightDistanceCm)
		{
			Phase = EBNPursuitPhase::Evade;
			EvadeMeter = 0.f;
			PostDispatch(TEXT("BIRIM"), TEXT("Gorsel temas zayifladi! Birimler supheliyi kaybetmek uzere."));
		}
		break;

	case EBNPursuitPhase::Evade:
		BustMeter = FMath::Max(0.f, BustMeter - Dt * BustDecayPerSecond);
		if (bAnyUnitNear && NearestActiveUnitDistCm < RegainSightDistanceCm)
		{
			Phase = EBNPursuitPhase::Pursuit;
			EvadeMeter = 0.f;
			PostDispatch(TEXT("BIRIM"), TEXT("Gorsel temas yeniden saglandi! Takip suruyor."));
		}
		else
		{
			EvadeMeter = FMath::Min(1.f, EvadeMeter + Dt / EvadeFillSeconds);
			if (EvadeMeter >= 1.f)
			{
				Phase = EBNPursuitPhase::Cooldown;
				PhaseProgress = 0.f;
				EvadeMeter = 0.f;
				PostDispatch(TEXT("MERKEZ"), TEXT("K. Vance gozden kayboldu! Soguma basladi, ara sokaklari tarayin."));
			}
		}
		break;

	case EBNPursuitPhase::Cooldown:
		BustMeter = 0.f;
		if (bAnyUnitNear && NearestActiveUnitDistCm < CooldownBreakDistanceCm)
		{
			Phase = EBNPursuitPhase::Pursuit;
			PhaseProgress = 0.f;
			PostDispatch(TEXT("BIRIM"), TEXT("Supheli tekrar goruldu! Tum birimler yonelsin!"));
		}
		else
		{
			const bool bSlowOrStopped = FMath::Abs(PlayerSpeedKmh) < HidingSpeedKmh;
			const float HideFactor = (bInCooldownSpot && bSlowOrStopped)
				? CooldownSpotMultiplier
				: (bSlowOrStopped ? HidingCooldownMultiplier : 1.0f);
			PhaseProgress = FMath::Min(1.f, PhaseProgress + (Dt * HideFactor) / FMath::Max(Spec.CooldownSeconds, 1.f));
			if (PhaseProgress >= 1.f)
			{
				Phase = EBNPursuitPhase::Escaped;
				PhaseProgress = ResultBannerSeconds;
				PostDispatch(TEXT("MERKEZ"),
					TEXT("Şüpheli izini kaybettirdi. Şehir hasar tutanağı merkeze aktarıldı."), ResultBannerSeconds);
			}
		}
		break;

	default:
		break;
	}
}
