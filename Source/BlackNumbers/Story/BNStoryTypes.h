#pragma once

#include "CoreMinimal.h"
#include "Garage/BNGarageTypes.h"
#include "Race/BNRaceDefinition.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TArray<FString> UnlockedEvidenceNotes;

	void Reset();
	void RebuildEvidenceNotes();

	bool IsBossDefeated(int32 Rank) const;
	bool CanChallengeBoss(int32 Rank) const;
	int32 GetMaxAllowedHeat() const;
	bool IsTier6Unlocked() const { return bTarikReconciled; }
	int32 GetDefeatedCount() const;

	void BeginStory(TArray<FBNStoryCard>& OutCards);
	bool RecordBossVictory(int32 Rank, TArray<FBNStoryCard>& OutCards);
	bool CompleteFinalChaseEscape(int32 RouteIndex, int32 CityDamageDollars, TArray<FBNStoryCard>& OutCards);
	void SkipToRank(int32 TargetRank);

	FString GetCurrentObjectiveLine() const;

	static const TCHAR* ActLabel(EBNStoryAct InAct);
	static const TCHAR* TarikAttitudeLabel(EBNTarikAttitude Attitude);
	static const TCHAR* EscapeRouteName(int32 RouteIndex);
	static const TArray<FBNBossSpec>& GetAllBosses();
	static const FBNBossSpec* FindBossByRank(int32 Rank);
};
