#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "BNGarageSubsystem.generated.h"

class ABNVehiclePawn;

/** Serialized SaveGame payload for BLACK NUMBERS player profile. */
UCLASS()
class BLACKNUMBERS_API UBNSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Cash = 4000;

	UPROPERTY()
	int32 Reputation = 0;

	UPROPERTY()
	int32 RacesWon = 0;

	UPROPERTY()
	int32 RacesCompleted = 0;

	UPROPERTY()
	bool bTier6Unlocked = false;

	UPROPERTY()
	int32 ActiveCarIndex = 0;

	UPROPERTY()
	TArray<bool> OwnedCars;

	UPROPERTY()
	TArray<int32> FlatUpgrades;

	UPROPERTY()
	int32 StoryNextBossRank = 16;

	UPROPERTY()
	int32 StoryDefeatedMask = 0;

	UPROPERTY()
	uint8 StoryAct = 0;

	UPROPERTY()
	uint8 StoryTarikAttitude = 0;

	UPROPERTY()
	bool bStoryIntroShown = false;

	UPROPERTY()
	bool bStoryIdentityExposed = false;

	UPROPERTY()
	bool bStorySelinExposed = false;

	UPROPERTY()
	bool bStoryTarikReconciled = false;

	UPROPERTY()
	bool bStoryEvidenceComplete = false;

	UPROPERTY()
	bool bStoryViktorArrested = false;

	UPROPERTY()
	bool bStoryFinalChaseActive = false;

	UPROPERTY()
	bool bStoryCampaignCompleted = false;

	UPROPERTY()
	int32 StoryEscapedRouteIndex = -1;

	UPROPERTY()
	int32 StoryFinalCityDamageDollars = 0;
};

/**
 * Owns cash, reputation, car collection, Tier 1-5/6 upgrades, SaveGame persistence,
 * and Selin'in Garaji interactive menu state across level transitions.
 */
UCLASS()
class BLACKNUMBERS_API UBNProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 GarageRowCount = FBNUpgradeSpec::NumSlots + FBNUpgradeSpec::NumCars;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FBNPlayerProfile& GetProfile() const { return Profile; }

	void ApplyProfileToCar(ABNVehiclePawn& Car) const;

	UFUNCTION(BlueprintCallable, Category = "Garage")
	bool TryBuyOrSelectCar(EBNCarId CarId, ABNVehiclePawn* Car = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	bool TryBuyUpgrade(EBNUpgradeSlot Slot, ABNVehiclePawn* Car = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	void GrantRaceReward(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	void GrantPursuitEscapeReward(int32 Bounty, int32 HeatLevel);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	void AddCash(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	void SetTier6Unlocked(bool bUnlocked);

	UFUNCTION(BlueprintCallable, Category = "Garage")
	void GrantStoryReward(int32 CashGain, int32 RepGain, const FString& Headline);

	const UBNSaveGame* GetLoadedSaveForStory() const { return LoadedSaveSnapshot.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveProfile(const FString& SlotName = TEXT("BlackNumbersSave")) const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadProfile(const FString& SlotName = TEXT("BlackNumbersSave"));

	UFUNCTION(BlueprintCallable, Category = "Save")
	void ResetProfile();

	// Interactive Canvas Garage Menu
	bool IsGarageOpen() const { return bGarageOpen; }
	int32 GetSelectedRow() const { return SelectedRow; }
	const FString& GetFeedbackText() const { return FeedbackText; }
	const FString& GetRewardBanner() const { return RewardBanner; }
	float GetRewardBannerSeconds() const { return RewardBannerRemaining; }

	void TickFeedback(float DeltaSeconds);
	void OpenGarage(ABNVehiclePawn* Car);
	void CloseGarage(ABNVehiclePawn* Car);
	void MoveSelection(int32 Delta);
	void ConfirmSelection(ABNVehiclePawn* Car);

private:
	FBNPlayerProfile Profile;
	bool bGarageOpen = false;
	int32 SelectedRow = 0;
	FString FeedbackText;
	FString RewardBanner;
	float RewardBannerRemaining = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<const UBNSaveGame> LoadedSaveSnapshot;
};
