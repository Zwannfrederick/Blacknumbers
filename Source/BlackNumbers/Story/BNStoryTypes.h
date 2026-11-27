#pragma once

#include "CoreMinimal.h"
#include "Garage/BNGarageTypes.h"
#include "Race/BNRaceDefinition.h"
#include "World/BNRoadGraph.h"
#include "BNStoryTypes.generated.h"

UENUM(BlueprintType)
enum class EBNStoryAct : uint8
{
	Perde1_Donus,
	Perde2_Yukselis,
	Perde3_Ifsa,
	Perde4_Zirve,
	Perde5_SonKovalamaca,
	Epilog,
};

UENUM(BlueprintType)
enum class EBNTarikAttitude : uint8
{
	Hostile,
	Suspicious,
	DeepDoubt,
	Reconciled,
	Retired,
};

enum class EBNRivalStyle : uint8 { Balanced, LateBrake, DefendLine, FastExit };

struct FBNPursuitState;

/** NFS Most Wanted 2005-style Pursuit / Open-World Milestone requirement for Blacklist bosses. */
UENUM(BlueprintType)
enum class EBNMilestoneType : uint8
{
	PursuitDurationSec,
	CopsDisabled,
	CostToState,
	RoadblocksDodged,
	SpikeStripsDodged,
	SpeedTrapKmh,
	PursuitEscapeHeat,
};

USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNBossMilestone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNMilestoneType Type = EBNMilestoneType::PursuitDurationSec;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 TargetValue = 45;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 BonusBounty = 5000;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Description;
};

/** Canonical BLACK NUMBERS syndicate racer (#16..#01). */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNBossSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 Rank = 16;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 TierLayer = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString OrgRole;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Personality;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString DrivingStyle;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString VehicleName;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNRaceDiscipline Discipline = EBNRaceDiscipline::Sprint;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString DisciplineLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString RaceAssetName;

	TArray<FName> Qualifiers;
	EBNRivalStyle RivalStyle = EBNRivalStyle::Balanced;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNCarId BaseCar = EBNCarId::KuroganeMSpec;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString EvidenceCode;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString EvidenceSummary;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 DriftTargetScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 RewardCash = 2000;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 RewardRep = 200;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	float RivalSkill = 0.72f;

	FBNComputedVehicleStats VehicleStats;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FLinearColor VehicleTint = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNDistrict HomeDistrict = EBNDistrict::MerkezBulvar;

	/** NFS Most Wanted 2005 Blacklist 3-gate progression requirements (Races + Milestones + Total Bounty). */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 RequiredBounty = 10000;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 RequiredMilestones = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TArray<FBNBossMilestone> Milestones;

	/** Short stylized street tag word shown in MW Blacklist graffiti stamp. */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString TagWord;

	/** Iconic 2-line rival bio / street taunt shown on the MW Blacklist screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString TauntQuote;
};

/** Most Wanted / Carbon-style borough territory & District Ruler control record. */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNDistrictTerritoryInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNDistrict District = EBNDistrict::MerkezBulvar;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString BoroughTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString BoroughTheme;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString CrewName;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 RulerRank = 16;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString RulerName;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString RulerVehicle;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FLinearColor TerritoryColor = FLinearColor(0.85f, 0.25f, 0.15f, 1.f);

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TArray<int32> CrewRanks;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 DefeatedInDistrict = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bRulerDefeated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bDistrictLiberated = false;
};

/** One storyboard / narrative overlay card rendered cleanly on Canvas HUD. */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNStoryCard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString CardId;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Speaker;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString Body;
};

/**
 * Pure story campaign state machine (Act 1-5 + Epilogue, R1-R8 evidence ledger,
 * Tarik gradual reconciliation + Tier 6 unlock, Heat 6 exposure, and 2-route Special Tier escape).
 * Kept free of UWorld/Actors so it can be unit tested directly.
 */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNStoryState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNStoryAct Act = EBNStoryAct::Perde1_Donus;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EBNTarikAttitude TarikAttitude = EBNTarikAttitude::Hostile;

	/** Next challenger rank (#16 down to #1; 0 once #01 Viktor Serez is defeated). */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 NextBossRank = 16;

	int32 QualifiersWon = 0;

	/** Bitmask (1 << Rank) for defeated BLACK NUMBERS members 1..16. */
	uint32 DefeatedMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIntroShown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIdentityExposed = false; // R4 (#08 Hakan)

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bSelinExposed = false; // R5 (#06 Mirac)

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bTarikReconciled = false; // R6 (#05 Kaan -> Tier 6 unlocked)

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bEvidenceComplete = false; // R8 (#02 Elena -> unlocks Viktor #01 confrontation)

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bViktorArrested = false;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bFinalChaseActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bCampaignCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 EscapedRouteIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 FinalCityDamageDollars = 0;

	/** Career banked Bounty (earned by escaping pursuits & completing Milestones — MW 2005 style!). */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 TotalBounty = 0;

	/** Bitmask (bits 0..2) for completed Milestones on the active NextBossRank (resets per boss). */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 CurrentBossMilestonesMask = 0;

	/** Impound Strikes (0..3) incremented on Busted; resets to 0 after bail/tow strike-out. */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 ImpoundStrikes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 TotalPursuitsEscaped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 TotalBustedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TArray<FString> UnlockedEvidenceNotes;

	void Reset();
	void RebuildEvidenceNotes();

	bool IsBossDefeated(int32 Rank) const;
	bool CanChallengeBoss(int32 Rank) const;
	bool AreRacesComplete(int32 Rank) const;
	bool AreMilestonesComplete(int32 Rank) const;
	bool IsBountyComplete(int32 Rank) const;
	bool IsDuelUnlocked(int32 Rank) const;
	int32 GetCompletedMilestoneCount() const;
	bool IsMilestoneCompleted(int32 Index) const;
	bool CompleteMilestone(int32 Index, int32* OutBonusBounty = nullptr);
	int32 EvaluatePursuitMilestones(const FBNPursuitState& PursuitState, bool bCommit, TArray<FString>* OutNewlyCompleted = nullptr);
	int32 EvaluateSpeedTrapMilestones(float SpeedKmh, TArray<FString>* OutNewlyCompleted = nullptr);
	void BankPursuitBounty(int32 BountyAmount);
	int32 RecordBustedStrike();
	FName GetNextQualifier() const;
	bool RecordQualifierVictory(FName RaceAsset);
	int32 GetQualifierDriftTarget() const;
	int32 GetMaxAllowedHeat() const;
	bool IsTier6Unlocked() const { return bTarikReconciled; }
	int32 GetDefeatedCount() const;

	void BeginStory(TArray<FBNStoryCard>& OutCards);
	bool RecordBossVictory(int32 Rank, TArray<FBNStoryCard>& OutCards);
	bool CompleteFinalChaseEscape(int32 RouteIndex, int32 CityDamageDollars, TArray<FBNStoryCard>& OutCards);
	void SkipToRank(int32 TargetRank);

	FString GetCurrentObjectiveLine() const;
	FBNDistrictTerritoryInfo GetDistrictTerritory(EBNDistrict District) const;
	int32 GetLiberatedDistrictCount() const;

	static EBNDistrict GetDistrictForBoss(int32 Rank);
	static bool IsDistrictRulerRank(int32 Rank);
	static const TCHAR* ActLabel(EBNStoryAct InAct);
	static const TCHAR* TarikAttitudeLabel(EBNTarikAttitude Attitude);
	static const TCHAR* EscapeRouteName(int32 RouteIndex);
	static const TArray<FBNBossSpec>& GetAllBosses();
	static const FBNBossSpec* FindBossByRank(int32 Rank);
};
