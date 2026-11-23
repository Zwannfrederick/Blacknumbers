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
		Spec.BodyStyle = EBNBodyStyle::StreetCoupe;
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
	default: return TEXT("SOKAK COUPE");
	}
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
	case EBNUpgradeSlot::Transmission: return TEXT("SANZIMAN");
	case EBNUpgradeSlot::Tires: return TEXT("LASTIK");
	case EBNUpgradeSlot::Chassis: return TEXT("SASI");
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

bool FBNPlayerProfile::IsCarOwned(EBNCarId CarId) const
{
	const int32 Idx = static_cast<int32>(CarId);
	return Idx >= 0 && Idx < FBNUpgradeSpec::NumCars && OwnedCars[Idx];
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

void FBNPlayerProfile::AwardRaceResult(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints, int32& OutCash, int32& OutRep)
{
	if (bDrift)
	{
		OutCash = FMath::Clamp(500 + DriftPoints / 5, 500, 12000);
		OutRep = FMath::Clamp(50 + DriftPoints / 50, 50, 1000);
		if (DriftPoints >= 5000)
		{
			++RacesWon;
		}
	}
	else if (TotalRacers <= 1)
	{
		OutCash = 1500;
		OutRep = 120;
		++RacesWon;
	}
	else
	{
		switch (Place)
		{
		case 1: OutCash = 5000; OutRep = 400; ++RacesWon; break;
		case 2: OutCash = 2500; OutRep = 220; break;
		case 3: OutCash = 1200; OutRep = 120; break;
		default: OutCash = 600; OutRep = 60; break;
		}
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

FBNComputedVehicleStats FBNPlayerProfile::GetActiveStats() const
{
	const int32 CarIdx = FMath::Clamp(static_cast<int32>(ActiveCar), 0, FBNUpgradeSpec::NumCars - 1);
	return FBNComputedVehicleStats::Compute(ActiveCar, Upgrades[CarIdx]);
}
