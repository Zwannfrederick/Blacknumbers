#include "Story/BNStorySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"

namespace
{
	bool IsInteractiveUIEnabled()
	{
		return !FApp::IsUnattended() && !GIsAutomationTesting;
	}

	int32 RankFromIndex(int32 Index)
	{
		return 16 - FMath::Clamp(Index, 0, 15);
	}

	int32 IndexFromRank(int32 Rank)
	{
		return FMath::Clamp(16 - FMath::Clamp(Rank, 1, 16), 0, 15);
	}
}

void UBNStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UBNProgressionSubsystem>();
	Super::Initialize(Collection);

	State.Reset();
	CardQueue.Reset();
	bDossierOpen = false;
	DossierTab = 0;
	SelectedBossIndex = IndexFromRank(State.NextBossRank);
	bPendingHeat6Pursuit = false;
	bPendingFinalChaseStart = false;

	if (const UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		if (const UBNSaveGame* Snap = Prog->GetLoadedSaveForStory())
		{
			ReadFromSaveGame(*Snap);
		}
	}
}

void UBNStorySubsystem::SyncWorldSystems(UWorld* World) const
{
	if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		if (Prog->GetProfile().bTier6Unlocked != State.IsTier6Unlocked())
		{
			Prog->SetTier6Unlocked(State.IsTier6Unlocked());
		}
	}

	UWorld* TargetWorld = World ? World : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	if (TargetWorld)
	{
		if (UBNPursuitSubsystem* Pursuit = TargetWorld->GetSubsystem<UBNPursuitSubsystem>())
		{
			Pursuit->SetMaxAllowedHeat(State.GetMaxAllowedHeat());
		}
	}
}

void UBNStorySubsystem::EnsureIntroShown(ABNVehiclePawn* PlayerCar)
{
	SyncWorldSystems(PlayerCar ? PlayerCar->GetWorld() : nullptr);
	if (!IsInteractiveUIEnabled() || State.bIntroShown)
	{
		return;
	}
	TArray<FBNStoryCard> Cards;
	State.BeginStory(Cards);
	EnqueueCards(Cards, PlayerCar);
}

void UBNStorySubsystem::ResetStory(bool bSave)
{
	State.Reset();
	CardQueue.Reset();
	bDossierOpen = false;
	DossierTab = 0;
	SelectedBossIndex = IndexFromRank(State.NextBossRank);
	StatusFeedback.Reset();
	bPendingHeat6Pursuit = false;
	bPendingFinalChaseStart = false;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
	{
		Pursuit->StopPursuit();
	}
	SyncWorldSystems(World);

	if (bSave && IsInteractiveUIEnabled())
	{
		if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->SaveProfile();
		}
	}
}

void UBNStorySubsystem::SkipToRank(int32 TargetRank, UWorld* World)
{
	State.SkipToRank(TargetRank);
	CardQueue.Reset();
	bDossierOpen = false;
	SelectedBossIndex = IndexFromRank(State.NextBossRank > 0 ? State.NextBossRank : 1);
	bPendingHeat6Pursuit = false;
	bPendingFinalChaseStart = false;
	SyncWorldSystems(World);

	StatusFeedback = FString::Printf(TEXT("Hikaye #%02d asamasina sarildi."), FMath::Clamp(TargetRank, 0, 16));
}

FVector UBNStorySubsystem::GetEscapeRouteLocation(const UWorld* World, int32 RouteIndex)
{
	if (World)
	{
		if (const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>(); RoadGraph && RoadGraph->HasGraph())
		{
			return RoadGraph->GetEscapeRouteLocation(RouteIndex);
		}
	}
	const bool bKaramara = World && World->GetMapName().Contains(TEXT("Karamara"));
	if (RouteIndex == 1) // Kuzey Geçidi Toprak Servis Yolu
	{
		return bKaramara ? FVector(-76000.f, -80000.f, 4800.f) : FVector(-16000.f, -17000.f, 0.f);
	}
	// Route 0: Eski Liman Tünelleri
	return bKaramara ? FVector(86000.f, -12000.f, 0.f) : FVector(26500.f, 7500.f, 0.f);
}

float UBNStorySubsystem::GetDistanceToEscapeRouteCm(const ABNVehiclePawn* PlayerCar, int32 RouteIndex)
{
	if (!PlayerCar)
	{
		return BIG_NUMBER;
	}
	return FVector::Dist2D(PlayerCar->GetActorLocation(), GetEscapeRouteLocation(PlayerCar->GetWorld(), RouteIndex));
}

bool UBNStorySubsystem::StartFinalChase(UWorld* World)
{
	UWorld* TargetWorld = World ? World : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	if (!TargetWorld)
	{
		return false;
	}
	State.bFinalChaseActive = true;
	State.bCampaignCompleted = false;
	State.Act = EBNStoryAct::Perde5_SonKovalamaca;
	bPendingFinalChaseStart = false;
	SyncWorldSystems(TargetWorld);

	if (UBNPursuitSubsystem* Pursuit = TargetWorld->GetSubsystem<UBNPursuitSubsystem>())
	{
		Pursuit->SetMaxAllowedHeat(11);
		Pursuit->StartPursuit(11);
		return true;
	}
	return false;
}

bool UBNStorySubsystem::CheckFinalChaseEscape(ABNVehiclePawn* PlayerCar)
{
	if (!PlayerCar || !State.bFinalChaseActive || State.bCampaignCompleted)
	{
		return false;
	}

	const UWorld* World = PlayerCar->GetWorld();
	const float DistRoute0 = GetDistanceToEscapeRouteCm(PlayerCar, 0);
	const float DistRoute1 = GetDistanceToEscapeRouteCm(PlayerCar, 1);

	int32 EscapedRoute = -1;
	if (DistRoute0 <= EscapeRadiusCm)
	{
		EscapedRoute = 0;
	}
	else if (DistRoute1 <= EscapeRadiusCm)
	{
		EscapedRoute = 1;
	}
	if (EscapedRoute < 0)
	{
		return false;
	}

	int32 CityDamage = 0;
	if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
	{
		CityDamage = Pursuit->GetCostToStateDollars();
		Pursuit->StopPursuit();
	}

	TArray<FBNStoryCard> EpilogCards;
	if (!State.CompleteFinalChaseEscape(EscapedRoute, CityDamage, EpilogCards))
	{
		return false;
	}

	SyncWorldSystems(PlayerCar->GetWorld());
	EnqueueCards(EpilogCards, PlayerCar);

	if (IsInteractiveUIEnabled())
	{
		if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->SaveProfile();
		}
	}
	return true;
}

void UBNStorySubsystem::OnBossRaceFinished(int32 BossRank, bool bPlayerWon, ABNVehiclePawn* PlayerCar)
{
	const FBNBossSpec* Spec = FBNStoryState::FindBossByRank(BossRank);
	if (!Spec)
	{
		return;
	}

	if (!bPlayerWon)
	{
		StatusFeedback = FString::Printf(TEXT("#%02d %s karsisinda kaybettin! [B] Dosya ile tekrar dene."),
			BossRank, *Spec->Name);
		return;
	}

	TArray<FBNStoryCard> NewCards;
	if (!State.RecordBossVictory(BossRank, NewCards))
	{
		return;
	}

	if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		Prog->GrantStoryReward(Spec->RewardCash, Spec->RewardRep,
			FString::Printf(TEXT("BLACK NUMBERS #%02d YENILDI: +$%s   +%d ITIBAR"),
				BossRank, *FString::FormatAsNumber(Spec->RewardCash), Spec->RewardRep));
	}

	UWorld* World = PlayerCar ? PlayerCar->GetWorld() : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	SyncWorldSystems(World);
	SelectedBossIndex = IndexFromRank(State.NextBossRank > 0 ? State.NextBossRank : 1);

	if (BossRank == 8)
	{
		bPendingHeat6Pursuit = true;
	}
	if (BossRank == 1)
	{
		bPendingFinalChaseStart = true;
	}

	if (IsInteractiveUIEnabled())
	{
		EnqueueCards(NewCards, PlayerCar);
		if (CardQueue.Num() == 0 && bPendingFinalChaseStart)
		{
			StartFinalChase(World);
		}
		if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->SaveProfile();
		}
	}
	else if (bPendingFinalChaseStart)
	{
		StartFinalChase(World);
	}
}

void UBNStorySubsystem::EnqueueCards(const TArray<FBNStoryCard>& Cards, ABNVehiclePawn* PlayerCar)
{
	if (!IsInteractiveUIEnabled() || Cards.Num() == 0)
	{
		return;
	}
	CardQueue.Append(Cards);
	if (PlayerCar && CardQueue.Num() > 0)
	{
		PlayerCar->SetInputLocked(true);
	}
}

void UBNStorySubsystem::ToggleDossier(ABNVehiclePawn* PlayerCar)
{
	if (bDossierOpen)
	{
		CloseDossier(PlayerCar);
	}
	else
	{
		OpenDossier(PlayerCar);
	}
}

void UBNStorySubsystem::OpenDossier(ABNVehiclePawn* PlayerCar)
{
	if (HasActiveCard())
	{
		return;
	}
	bDossierOpen = true;
	if (State.NextBossRank >= 1 && State.NextBossRank <= 16)
	{
		SelectedBossIndex = IndexFromRank(State.NextBossRank);
	}
	StatusFeedback = TEXT("W/S ile sec, TAB/A/D ile sekme degistir, ENTER ile boss yarisina cik.");
	if (PlayerCar)
	{
		PlayerCar->SetInputLocked(true);
	}
}

void UBNStorySubsystem::CloseDossier(ABNVehiclePawn* PlayerCar)
{
	bDossierOpen = false;
	if (PlayerCar && !HasActiveCard())
	{
		PlayerCar->SetInputLocked(false);
	}
}

void UBNStorySubsystem::MoveDossierSelection(int32 Delta)
{
	if (DossierTab == 0)
	{
		constexpr int32 Count = 16;
		SelectedBossIndex = (SelectedBossIndex + Delta % Count + Count) % Count;
	}
}

void UBNStorySubsystem::CycleDossierTab()
{
	DossierTab = (DossierTab + 1) % 2;
}

void UBNStorySubsystem::DismissActiveCard(ABNVehiclePawn* PlayerCar)
{
	if (CardQueue.Num() == 0)
	{
		return;
	}
	CardQueue.RemoveAt(0);
	if (CardQueue.Num() > 0)
	{
		return;
	}

	if (PlayerCar)
	{
		PlayerCar->SetInputLocked(false);
	}

	UWorld* World = PlayerCar ? PlayerCar->GetWorld() : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	if (bPendingFinalChaseStart)
	{
		StartFinalChase(World);
		return;
	}
	if (bPendingHeat6Pursuit)
	{
		bPendingHeat6Pursuit = false;
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->SetMaxAllowedHeat(State.GetMaxAllowedHeat());
			Pursuit->StartPursuit(6);
		}
	}
}

bool UBNStorySubsystem::ChallengeSelectedBoss(ABNVehiclePawn* PlayerCar)
{
	if (!PlayerCar || !PlayerCar->GetWorld())
	{
		return false;
	}

	if (State.bCampaignCompleted)
	{
		StatusFeedback = TEXT("Kampanya tamamlandi. Yeniden oynamak icin konsola bn.StoryReset yaz.");
		return false;
	}

	if (State.bFinalChaseActive)
	{
		CloseDossier(PlayerCar);
		StartFinalChase(PlayerCar->GetWorld());
		return true;
	}

	const int32 Rank = RankFromIndex(SelectedBossIndex);
	if (State.IsBossDefeated(Rank))
	{
		StatusFeedback = FString::Printf(TEXT("#%02d zaten yenildi! Siradaki hedef: #%02d."), Rank, State.NextBossRank);
		return false;
	}
	if (!State.CanChallengeBoss(Rank))
	{
		StatusFeedback = FString::Printf(TEXT("#%02d henuz kilitli! Once #%02d yenilmeli."), Rank, State.NextBossRank);
		return false;
	}

	const FBNBossSpec* Spec = FBNStoryState::FindBossByRank(Rank);
	if (!Spec)
	{
		return false;
	}

	ABNRaceEvent* ExactEvent = nullptr;
	ABNRaceEvent* DisciplineEvent = nullptr;
	ABNRaceEvent* FallbackEvent = nullptr;
	for (TActorIterator<ABNRaceEvent> It(PlayerCar->GetWorld()); It; ++It)
	{
		if (It->GetState() == EBNRaceState::Idle)
		{
			if (!FallbackEvent)
			{
				FallbackEvent = *It;
			}
			if (It->Definition)
			{
				if (!Spec->RaceAssetName.IsEmpty() && It->Definition->GetName().Equals(Spec->RaceAssetName, ESearchCase::IgnoreCase))
				{
					ExactEvent = *It;
					break;
				}
				if (!DisciplineEvent && It->Definition->Discipline == Spec->Discipline)
				{
					DisciplineEvent = *It;
				}
			}
		}
	}

	ABNRaceEvent* Chosen = ExactEvent ? ExactEvent : (DisciplineEvent ? DisciplineEvent : FallbackEvent);
	if (!Chosen)
	{
		StatusFeedback = TEXT("Bu haritada uygun boş yarış etkinliği bulunamadı.");
		return false;
	}

	CloseDossier(PlayerCar);
	Chosen->StartBossRace(PlayerCar, Spec->Rank, Spec->RivalSkill, Spec->VehicleStats, Spec->VehicleTint, Spec->DriftTargetScore, Spec->BaseCar);
	return true;
}

void UBNStorySubsystem::WriteToSaveGame(UBNSaveGame& SaveObj) const
{
	SaveObj.StoryNextBossRank = State.NextBossRank;
	SaveObj.StoryDefeatedMask = static_cast<int32>(State.DefeatedMask);
	SaveObj.StoryAct = static_cast<uint8>(State.Act);
	SaveObj.StoryTarikAttitude = static_cast<uint8>(State.TarikAttitude);
	SaveObj.bStoryIntroShown = State.bIntroShown;
	SaveObj.bStoryIdentityExposed = State.bIdentityExposed;
	SaveObj.bStorySelinExposed = State.bSelinExposed;
	SaveObj.bStoryTarikReconciled = State.bTarikReconciled;
	SaveObj.bStoryEvidenceComplete = State.bEvidenceComplete;
	SaveObj.bStoryViktorArrested = State.bViktorArrested;
	SaveObj.bStoryFinalChaseActive = State.bFinalChaseActive;
	SaveObj.bStoryCampaignCompleted = State.bCampaignCompleted;
	SaveObj.StoryEscapedRouteIndex = State.EscapedRouteIndex;
	SaveObj.StoryFinalCityDamageDollars = State.FinalCityDamageDollars;
}

void UBNStorySubsystem::ReadFromSaveGame(const UBNSaveGame& SaveObj)
{
	State.NextBossRank = FMath::Clamp(SaveObj.StoryNextBossRank, 0, 16);
	State.DefeatedMask = static_cast<uint32>(SaveObj.StoryDefeatedMask);
	State.Act = static_cast<EBNStoryAct>(FMath::Clamp<int32>(SaveObj.StoryAct, 0, static_cast<int32>(EBNStoryAct::Epilog)));
	State.TarikAttitude = static_cast<EBNTarikAttitude>(FMath::Clamp<int32>(SaveObj.StoryTarikAttitude, 0, static_cast<int32>(EBNTarikAttitude::Retired)));
	State.bIntroShown = SaveObj.bStoryIntroShown;
	State.bIdentityExposed = SaveObj.bStoryIdentityExposed;
	State.bSelinExposed = SaveObj.bStorySelinExposed;
	State.bTarikReconciled = SaveObj.bStoryTarikReconciled;
	State.bEvidenceComplete = SaveObj.bStoryEvidenceComplete;
	State.bViktorArrested = SaveObj.bStoryViktorArrested;
	State.bFinalChaseActive = SaveObj.bStoryFinalChaseActive;
	State.bCampaignCompleted = SaveObj.bStoryCampaignCompleted;
	State.EscapedRouteIndex = SaveObj.StoryEscapedRouteIndex;
	State.FinalCityDamageDollars = SaveObj.StoryFinalCityDamageDollars;
	State.RebuildEvidenceNotes();

	SelectedBossIndex = IndexFromRank(State.NextBossRank > 0 ? State.NextBossRank : 1);
	SyncWorldSystems(nullptr);
}

static FAutoConsoleCommandWithWorldAndArgs GBNStorySkipToCmd(
	TEXT("bn.StorySkipTo"),
	TEXT("bn.StorySkipTo <1-16 | 0>  Advances story campaign cleanly to boss rank (or 0 for Perde 5)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNStorySubsystem* Story = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			if (Args.Num() == 1 && Args[0].IsNumeric())
			{
				Story->SkipToRank(FCString::Atoi(*Args[0]), World);
			}
		}
	}));

static FAutoConsoleCommandWithWorld GBNStoryStartFinalChaseCmd(
	TEXT("bn.StoryStartFinalChase"),
	TEXT("Starts Perde 5 Special Tier chase immediately on the active map."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBNStorySubsystem* Story = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->SkipToRank(0, World);
			Story->StartFinalChase(World);
		}
	}));

static FAutoConsoleCommandWithWorld GBNStoryResetCmd(
	TEXT("bn.StoryReset"),
	TEXT("Resets BLACK NUMBERS campaign progression back to Perde 1 (#16)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBNStorySubsystem* Story = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->ResetStory();
		}
	}));
