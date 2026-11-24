#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/BNStoryTypes.h"
#include "BNStorySubsystem.generated.h"

class ABNVehiclePawn;
class UBNSaveGame;

/** Non-blocking live crew/rival radio or phone message banner shown during gameplay. */
struct FBNLiveComms
{
	FString Speaker;
	FString Message;
	FLinearColor AccentColor = FLinearColor(0.95f, 0.72f, 0.16f, 1.f);
	float RemainingSeconds = 0.f;
};

/**
 * Owns BLACK NUMBERS campaign progress, 16-member boss roster dossier, R1-R8 evidence ledger,
 * Tarik reconciliation + Tier 6 gating, Heat 6+ unlock sync, Perde 5 2-route Special Tier escape,
 * and Canvas Storyboard Card queue.
 */
UCLASS()
class BLACKNUMBERS_API UBNStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float EscapeRadiusCm = 2200.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FBNStoryState& GetState() const { return State; }

	/** Syncs Heat cap (5/10/11) and Tier 6 unlock into World/Progression subsystems cleanly. */
	void SyncWorldSystems(UWorld* World) const;

	/** Called on interactive free-roam start to display C03/C04 once per save/campaign. */
	void EnsureIntroShown(ABNVehiclePawn* PlayerCar);

	UFUNCTION(BlueprintCallable, Category = "Story")
	void ResetStory(bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Story")
	void SkipToRank(int32 TargetRank, UWorld* World = nullptr);

	/** Called by ABNRaceEvent::Finish when a boss race ends (1v1 Sprint/Circuit or Drift target). */
	void OnBossRaceFinished(int32 BossRank, bool bPlayerWon, ABNVehiclePawn* PlayerCar);

	/** Starts Perde 5 Special Tier chase ("ONU BANA GETIRIN! OLU YA DA DIRI!") cleanly. */
	UFUNCTION(BlueprintCallable, Category = "Story")
	bool StartFinalChase(UWorld* World = nullptr);

	/** Checks 2-route exit corridors during Perde 5 and triggers Epilogue + real Sehir Faturasi on escape. */
	bool CheckFinalChaseEscape(ABNVehiclePawn* PlayerCar);

	static FVector GetEscapeRouteLocation(const UWorld* World, int32 RouteIndex);
	static float GetDistanceToEscapeRouteCm(const ABNVehiclePawn* PlayerCar, int32 RouteIndex);

	// SaveGame integration
	void WriteToSaveGame(UBNSaveGame& SaveObj) const;
	void ReadFromSaveGame(const UBNSaveGame& SaveObj);

	// Interactive Canvas Dossier & Storyboard Cards
	bool IsDossierOpen() const { return bDossierOpen; }
	bool HasActiveCard() const { return CardQueue.Num() > 0; }
	bool IsBlockingGameplayInput() const { return bDossierOpen || CardQueue.Num() > 0; }
	const FBNStoryCard* GetActiveCard() const { return CardQueue.Num() > 0 ? &CardQueue[0] : nullptr; }
	int32 GetQueuedCardCount() const { return CardQueue.Num(); }

	int32 GetDossierTab() const { return DossierTab; }
	int32 GetSelectedBossIndex() const { return SelectedBossIndex; }
	const FString& GetStatusFeedback() const { return StatusFeedback; }

	void ToggleDossier(ABNVehiclePawn* PlayerCar);
	void OpenDossier(ABNVehiclePawn* PlayerCar);
	void CloseDossier(ABNVehiclePawn* PlayerCar);
	void MoveDossierSelection(int32 Delta);
	void CycleDossierTab();
	void DismissActiveCard(ABNVehiclePawn* PlayerCar);
	bool ChallengeSelectedBoss(ABNVehiclePawn* PlayerCar);

	// Non-blocking Live Crew/Rival Comms & living world story hooks
	void PostComms(const FString& Speaker, const FString& Message, float DurationSeconds = 5.2f,
		const FLinearColor& AccentColor = FLinearColor(0.95f, 0.72f, 0.16f, 1.f));
	void TickComms(float DeltaSeconds);
	const FBNLiveComms* GetActiveComms() const { return CommsQueue.Num() > 0 ? &CommsQueue[0] : nullptr; }
	void NotifyDistrictEntered(int32 DistrictId);
	void NotifyReputationUpdated(int32 CurrentReputation);
	void NotifyTotaledOrBlown(bool bBlownEngine);
	void NotifyRaceOutcome(bool bWon, bool bDrag, const FString& RaceName);

private:
	void EnqueueCards(const TArray<FBNStoryCard>& Cards, ABNVehiclePawn* PlayerCar);

	FBNStoryState State;
	TArray<FBNStoryCard> CardQueue;
	TArray<FBNLiveComms> CommsQueue;
	uint32 VisitedDistrictMask = 0;
	int32 AnnouncedReadyBossRank = 0;
	bool bDossierOpen = false;
	int32 DossierTab = 0; // 0 = 16'lik Liste, 1 = Kanit Dosyasi
	int32 SelectedBossIndex = 0; // 0..15 maps to Rank 16..1
	FString StatusFeedback;
	bool bPendingHeat6Pursuit = false;
	bool bPendingFinalChaseStart = false;
};
