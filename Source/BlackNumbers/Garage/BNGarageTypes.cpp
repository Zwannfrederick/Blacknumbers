#include "Garage/BNGarageTypes.h"

static_assert(FBNUpgradeSpec::NumCars == 6, "Keep FBNPlayerProfile::OwnedCars default initializer in sync with EBNCarId.");

FBNVehicleSpec FBNVehicleSpec::ForCar(EBNCarId Id)
{
	FBNVehicleSpec Spec;
	Spec.Id = Id;
	switch (Id)
	{
	case EBNCarId::ValkenRS4:
		Spec.DisplayName = TEXT("Valken RS-4");
		Spec.Subtitle = TEXT("AWD Pist & Viraj");
		Spec.PurchaseCost = 18000;
		Spec.BodyStyle = EBNBodyStyle::StreetCoupe;
		Spec.AeroKit = EBNAeroKit::RallyWing;
		Spec.PaintTint = FLinearColor(0.06f, 0.20f, 0.50f, 1.f);
		Spec.AccentTint = FLinearColor(0.82f, 0.62f, 0.14f, 1.f);
		Spec.Metallic = 0.82f;
		Spec.ClearCoat = 0.90f;
		Spec.BaseTorqueNm = 385.f;
		Spec.BaseMaxRpm = 7600.f;
		Spec.BaseDrag = 0.29f;
		Spec.BaseTireGripScale = 1.08f;
		Spec.BaseDriftThrust = 1020.f;
		Spec.BaseBrakeScale = 1.08f;
		break;

	case EBNCarId::Vandal440R:
		Spec.DisplayName = TEXT("Vandal 440R");
		Spec.Subtitle = TEXT("Ağır V8 Muscle & Drift");
		Spec.PurchaseCost = 32000;
		Spec.BodyStyle = EBNBodyStyle::StreetCoupe;
		Spec.AeroKit = EBNAeroKit::MuscleBlower;
		Spec.PaintTint = FLinearColor(0.50f, 0.09f, 0.05f, 1.f);
		Spec.AccentTint = FLinearColor(0.74f, 0.76f, 0.80f, 1.f);
		Spec.Metallic = 0.78f;
		Spec.ClearCoat = 0.92f;
		Spec.BaseTorqueNm = 455.f;
		Spec.BaseMaxRpm = 6500.f;
		Spec.BaseDrag = 0.34f;
		Spec.BaseTireGripScale = 0.95f;
		Spec.BaseDriftThrust = 1250.f;
		Spec.BaseBrakeScale = 0.96f;
		break;

	case EBNCarId::TauronV6Rally:
		Spec.DisplayName = TEXT("Tauron V6 Rally");
		Spec.Subtitle = TEXT("Ağır Ralli / Barikat Kırıcı");
		Spec.PurchaseCost = 44000;
		Spec.BodyStyle = EBNBodyStyle::RallyOffroad;
		Spec.AeroKit = EBNAeroKit::OffroadRack;
		Spec.PaintTint = FLinearColor(0.68f, 0.34f, 0.06f, 1.f);
		Spec.AccentTint = FLinearColor(0.08f, 0.09f, 0.10f, 1.f);
		Spec.Metallic = 0.45f;
		Spec.ClearCoat = 0.55f;
		Spec.BaseTorqueNm = 485.f;
		Spec.BaseMaxRpm = 6800.f;
		Spec.BaseDrag = 0.35f;
		Spec.BaseTireGripScale = 1.12f;
		Spec.BaseDriftThrust = 1220.f;
		Spec.BaseBrakeScale = 1.05f;
		break;

	case EBNCarId::DracoRST:
		Spec.DisplayName = TEXT("Draco RS-T");
		Spec.Subtitle = TEXT("Widebody Çift Turbo GT");
		Spec.PurchaseCost = 62000;
		Spec.BodyStyle = EBNBodyStyle::GrandTourer;
		Spec.AeroKit = EBNAeroKit::WidebodyGT;
		Spec.PaintTint = FLinearColor(0.05f, 0.36f, 0.34f, 1.f);
		Spec.AccentTint = FLinearColor(0.04f, 0.04f, 0.05f, 1.f);
		Spec.Metallic = 0.88f;
		Spec.ClearCoat = 0.96f;
		Spec.BaseTorqueNm = 475.f;
		Spec.BaseMaxRpm = 8200.f;
		Spec.BaseDrag = 0.27f;
		Spec.BaseTireGripScale = 1.10f;
		Spec.BaseDriftThrust = 1320.f;
		Spec.BaseBrakeScale = 1.12f;
		break;

	case EBNCarId::NocturneBN01:
		Spec.DisplayName = TEXT("Nocturne BN-01");
		Spec.Subtitle = TEXT("Karbon Hiper-GT");
		Spec.PurchaseCost = 95000;
		Spec.BodyStyle = EBNBodyStyle::HyperConcept;
		Spec.AeroKit = EBNAeroKit::HyperCarbon;
		Spec.PaintTint = FLinearColor(0.055f, 0.06f, 0.075f, 1.f);
		Spec.AccentTint = FLinearColor(0.92f, 0.48f, 0.06f, 1.f);
		Spec.Metallic = 0.90f;
		Spec.ClearCoat = 1.0f;
		Spec.BaseTorqueNm = 530.f;
		Spec.BaseMaxRpm = 8600.f;
		Spec.BaseDrag = 0.24f;
		Spec.BaseTireGripScale = 1.18f;
		Spec.BaseDriftThrust = 1360.f;
		Spec.BaseBrakeScale = 1.18f;
		break;

	default: // KuroganeMSpec: exact stock Tier 0 baseline matching existing vehicle handling tests.
		Spec.Id = EBNCarId::KuroganeMSpec;
		Spec.DisplayName = TEXT("Kurogane M-Spec");
		Spec.Subtitle = TEXT("Dengeli Sokak Coupe");
		Spec.PurchaseCost = 0;
		Spec.BodyStyle = EBNBodyStyle::StreetCoupe;
		Spec.AeroKit = EBNAeroKit::StreetLip;
		Spec.PaintTint = FLinearColor(0.22f, 0.24f, 0.27f, 1.f);
		Spec.AccentTint = FLinearColor(0.06f, 0.06f, 0.07f, 1.f);
		Spec.Metallic = 0.72f;
		Spec.ClearCoat = 0.85f;
		Spec.BaseTorqueNm = 400.f;
		Spec.BaseMaxRpm = 7000.f;
		Spec.BaseDrag = 0.31f;
		Spec.BaseTireGripScale = 1.f;
		Spec.BaseDriftThrust = 1100.f;
		Spec.BaseBrakeScale = 1.f;
		break;
	}
	return Spec;
}

const TCHAR* FBNVehicleSpec::BodyStyleLabel(EBNBodyStyle Style)
{
	switch (Style)
	{
	case EBNBodyStyle::RallyOffroad: return TEXT("RALLİ / ARAZİ");
	case EBNBodyStyle::HyperConcept: return TEXT("KARBON HİPER-GT");
	case EBNBodyStyle::GrandTourer:  return TEXT("ÇİFT TURBO GT");
	default: return TEXT("SOKAK COUPE");
	}
}

const TCHAR* FBNVehicleSpec::PaintFinishLabel(EBNPaintFinish Finish)
{
	switch (Finish)
	{
	case EBNPaintFinish::Gloss:       return TEXT("PARLAK");
	case EBNPaintFinish::Satin:       return TEXT("SATEN");
	case EBNPaintFinish::MatteCarbon: return TEXT("MAT KARBON");
	default:                          return TEXT("METALİK");
	}
}

const TCHAR* FBNVehicleSpec::DifficultyLabel(EBNDifficulty Diff)
{
	switch (Diff)
	{
	case EBNDifficulty::Easy:   return TEXT("RAHAT");
	case EBNDifficulty::Legend: return TEXT("EFSANE");
	default:                    return TEXT("SOKAK (OTO-TİER)");
	}
}

float FBNVehicleSpec::DifficultySkillScale(EBNDifficulty Diff)
{
	switch (Diff)
	{
	case EBNDifficulty::Easy:   return 0.86f;
	case EBNDifficulty::Legend: return 1.10f;
	default:                    return 1.00f;
	}
}

const TArray<TPair<FString, FLinearColor>>& FBNVehicleSpec::GetPaintPalette()
{
	static const TArray<TPair<FString, FLinearColor>> Palette = {
		{ TEXT("KARAMARA GRAFİT"),   FLinearColor(0.14f, 0.16f, 0.19f, 1.f) },
		{ TEXT("GECE KOBALT"),       FLinearColor(0.05f, 0.16f, 0.46f, 1.f) },
		{ TEXT("PASLI KEHRİBAR"),    FLinearColor(0.76f, 0.34f, 0.04f, 1.f) },
		{ TEXT("KAN KIRMIZI"),       FLinearColor(0.56f, 0.05f, 0.04f, 1.f) },
		{ TEXT("LİMAN TURKUAZ"),     FLinearColor(0.04f, 0.36f, 0.38f, 1.f) },
		{ TEXT("ALTIN SARI"),        FLinearColor(0.84f, 0.62f, 0.06f, 1.f) },
		{ TEXT("MAT SİYAH"),         FLinearColor(0.03f, 0.03f, 0.035f, 1.f) },
		{ TEXT("İNCİ BEYAZ"),        FLinearColor(0.86f, 0.87f, 0.89f, 1.f) },
		{ TEXT("ZÜMRÜT YEŞİL"),      FLinearColor(0.05f, 0.34f, 0.14f, 1.f) },
		{ TEXT("GECE MORU"),         FLinearColor(0.24f, 0.06f, 0.38f, 1.f) },
	};
	return Palette;
}

const TArray<TPair<FString, FLinearColor>>& FBNVehicleSpec::GetRimPaintPalette()
{
	static const TArray<TPair<FString, FLinearColor>> Rims = {
		{ TEXT("GÜMÜŞ ALAŞIM"), FLinearColor(0.68f, 0.70f, 0.74f, 1.f) },
		{ TEXT("KARBON SİYAH"), FLinearColor(0.06f, 0.065f, 0.075f, 1.f) },
		{ TEXT("BRONZ YARIŞ"),  FLinearColor(0.54f, 0.36f, 0.16f, 1.f) },
		{ TEXT("ALTIN RALLİ"),  FLinearColor(0.82f, 0.62f, 0.14f, 1.f) },
		{ TEXT("BEYAZ PİST"),   FLinearColor(0.86f, 0.86f, 0.88f, 1.f) },
	};
	return Rims;
}

const TArray<TPair<FString, FLinearColor>>& FBNVehicleSpec::GetUnderglowPalette()
{
	static const TArray<TPair<FString, FLinearColor>> Neons = {
		{ TEXT("KAPALI"),           FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("KOBALT NEON"),      FLinearColor(0.10f, 0.62f, 1.00f, 1.f) },
		{ TEXT("KEHRİBAR NEON"),    FLinearColor(1.00f, 0.56f, 0.08f, 1.f) },
		{ TEXT("SİNDİKA KIRMIZI"),  FLinearColor(1.00f, 0.12f, 0.18f, 1.f) },
		{ TEXT("ZÜMRÜT NEON"),      FLinearColor(0.12f, 0.96f, 0.48f, 1.f) },
	};
	return Neons;
}

int32 FBNUpgradeSpec::CostForTier(int32 TargetTier)
{
	switch (TargetTier)
	{
	case 1: return 2500;
	case 2: return 5500;
	case 3: return 10000;
	case 4: return 17500;
	case 5: return 28000;
	case 6: return 55000;
	default: return 0;
	}
}

const TCHAR* FBNUpgradeSpec::SlotLabel(EBNUpgradeSlot Slot)
{
	switch (Slot)
	{
	case EBNUpgradeSlot::Engine: return TEXT("MOTOR");
	case EBNUpgradeSlot::Transmission: return TEXT("ŞANZIMAN");
	case EBNUpgradeSlot::Tires: return TEXT("LASTİK");
	case EBNUpgradeSlot::Chassis: return TEXT("ŞASİ");
	case EBNUpgradeSlot::Nitro: return TEXT("NOS");
	default: return TEXT("?");
	}
}

FBNComputedVehicleStats FBNComputedVehicleStats::Compute(EBNCarId CarId, const int32 (&SlotTiers)[FBNUpgradeSpec::NumSlots])
{
	const FBNVehicleSpec Base = FBNVehicleSpec::ForCar(CarId);
	const int32 EngineTier = FMath::Clamp(SlotTiers[static_cast<int32>(EBNUpgradeSlot::Engine)], 0, 6);
	const int32 TransTier = FMath::Clamp(SlotTiers[static_cast<int32>(EBNUpgradeSlot::Transmission)], 0, 6);
	const int32 TireTier = FMath::Clamp(SlotTiers[static_cast<int32>(EBNUpgradeSlot::Tires)], 0, 6);
	const int32 ChassisTier = FMath::Clamp(SlotTiers[static_cast<int32>(EBNUpgradeSlot::Chassis)], 0, 6);
	const int32 NitroTier = FMath::Clamp(SlotTiers[static_cast<int32>(EBNUpgradeSlot::Nitro)], 0, 6);

	FBNComputedVehicleStats Out;
	Out.MaxTorqueNm = Base.BaseTorqueNm * (1.f + 0.085f * EngineTier);
	Out.MaxRpm = Base.BaseMaxRpm + 200.f * EngineTier;

	Out.GearChangeTime = FMath::Max(0.06f, 0.20f - 0.022f * TransTier);
	Out.TransmissionEfficiency = FMath::Min(0.97f, 0.90f + 0.011f * TransTier);

	Out.TireGripScale = Base.BaseTireGripScale * (1.f + 0.04f * TireTier);
	Out.BrakeScale = Base.BaseBrakeScale * (1.f + 0.03f * TireTier);
	Out.DriftThrust = Base.BaseDriftThrust * (1.f + 0.04f * TireTier);

	Out.DragCoefficient = FMath::Max(0.22f, Base.BaseDrag - 0.013f * ChassisTier);
	Out.DownforceCoefficient = 0.30f + 0.06f * ChassisTier;
	Out.SteeringAngleRatio = FMath::Min(0.92f, 0.70f + 0.032f * ChassisTier);

	Out.NosAccelCmPerSec2 = 750.f + 95.f * NitroTier;
	Out.NosMaxSeconds = 4.5f + 0.45f * NitroTier;
	Out.NosMaxSpeedKmh = 275.f + 10.f * NitroTier;
	return Out;
}

int32 FBNPlayerProfile::GetUpgradeTier(EBNUpgradeSlot Slot) const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	const int32 SlotIdx = FMath::Clamp(static_cast<int32>(Slot), 0, FBNUpgradeSpec::NumSlots - 1);
	return Upgrades[CarIdx][SlotIdx];
}

int32 FBNPlayerProfile::GetActiveAverageUpgradeTier() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	int32 Sum = 0;
	for (int32 s = 0; s < FBNUpgradeSpec::NumSlots; ++s)
	{
		Sum += Upgrades[CarIdx][s];
	}
	return FMath::RoundToInt(static_cast<float>(Sum) / static_cast<float>(FBNUpgradeSpec::NumSlots));
}

int32 FBNPlayerProfile::GetActivePaintIndex() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return CustomPaintIndices[CarIdx];
}

FLinearColor FBNPlayerProfile::GetActivePaintTint() const
{
	const int32 PaintIdx = GetActivePaintIndex();
	const TArray<TPair<FString, FLinearColor>>& Palette = FBNVehicleSpec::GetPaintPalette();
	if (Palette.IsValidIndex(PaintIdx))
	{
		return Palette[PaintIdx].Value;
	}
	return FBNVehicleSpec::ForCar(ActiveCar).PaintTint;
}

void FBNPlayerProfile::SetActivePaintIndex(int32 PaletteIndex)
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	const int32 MaxIdx = FBNVehicleSpec::GetPaintPalette().Num() - 1;
	CustomPaintIndices[CarIdx] = (PaletteIndex < 0) ? -1 : FMath::Clamp(PaletteIndex, 0, MaxIdx);
}

EBNPaintFinish FBNPlayerProfile::GetActivePaintFinish() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return PaintFinishes[CarIdx];
}

void FBNPlayerProfile::SetActivePaintFinish(EBNPaintFinish Finish)
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	PaintFinishes[CarIdx] = Finish;
}

int32 FBNPlayerProfile::GetActiveRimPaintIndex() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return RimPaintIndices[CarIdx];
}

FLinearColor FBNPlayerProfile::GetActiveRimPaintTint() const
{
	const TArray<TPair<FString, FLinearColor>>& Rims = FBNVehicleSpec::GetRimPaintPalette();
	const int32 Idx = GetActiveRimPaintIndex();
	return Rims.IsValidIndex(Idx) ? Rims[Idx].Value : Rims[0].Value;
}

void FBNPlayerProfile::SetActiveRimPaintIndex(int32 RimIndex)
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	const int32 MaxIdx = FBNVehicleSpec::GetRimPaintPalette().Num() - 1;
	RimPaintIndices[CarIdx] = FMath::Clamp(RimIndex, 0, MaxIdx);
}

int32 FBNPlayerProfile::GetActiveUnderglowIndex() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return UnderglowIndices[CarIdx];
}

FLinearColor FBNPlayerProfile::GetActiveUnderglowColor() const
{
	const TArray<TPair<FString, FLinearColor>>& Neons = FBNVehicleSpec::GetUnderglowPalette();
	const int32 Idx = GetActiveUnderglowIndex();
	return Neons.IsValidIndex(Idx) ? Neons[Idx].Value : FLinearColor(0.f, 0.f, 0.f, 0.f);
}

void FBNPlayerProfile::SetActiveUnderglowIndex(int32 NeonIndex)
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	const int32 MaxIdx = FBNVehicleSpec::GetUnderglowPalette().Num() - 1;
	UnderglowIndices[CarIdx] = FMath::Clamp(NeonIndex, 0, MaxIdx);
}

bool FBNPlayerProfile::IsCarOwned(EBNCarId CarId) const
{
	const int32 Idx = static_cast<int32>(CarId);
	return Idx >= 0 && Idx < FBNUpgradeSpec::NumCars && OwnedCars[Idx];
}

bool FBNPlayerProfile::AwardPinkSlip(EBNCarId CarId)
{
	const int32 Idx = static_cast<int32>(CarId);
	if (Idx < 0 || Idx >= FBNUpgradeSpec::NumCars || OwnedCars[Idx])
	{
		return false;
	}
	OwnedCars[Idx] = true;
	return true;
}

bool FBNPlayerProfile::CanBuyCar(EBNCarId CarId) const
{
	if (IsCarOwned(CarId))
	{
		return false;
	}
	return Cash >= FBNVehicleSpec::ForCar(CarId).PurchaseCost;
}

bool FBNPlayerProfile::BuyOrSelectCar(EBNCarId CarId)
{
	const int32 Idx = static_cast<int32>(CarId);
	if (Idx < 0 || Idx >= FBNUpgradeSpec::NumCars)
	{
		return false;
	}
	if (OwnedCars[Idx])
	{
		ActiveCar = CarId;
		return true;
	}
	const int32 Cost = FBNVehicleSpec::ForCar(CarId).PurchaseCost;
	if (Cash < Cost)
	{
		return false;
	}
	Cash -= Cost;
	OwnedCars[Idx] = true;
	ActiveCar = CarId;
	return true;
}

bool FBNPlayerProfile::CanBuyUpgrade(EBNUpgradeSlot Slot) const
{
	const int32 Current = GetUpgradeTier(Slot);
	if (Current >= GetMaxAllowedUpgradeTier())
	{
		return false;
	}
	return Cash >= FBNUpgradeSpec::CostForTier(Current + 1);
}

bool FBNPlayerProfile::BuyUpgrade(EBNUpgradeSlot Slot)
{
	if (!CanBuyUpgrade(Slot))
	{
		return false;
	}
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	const int32 SlotIdx = FMath::Clamp(static_cast<int32>(Slot), 0, FBNUpgradeSpec::NumSlots - 1);
	const int32 NextTier = Upgrades[CarIdx][SlotIdx] + 1;
	Cash -= FBNUpgradeSpec::CostForTier(NextTier);
	Upgrades[CarIdx][SlotIdx] = NextTier;
	return true;
}

void FBNPlayerProfile::EstimateRacePayout(bool bDrift, int32 Place, int32 TotalRacers, int32 DriftPoints, int32& OutCash, int32& OutRep)
{
	if (bDrift)
	{
		OutCash = FMath::Clamp(500 + DriftPoints / 5, 500, 12000);
		OutRep = FMath::Clamp(50 + DriftPoints / 50, 50, 1000);
	}
	else if (TotalRacers <= 1)
	{
		OutCash = 1500;
		OutRep = 120;
	}
	else
	{
		switch (Place)
		{
		case 1: OutCash = 5000; OutRep = 400; break;
		case 2: OutCash = 2500; OutRep = 220; break;
		case 3: OutCash = 1200; OutRep = 120; break;
		default: OutCash = 600; OutRep = 60; break;
		}
	}
}

void FBNPlayerProfile::AwardRaceResult(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints, int32& OutCash, int32& OutRep)
{
	EstimateRacePayout(bDrift, Place, TotalRacers, DriftPoints, OutCash, OutRep);
	if (bDrift)
	{
		if (DriftPoints >= 5000)
		{
			++RacesWon;
		}
	}
	else if (TotalRacers <= 1 || Place == 1)
	{
		++RacesWon;
	}
	++RacesCompleted;
	Cash += OutCash;
	Reputation += OutRep;
}

void FBNPlayerProfile::AwardPursuitEscape(int32 Bounty, int32 HeatLevel, int32& OutRep)
{
	OutRep = FMath::Max(100, HeatLevel * 180 + Bounty / 40);
	Reputation += OutRep;
}

bool FBNPlayerProfile::RecordBestRaceTime(const FString& RaceKey, float FinishSeconds, float* OutPrevBest)
{
	if (RaceKey.IsEmpty() || FinishSeconds <= 0.1f)
	{
		return false;
	}
	const float* Existing = BestRaceTimes.Find(RaceKey);
	if (OutPrevBest)
	{
		*OutPrevBest = Existing ? *Existing : 0.f;
	}
	if (!Existing || FinishSeconds < *Existing)
	{
		BestRaceTimes.Add(RaceKey, FinishSeconds);
		return true;
	}
	return false;
}

bool FBNPlayerProfile::RecordBestDriftScore(const FString& RaceKey, int32 Score, int32* OutPrevBest)
{
	if (RaceKey.IsEmpty() || Score <= 0)
	{
		return false;
	}
	const int32* Existing = BestDriftScores.Find(RaceKey);
	if (OutPrevBest)
	{
		*OutPrevBest = Existing ? *Existing : 0;
	}
	if (!Existing || Score > *Existing)
	{
		BestDriftScores.Add(RaceKey, Score);
		return true;
	}
	return false;
}

bool FBNPlayerProfile::RecordSpeedTrap(const FString& TrapKey, float SpeedKmh, float& OutPrevBest, int32& OutCash, int32& OutRep)
{
	OutPrevBest = 0.f;
	OutCash = 0;
	OutRep = 0;
	if (TrapKey.IsEmpty() || SpeedKmh < 100.f)
	{
		return false;
	}
	const float* Existing = BestSpeedTrapKmh.Find(TrapKey);
	OutPrevBest = Existing ? *Existing : 0.f;
	const bool bNewRecord = !Existing || SpeedKmh > *Existing + 0.5f;
	if (bNewRecord)
	{
		BestSpeedTrapKmh.Add(TrapKey, SpeedKmh);
		OutCash = FMath::Clamp(FMath::RoundToInt(SpeedKmh * 3.5f), 400, 1500);
		OutRep = FMath::Clamp(FMath::RoundToInt(SpeedKmh * 0.30f), 30, 140);
		Cash += OutCash;
		Reputation += OutRep;
	}
	return bNewRecord;
}

bool FBNPlayerProfile::RecordStuntJump(const FString& JumpKey, float DistanceMeters, float& OutPrevBest, int32& OutCash, int32& OutRep)
{
	OutPrevBest = 0.f;
	OutCash = 0;
	OutRep = 0;
	if (JumpKey.IsEmpty() || DistanceMeters < 10.f)
	{
		return false;
	}
	const float* Existing = BestJumpMeters.Find(JumpKey);
	OutPrevBest = Existing ? *Existing : 0.f;
	const bool bNewRecord = !Existing || DistanceMeters > *Existing + 0.5f;
	if (bNewRecord)
	{
		BestJumpMeters.Add(JumpKey, DistanceMeters);
		OutCash = FMath::Clamp(FMath::RoundToInt(DistanceMeters * 14.f), 350, 1800);
		OutRep = FMath::Clamp(FMath::RoundToInt(DistanceMeters * 1.2f), 25, 160);
		Cash += OutCash;
		Reputation += OutRep;
	}
	return bNewRecord;
}

FBNComputedVehicleStats FBNPlayerProfile::GetActiveStats() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return FBNComputedVehicleStats::Compute(ActiveCar, Upgrades[CarIdx]);
}
