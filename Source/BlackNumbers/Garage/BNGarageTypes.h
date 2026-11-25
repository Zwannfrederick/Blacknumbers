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
	HyperConcept = 2,
	GrandTourer = 3
};

UENUM(BlueprintType)
enum class EBNPaintFinish : uint8
{
	Gloss = 0,
	Metallic = 1,
	Satin = 2,
	MatteCarbon = 3,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EBNDifficulty : uint8
{
	Easy = 0,
	StreetAutoTier = 1,
	Legend = 2,
	Count UMETA(Hidden)
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
	static const TCHAR* PaintFinishLabel(EBNPaintFinish Finish);
	static const TCHAR* DifficultyLabel(EBNDifficulty Diff);
	static float DifficultySkillScale(EBNDifficulty Diff);
	static const TArray<TPair<FString, FLinearColor>>& GetPaintPalette();
	static const TArray<TPair<FString, FLinearColor>>& GetRimPaintPalette();
	static const TArray<TPair<FString, FLinearColor>>& GetUnderglowPalette();
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
	/** -1 = stock factory paint; 0..9 = custom index into FBNVehicleSpec::GetPaintPalette(). */
	int32 CustomPaintIndices[FBNUpgradeSpec::NumCars] = { -1, -1, -1, -1, -1, -1 };
	EBNPaintFinish PaintFinishes[FBNUpgradeSpec::NumCars] = {
		EBNPaintFinish::Metallic, EBNPaintFinish::Metallic, EBNPaintFinish::Metallic,
		EBNPaintFinish::Gloss, EBNPaintFinish::Metallic, EBNPaintFinish::Metallic
	};
	int32 RimPaintIndices[FBNUpgradeSpec::NumCars] = { 0, 0, 0, 0, 0, 0 };
	int32 UnderglowIndices[FBNUpgradeSpec::NumCars] = { 0, 0, 0, 0, 0, 0 };
	EBNDifficulty Difficulty = EBNDifficulty::StreetAutoTier;

	/** Personal Best records keyed by race asset name or world activity ID. */
	TMap<FString, float> BestRaceTimes;
	TMap<FString, int32> BestDriftScores;
	TMap<FString, float> BestSpeedTrapKmh;
	TMap<FString, float> BestJumpMeters;

	int32 GetMaxAllowedUpgradeTier() const { return bTier6Unlocked ? 6 : 5; }
	int32 GetUpgradeTier(EBNUpgradeSlot Slot) const;
	int32 GetActiveAverageUpgradeTier() const;
	int32 GetActivePaintIndex() const;
	FLinearColor GetActivePaintTint() const;
	void SetActivePaintIndex(int32 PaletteIndex);
	EBNPaintFinish GetActivePaintFinish() const;
	void SetActivePaintFinish(EBNPaintFinish Finish);
	int32 GetActiveRimPaintIndex() const;
	FLinearColor GetActiveRimPaintTint() const;
	void SetActiveRimPaintIndex(int32 RimIndex);
	int32 GetActiveUnderglowIndex() const;
	FLinearColor GetActiveUnderglowColor() const;
	void SetActiveUnderglowIndex(int32 NeonIndex);
	bool IsCarOwned(EBNCarId CarId) const;
	bool CanBuyCar(EBNCarId CarId) const;
	bool BuyOrSelectCar(EBNCarId CarId);
	/** District Ruler pink slip: the beaten rival's car joins the garage for free. False if already owned/invalid. */
	bool AwardPinkSlip(EBNCarId CarId);
	bool CanBuyUpgrade(EBNUpgradeSlot Slot) const;
	bool BuyUpgrade(EBNUpgradeSlot Slot);
	static void EstimateRacePayout(bool bDrift, int32 Place, int32 TotalRacers, int32 DriftPoints, int32& OutCash, int32& OutRep);
	void AwardRaceResult(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints, int32& OutCash, int32& OutRep);
	void AwardPursuitEscape(int32 Bounty, int32 HeatLevel, int32& OutRep);
	bool RecordBestRaceTime(const FString& RaceKey, float FinishSeconds, float* OutPrevBest = nullptr);
	bool RecordBestDriftScore(const FString& RaceKey, int32 Score, int32* OutPrevBest = nullptr);
	bool RecordSpeedTrap(const FString& TrapKey, float SpeedKmh, float& OutPrevBest, int32& OutCash, int32& OutRep);
	bool RecordStuntJump(const FString& JumpKey, float DistanceMeters, float& OutPrevBest, int32& OutCash, int32& OutRep);
	FBNComputedVehicleStats GetActiveStats() const;
};
