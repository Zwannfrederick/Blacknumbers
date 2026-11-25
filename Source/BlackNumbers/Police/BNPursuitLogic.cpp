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
	constexpr int32 BountyPerInfractionPerHeat = 750;
	constexpr int32 BountyPerDodge = 2500;
	constexpr float SpeedingKmh = 150.f;
	constexpr float ResistingArrestSeconds = 30.f;
	/** Scanner silence (s) before an ambient line is posted, and how long that line stays up. */
	constexpr float AmbientChatterIdleSeconds = 4.5f;
	constexpr float AmbientChatterSeconds = 3.6f;

	struct FChatterLine
	{
		const TCHAR* Speaker;
		const TCHAR* Message;
	};

	const FChatterLine PursuitChatter[] = {
		{ TEXT("BİRİM-07"), TEXT("Şüpheli hâlâ önümüzde, görsel temas devam ediyor.") },
		{ TEXT("MERKEZ"), TEXT("Tüm birimler, şüpheliye yakın durun. Sivil trafiğe dikkat.") },
		{ TEXT("BİRİM-12"), TEXT("Kırmızı ışıkta geçti! Takipteyiz, destek istiyoruz.") },
		{ TEXT("MERKEZ"), TEXT("Kavşakları kapatın, şüpheliyi ana yola sürün.") },
		{ TEXT("BİRİM-03"), TEXT("Yanından geçmeye çalışıyorum, sıkıştırmaya hazırım.") },
		{ TEXT("MERKEZ"), TEXT("Plaka okunamıyor. Araç tarifini tüm ekiplere geçin.") },
	};
	const FChatterLine HighSpeedChatter[] = {
		{ TEXT("BİRİM-07"), TEXT("Şüpheli 180 üstünde! Mesafe açılıyor!") },
		{ TEXT("MERKEZ"), TEXT("Bu hızda temas yok, sadece takip edin. Önünü kesecek ekip arıyoruz.") },
		{ TEXT("BİRİM-12"), TEXT("Çok hızlı! Motoru bizimkilerden güçlü!") },
	};
	const FChatterLine EvadeChatter[] = {
		{ TEXT("BİRİM-03"), TEXT("Kaybettik! Son görüldüğü yeri bildirin!") },
		{ TEXT("MERKEZ"), TEXT("Tüm birimler son bilinen konuma. Gözünüzü dört açın.") },
	};
	const FChatterLine CooldownChatter[] = {
		{ TEXT("MERKEZ"), TEXT("Ara sokakları ve otoparkları tarayın. Uzağa gitmiş olamaz.") },
		{ TEXT("BİRİM-07"), TEXT("Bu bölgede iz yok, bir sonraki kavşağa geçiyorum.") },
		{ TEXT("BİRİM-12"), TEXT("Köprü tarafında sessiz. Liman yönünü kontrol edin.") },
	};

	template <int32 N>
	const FChatterLine& PickLine(const FChatterLine (&Lines)[N], int32 Index)
	{
		return Lines[((Index % N) + N) % N];
	}
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
	Infractions = 0;
	TopSpeedKmh = 0.f;
	RoadblocksDodged = 0;
	SpikeStripsDodged = 0;
	RadioIdleSeconds = 0.f;

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
	RadioIdleSeconds = 0.f;
	++DispatchSerial;
}

FString FBNPursuitState::InfractionLabel(EBNInfraction Infraction)
{
	switch (Infraction)
	{
	case EBNInfraction::Speeding: return TEXT("Aşırı hız");
	case EBNInfraction::Reckless: return TEXT("Tehlikeli sürüş");
	case EBNInfraction::RammedPolice: return TEXT("Polis aracına çarpma");
	case EBNInfraction::PropertyDamage: return TEXT("Kamu malına zarar");
	case EBNInfraction::ResistingArrest: return TEXT("Polise direnme");
	case EBNInfraction::Evading: return TEXT("Polisten kaçma");
	}
	return FString();
}

void FBNPursuitState::AddInfraction(EBNInfraction Infraction)
{
	if (HasInfraction(Infraction))
	{
		return;
	}
	Infractions |= static_cast<uint8>(Infraction);
	Bounty += HeatLevel * BountyPerInfractionPerHeat;
}

void FBNPursuitState::RecordRoadblockDodged()
{
	++RoadblocksDodged;
	Bounty += BountyPerDodge * HeatLevel;
	PostDispatch(TEXT("BARİKAT"), TEXT("Barikatı aştı! Tekrarlıyorum, barikatı aştı!"), 3.5f);
}

void FBNPursuitState::RecordSpikeStripDodged()
{
	++SpikeStripsDodged;
	Bounty += BountyPerDodge * HeatLevel;
	PostDispatch(TEXT("BİRİM"), TEXT("Dikenli şeridi kaçırdı! Bir sonrakini hazırlayın!"), 3.5f);
}

void FBNPursuitState::PostAmbientChatter(float PlayerSpeedKmh)
{
	const int32 Index = AmbientChatterIndex++;
	const FChatterLine* Line = nullptr;
	switch (Phase)
	{
	case EBNPursuitPhase::Evade: Line = &PickLine(EvadeChatter, Index); break;
	case EBNPursuitPhase::Cooldown: Line = &PickLine(CooldownChatter, Index); break;
	default:
		Line = FMath::Abs(PlayerSpeedKmh) >= 180.f ? &PickLine(HighSpeedChatter, Index) : &PickLine(PursuitChatter, Index);
		break;
	}
	PostDispatch(Line->Speaker, Line->Message, AmbientChatterSeconds);
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
				PostDispatch(TEXT("BAŞKOMİSER DEMİR"),
					FString::Printf(TEXT("%s! Kordonu daraltın, kaçmasına izin vermeyin!"), *Next.Label));
			}
			else if (Next.bRoadblocksEnabled && HeatLevel == 3)
			{
				PostDispatch(TEXT("MERKEZ"), TEXT("Heat 3 aktif! K. Vance için yol barikatları kurulsun."));
			}
			else
			{
				PostDispatch(TEXT("MERKEZ"),
					FString::Printf(TEXT("Takip %s seviyesine çıktı! Hızlı takip birimleri sevk edildi."), *Next.Label));
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
	if (IsPursuitRunning())
	{
		AddInfraction(EBNInfraction::Reckless);
	}
	if (ReasonTag && *ReasonTag && ActiveRadio.RemainingSeconds <= 0.5f)
	{
		PostDispatch(TEXT("MERKEZ"),
			FString::Printf(TEXT("Hasar tutanağı (%s): +$%d şehir faturasına eklendi."), ReasonTag, Dollars), 3.5f);
	}
}

void FBNPursuitState::RecordUnitDisabled(int32 BonusDollars)
{
	++UnitsDisabled;
	Bounty += HeatLevel * BountyPerDisabledPerHeat;
	AddInfraction(EBNInfraction::RammedPolice);
	RecordCollateralDamage(BonusDollars);
	PostDispatch(TEXT("BİRİM"), TEXT("Devriye aracı devre dışı kaldı! Takviye ekip isteniyor!"), 3.5f);
}

void FBNPursuitState::RecordPursuitBreaker(int32 CollateralDollars, int32 DisabledCount)
{
	++BreakersTriggered;
	AddInfraction(EBNInfraction::PropertyDamage);
	for (int32 i = 0; i < DisabledCount; ++i)
	{
		++UnitsDisabled;
		Bounty += HeatLevel * BountyPerDisabledPerHeat;
	}
	RecordCollateralDamage(CollateralDollars);
	PostDispatch(TEXT("BİRİM-04"),
		TEXT("Şüpheli kamu yapısını yıktı! Şehir hasarı artıyor, destek ekip sevk edin!"), 4.5f);
}

void FBNPursuitState::Update(float DeltaSeconds, float PlayerSpeedKmh, float NearestActiveUnitDistCm, int32 ActiveUnitCount,
	bool bInCooldownSpot, bool bAirUnitHasSight)
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
	TopSpeedKmh = FMath::Max(TopSpeedKmh, FMath::Abs(PlayerSpeedKmh));
	if (ActiveRadio.RemainingSeconds <= 0.f)
	{
		RadioIdleSeconds += Dt;
		if (RadioIdleSeconds >= AmbientChatterIdleSeconds)
		{
			PostAmbientChatter(PlayerSpeedKmh);
		}
	}

	switch (Phase)
	{
	case EBNPursuitPhase::Pursuit:
		PursuitTimeSeconds += Dt;
		Bounty += FMath::RoundToInt(Dt * HeatLevel * BountyPerSecondPerHeat);
		if (FMath::Abs(PlayerSpeedKmh) >= SpeedingKmh)
		{
			AddInfraction(EBNInfraction::Speeding);
		}
		if (PursuitTimeSeconds >= ResistingArrestSeconds)
		{
			AddInfraction(EBNInfraction::ResistingArrest);
		}
		TryEscalateHeat();

		if (bAnyUnitNear && NearestActiveUnitDistCm <= BustRadiusCm && FMath::Abs(PlayerSpeedKmh) < BustMaxSpeedKmh)
		{
			BustMeter = FMath::Min(1.f, BustMeter + Dt / BustFillSeconds);
			if (BustMeter >= 1.f)
			{
				Phase = EBNPursuitPhase::Busted;
				PhaseProgress = ResultBannerSeconds;
				PostDispatch(TEXT("MERKEZ"), TEXT("Şüpheli durduruldu. K. Vance gözaltında."), ResultBannerSeconds);
				return;
			}
		}
		else
		{
			BustMeter = FMath::Max(0.f, BustMeter - Dt * BustDecayPerSecond);
		}

		if (!bAirUnitHasSight && (!bAnyUnitNear || NearestActiveUnitDistCm > BreakSightDistanceCm))
		{
			Phase = EBNPursuitPhase::Evade;
			EvadeMeter = 0.f;
			PostDispatch(TEXT("BİRİM"), TEXT("Görsel temas zayıfladı! Birimler şüpheliyi kaybetmek üzere."));
		}
		break;

	case EBNPursuitPhase::Evade:
		BustMeter = FMath::Max(0.f, BustMeter - Dt * BustDecayPerSecond);
		if (bAirUnitHasSight || (bAnyUnitNear && NearestActiveUnitDistCm < RegainSightDistanceCm))
		{
			Phase = EBNPursuitPhase::Pursuit;
			EvadeMeter = 0.f;
			PostDispatch(TEXT("BİRİM"), TEXT("Görsel temas yeniden sağlandı! Takip sürüyor."));
		}
		else
		{
			EvadeMeter = FMath::Min(1.f, EvadeMeter + Dt / EvadeFillSeconds);
			if (EvadeMeter >= 1.f)
			{
				Phase = EBNPursuitPhase::Cooldown;
				PhaseProgress = 0.f;
				EvadeMeter = 0.f;
				PostDispatch(TEXT("MERKEZ"), TEXT("K. Vance gözden kayboldu! Soğuma başladı, ara sokakları tarayın."));
			}
		}
		break;

	case EBNPursuitPhase::Cooldown:
		BustMeter = 0.f;
		if (bAirUnitHasSight || (bAnyUnitNear && NearestActiveUnitDistCm < CooldownBreakDistanceCm))
		{
			Phase = EBNPursuitPhase::Pursuit;
			PhaseProgress = 0.f;
			PostDispatch(TEXT("BİRİM"), TEXT("Şüpheli tekrar görüldü! Tüm birimler yönelsin!"));
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
				AddInfraction(EBNInfraction::Evading);
				PostDispatch(TEXT("MERKEZ"),
					TEXT("Şüpheli izini kaybettirdi. Şehir hasar tutanağı merkeze aktarıldı."), ResultBannerSeconds);
			}
		}
		break;

	default:
		break;
	}
}
