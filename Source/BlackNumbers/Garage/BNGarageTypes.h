#pragma once

#include "CoreMinimal.h"
#include "BNGarageTypes.generated.h"

UENUM(BlueprintType)
enum class EBNUpgradeSlot : uint8
{
	Engine = 0,
	Transmission = 1,
	Tires = 2,
	Chassis = 3,
	Nitro = 4,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EBNCarId : uint8
{
	KuroganeMSpec = 0,
	ValkenRS4 = 1,
	Vandal440R = 2,
	TauronV6Rally = 3,
	DracoRST = 4,
	NocturneBN01 = 5,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EBNBodyStyle : uint8
{
	StreetCoupe = 0,
	RallyOffroad = 1,
	HyperConcept = 2
};

UENUM(BlueprintType)
enum class EBNAeroKit : uint8
{
	StreetLip = 0,
	RallyWing = 1,
	MuscleBlower = 2,
	OffroadRack = 3,
	WidebodyGT = 4,
	HyperCarbon = 5
};

/** Base tuning and identity for one fictional Karamara street car. */
struct BLACKNUMBERS_API FBNVehicleSpec
{
	EBNCarId Id = EBNCarId::KuroganeMSpec;
	FString DisplayName;
	FString Subtitle;
	int32 PurchaseCost = 0;
	EBNBodyStyle BodyStyle = EBNBodyStyle::StreetCoupe;
	EBNAeroKit AeroKit = EBNAeroKit::StreetLip;
	FLinearColor PaintTint = FLinearColor::White;
	FLinearColor AccentTint = FLinearColor(0.06f, 0.06f, 0.07f, 1.f);
	float Metallic = 0.72f;
	float ClearCoat = 0.85f;
	float BaseTorqueNm = 400.f;
	float BaseMaxRpm = 7000.f;
	float BaseDrag = 0.31f;
	float BaseTireGripScale = 1.f;
	float BaseDriftThrust = 1100.f;
	float BaseBrakeScale = 1.f;

	static FBNVehicleSpec ForCar(EBNCarId Id);
	static const TCHAR* BodyStyleLabel(EBNBodyStyle Style);
};

struct BLACKNUMBERS_API FBNUpgradeSpec
{
	static constexpr int32 NumSlots = static_cast<int32>(EBNUpgradeSlot::Count);
	static constexpr int32 NumCars = static_cast<int32>(EBNCarId::Count);

	static int32 CostForTier(int32 TargetTier);
	static const TCHAR* SlotLabel(EBNUpgradeSlot Slot);
};

/** Final physics parameters derived from car base spec + installed Tier 0..6 parts. */
struct BLACKNUMBERS_API FBNComputedVehicleStats
{
	float MaxTorqueNm = 400.f;
	float MaxRpm = 7000.f;
	float GearChangeTime = 0.2f;
	float TransmissionEfficiency = 0.9f;
	float TireGripScale = 1.f;
	float BrakeScale = 1.f;
	float DragCoefficient = 0.31f;
	float DownforceCoefficient = 0.3f;
	float SteeringAngleRatio = 0.7f;
	float DriftThrust = 1100.f;
	float NosAccelCmPerSec2 = 750.f;
	float NosMaxSeconds = 4.5f;
	float NosMaxSpeedKmh = 275.f;

	static FBNComputedVehicleStats Compute(EBNCarId CarId, const int32 (&SlotTiers)[FBNUpgradeSpec::NumSlots]);
};

/**
 * Pure player progression state: cash, reputation, owned fictional cars, Tier 1-5 parts,
 * and Kanon #8 Tier 6 gate (locked until Tarik reconciliation R6).
 */
struct BLACKNUMBERS_API FBNPlayerProfile
{
	int32 Cash = 4000;
	int32 Reputation = 0;
	int32 RacesWon = 0;
	int32 RacesCompleted = 0;
	bool bTier6Unlocked = false;
	EBNCarId ActiveCar = EBNCarId::KuroganeMSpec;
	bool OwnedCars[FBNUpgradeSpec::NumCars] = { true, false, false, false, false, false };
	int32 Upgrades[FBNUpgradeSpec::NumCars][FBNUpgradeSpec::NumSlots] = {};

	int32 GetMaxAllowedUpgradeTier() const { return bTier6Unlocked ? 6 : 5; }
	int32 GetUpgradeTier(EBNUpgradeSlot Slot) const;
	bool IsCarOwned(EBNCarId CarId) const;
	bool CanBuyCar(EBNCarId CarId) const;
	bool BuyOrSelectCar(EBNCarId CarId);
	bool CanBuyUpgrade(EBNUpgradeSlot Slot) const;
	bool BuyUpgrade(EBNUpgradeSlot Slot);
	void AwardRaceResult(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints, int32& OutCash, int32& OutRep);
	void AwardPursuitEscape(int32 Bounty, int32 HeatLevel, int32& OutRep);
	FBNComputedVehicleStats GetActiveStats() const;
};
