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
	if (!Spec || !State.IsDuelUnlocked(BossRank))
	{
		return;
	}

	if (!bPlayerWon)
	{
		StatusFeedback = FString::Printf(TEXT("#%02d %s karşısında kaybettin! [B] Dosya ile tekrar dene."),
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
			FString::Printf(TEXT("BLACK NUMBERS #%02d YENİLDİ: +$%s   +%d İTİBAR"),
				BossRank, *FString::FormatAsNumber(Spec->RewardCash), Spec->RewardRep));
		if (FBNStoryState::IsDistrictRulerRank(BossRank))
		{
			Prog->GrantPinkSlip(Spec->BaseCar); // overrides the cash banner when the car is new
		}
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
	StatusFeedback = TEXT("W/S ile seç, ENTER ile programdaki sıradaki yarışa çık.");
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

ABNRaceEvent* UBNStorySubsystem::FindProgramRace(UWorld* World, FName RaceAsset)
{
	if (World && !RaceAsset.IsNone())
	{
		for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
		{
			if (It->Definition && It->Definition->GetFName() == RaceAsset)
			{
				return *It;
			}
		}
	}
	return nullptr;
}

bool UBNStorySubsystem::ChallengeSelectedBoss(ABNVehiclePawn* PlayerCar, int32 RequestedRank)
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

	if (HasActiveCard() || ABNRaceEvent::FindActiveRace(PlayerCar->GetWorld()))
	{
		StatusFeedback = TEXT("Yeni yarışa geçmek için mevcut etkinliği bitir.");
		return false;
	}
	const int32 Rank = RequestedRank == INDEX_NONE ? RankFromIndex(SelectedBossIndex) : RequestedRank;
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

	const FName Qualifier = State.GetNextQualifier();
	if (Qualifier.IsNone() && !State.IsDuelUnlocked(Rank))
	{
		StatusFeedback = FString::Printf(TEXT("Düello kilitli! Dönüm Noktası (%d/%d) ve Toplam Ödül (%s/%s) tamamlanmalı."),
			State.GetCompletedMilestoneCount(), Spec->RequiredMilestones,
			*FString::FormatAsNumber(State.TotalBounty), *FString::FormatAsNumber(Spec->RequiredBounty));
		return false;
	}
	ABNRaceEvent* Chosen = FindProgramRace(PlayerCar->GetWorld(),
		Qualifier.IsNone() ? FName(*Spec->RaceAssetName) : Qualifier);
	if (!Chosen)
	{
		StatusFeedback = TEXT("Bu yarış Karamara haritasında oynanır.");
		return false;
	}

	CloseDossier(PlayerCar);
	StatusFeedback.Reset();
	if (Qualifier.IsNone())
	{
		Chosen->StartBossRace(PlayerCar, Spec->Rank, Spec->RivalSkill, Spec->VehicleStats, Spec->VehicleTint, Spec->DriftTargetScore, Spec->BaseCar);
	}
	else
	{
		Chosen->StartRace(PlayerCar);
	}
	return true;
}

void UBNStorySubsystem::NotifyStreetRaceWon(FName RaceAsset)
{
	if (State.RecordQualifierVictory(RaceAsset))
	{
		StatusFeedback = State.IsDuelUnlocked(State.NextBossRank)
			? FString::Printf(TEXT("#%02d düellosu açıldı! [B] Kara Liste"), State.NextBossRank)
			: State.GetNextQualifier().IsNone()
				? FString::Printf(TEXT("Yarış galibiyetleri tamam! Dönüm noktaları ve Ödül için [B] Kara Liste'ye bak."))
				: TEXT("Yarış kazanıldı. Sıradaki etkinlik [B] Kara Liste'de.");
		PostComms(TEXT("SELİN"), StatusFeedback);
	}
}

void UBNStorySubsystem::OnPursuitEscaped(const FBNPursuitState& PursuitState, int32& OutMilestonesCompleted, int32& OutBonusBounty)
{
	OutMilestonesCompleted = 0;
	OutBonusBounty = 0;
	const int32 BountyBefore = State.TotalBounty;
	State.BankPursuitBounty(PursuitState.Bounty);

	TArray<FString> CompletedTitles;
	OutMilestonesCompleted = State.EvaluatePursuitMilestones(PursuitState, true, &CompletedTitles);
	OutBonusBounty = FMath::Max(0, State.TotalBounty - BountyBefore - FMath::Max(0, PursuitState.Bounty));

	if (OutMilestonesCompleted > 0)
	{
		PostComms(TEXT("ARDA"),
			FString::Printf(TEXT("%d Dönüm Noktası onaylandı (+%s Ödül)! Toplam Ödül: %s"),
				OutMilestonesCompleted, *FString::FormatAsNumber(OutBonusBounty), *FString::FormatAsNumber(State.TotalBounty)),
			6.2f, FLinearColor(0.95f, 0.72f, 0.14f, 1.f));
	}
	else
	{
		PostComms(TEXT("SELİN"),
			FString::Printf(TEXT("Temiz kaçış! +%s Ödül siciline işlendi (Toplam: %s)."),
				*FString::FormatAsNumber(PursuitState.Bounty), *FString::FormatAsNumber(State.TotalBounty)),
			5.0f, FLinearColor(0.24f, 0.88f, 0.50f, 1.f));
	}

	if (State.IsDuelUnlocked(State.NextBossRank))
	{
		if (const FBNBossSpec* Boss = FBNStoryState::FindBossByRank(State.NextBossRank))
		{
			PostComms(TEXT("SELİN"),
				FString::Printf(TEXT("#%02d %s DÜELLOSU AÇILDI! [B] Kara Liste'den meydan oku!"), Boss->Rank, *Boss->Name),
				6.5f, FLinearColor(1.0f, 0.78f, 0.18f, 1.f));
		}
	}
	if (IsInteractiveUIEnabled())
	{
		if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->SaveProfile();
		}
	}
}

void UBNStorySubsystem::OnPursuitBusted(int32 HeatLevel, UBNProgressionSubsystem* Prog, int32& OutFinePaid, bool& bOutImpoundReset)
{
	OutFinePaid = 0;
	bOutImpoundReset = false;
	const int32 Strikes = State.RecordBustedStrike();
	const int32 RawFine = FMath::Max(500, HeatLevel * 600);
	if (Strikes >= 3)
	{
		bOutImpoundReset = true;
		State.ImpoundStrikes = 0;
		const int32 TowFine = RawFine * 2;
		if (Prog)
		{
			OutFinePaid = FMath::Min(Prog->GetProfile().Cash, TowFine);
			Prog->AddCash(-OutFinePaid);
		}
		PostComms(TEXT("TARIK"),
			FString::Printf(TEXT("3/3 BAĞLAMA İHTARI! Aracı otoparktan çekmek $%s tuttu — takip ödülün yandı!"),
				*FString::FormatAsNumber(OutFinePaid)),
			6.5f, FLinearColor(0.95f, 0.22f, 0.18f, 1.f));
	}
	else
	{
		if (Prog)
		{
			OutFinePaid = FMath::Min(Prog->GetProfile().Cash, RawFine);
			Prog->AddCash(-OutFinePaid);
		}
		PostComms(TEXT("SELİN"),
			FString::Printf(TEXT("Yakalandın! İhtar %d/3 • Ceza: $%s • Bankalanmamış takip ödülü yandı."),
				Strikes, *FString::FormatAsNumber(OutFinePaid)),
			5.8f, FLinearColor(0.95f, 0.22f, 0.18f, 1.f));
	}
	if (IsInteractiveUIEnabled() && Prog)
	{
		Prog->SaveProfile();
	}
}

int32 UBNStorySubsystem::EvaluateLivePursuitMilestones(const FBNPursuitState& PursuitState, FString& OutNewestTitle) const
{
	TArray<FString> Titles;
	FBNStoryState Copy = State;
	const int32 Mask = Copy.EvaluatePursuitMilestones(PursuitState, false, &Titles);
	const int32 NewlyBit = Mask & ~PursuitState.PendingMilestonesMask;
	if (NewlyBit != 0 && Titles.Num() > 0)
	{
		OutNewestTitle = Titles.Last();
	}
	return Mask;
}

void UBNStorySubsystem::NotifySpeedTrapRecorded(float SpeedKmh)
{
	TArray<FString> Completed;
	if (State.EvaluateSpeedTrapMilestones(SpeedKmh, &Completed) > 0 && Completed.Num() > 0)
	{
		PostComms(TEXT("ARDA"),
			FString::Printf(TEXT("DÖNÜM NOKTASI TAMAMLANDI: %s! Toplam Ödül: %s"),
				*Completed[0], *FString::FormatAsNumber(State.TotalBounty)),
			6.0f, FLinearColor(0.96f, 0.74f, 0.16f, 1.f));
		if (IsInteractiveUIEnabled())
		{
			if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
			{
				Prog->SaveProfile();
			}
		}
	}
}

void UBNStorySubsystem::GrantDebugBountyAndMilestonesForCurrentBoss()
{
	if (const FBNBossSpec* Boss = FBNStoryState::FindBossByRank(State.NextBossRank))
	{
		for (int32 i = 0; i < Boss->RequiredMilestones && i < 3; ++i)
		{
			State.CompleteMilestone(i);
		}
		if (State.TotalBounty < Boss->RequiredBounty)
		{
			State.TotalBounty = Boss->RequiredBounty;
		}
	}
}

void UBNStorySubsystem::WriteToSaveGame(UBNSaveGame& SaveObj) const
{
	SaveObj.StoryNextBossRank = State.NextBossRank;
	SaveObj.StoryQualifiersWon = State.QualifiersWon;
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
	SaveObj.StoryTotalBounty = State.TotalBounty;
	SaveObj.StoryBossMilestonesMask = State.CurrentBossMilestonesMask;
	SaveObj.StoryImpoundStrikes = State.ImpoundStrikes;
	SaveObj.StoryPursuitsEscaped = State.TotalPursuitsEscaped;
	SaveObj.StoryBustedCount = State.TotalBustedCount;
}

void UBNStorySubsystem::ReadFromSaveGame(const UBNSaveGame& SaveObj)
{
	State.NextBossRank = FMath::Clamp(SaveObj.StoryNextBossRank, 0, 16);
	const FBNBossSpec* CurrentBoss = FBNStoryState::FindBossByRank(State.NextBossRank);
	State.QualifiersWon = CurrentBoss ? FMath::Clamp(SaveObj.StoryQualifiersWon, 0, CurrentBoss->Qualifiers.Num()) : 0;
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
	State.TotalBounty = FMath::Max(0, SaveObj.StoryTotalBounty);
	State.CurrentBossMilestonesMask = SaveObj.StoryBossMilestonesMask & 0x7;
	State.ImpoundStrikes = FMath::Clamp(SaveObj.StoryImpoundStrikes, 0, 3);
	State.TotalPursuitsEscaped = FMath::Max(0, SaveObj.StoryPursuitsEscaped);
	State.TotalBustedCount = FMath::Max(0, SaveObj.StoryBustedCount);
	State.RebuildEvidenceNotes();

	SelectedBossIndex = IndexFromRank(State.NextBossRank > 0 ? State.NextBossRank : 1);
	SyncWorldSystems(nullptr);
}

void UBNStorySubsystem::PostComms(const FString& Speaker, const FString& Message, float DurationSeconds, const FLinearColor& AccentColor)
{
	if (!IsInteractiveUIEnabled() || Speaker.IsEmpty() || Message.IsEmpty())
	{
		return;
	}
	// Cap queue length to prevent spam piling up during intense driving.
	if (CommsQueue.Num() >= 3)
	{
		CommsQueue.RemoveAt(0);
	}
	FBNLiveComms Entry;
	Entry.Speaker = Speaker;
	Entry.Message = Message;
	Entry.AccentColor = AccentColor;
	Entry.RemainingSeconds = FMath::Clamp(DurationSeconds, 2.5f, 9.f);
	CommsQueue.Add(MoveTemp(Entry));
}

void UBNStorySubsystem::TickComms(float DeltaSeconds)
{
	if (CommsQueue.Num() == 0)
	{
		return;
	}
	CommsQueue[0].RemainingSeconds -= FMath::Clamp(DeltaSeconds, 0.f, 0.25f);
	if (CommsQueue[0].RemainingSeconds <= 0.f)
	{
		CommsQueue.RemoveAt(0);
	}
}

void UBNStorySubsystem::NotifyDistrictEntered(int32 DistrictId)
{
	if (DistrictId < 0 || DistrictId >= 6)
	{
		return;
	}
	const uint32 Bit = 1u << static_cast<uint32>(DistrictId);
	if ((VisitedDistrictMask & Bit) != 0)
	{
		return;
	}
	VisitedDistrictMask |= Bit;

	const FLinearColor SelinCol(0.28f, 0.86f, 1.0f, 1.f);
	const FLinearColor TarikCol(0.96f, 0.68f, 0.14f, 1.f);
	const FLinearColor ArdaCol(0.22f, 0.90f, 0.48f, 1.f);
	const FLinearColor DemirCol(0.96f, 0.26f, 0.20f, 1.f);

	switch (DistrictId)
	{
	case 0:
		PostComms(TEXT("SELİN"), TEXT("Merkez Bulvar temiz görünüyor ama devriye sıklığı arttı. [M] ile haritadan rota çiz."), 5.5f, SelinCol);
		break;
	case 1:
		PostComms(TEXT("TARIK"), TEXT("Doğu Sanayi'ye hoş geldin. Konteyner avlusu drift için birebir — sakın gövdeyi dağıtma."), 5.5f, TarikCol);
		break;
	case 2:
		PostComms(TEXT("ARDA"), TEXT("Eski Liman Köprüsü geceleri Black Numbers'ın hız testi hattı. Köprü üstünde gaz kesme."), 5.5f, ArdaCol);
		break;
	case 3:
		PostComms(TEXT("ARDA"), TEXT("Kuzey Viyadüğü O-5 düzlüğü tam bir Drag cenneti. Vites zamanlamanı kaçırırsan motoru eline alırsın!"), 6.0f, ArdaCol);
		break;
	case 4:
		PostComms(TEXT("SELİN"), TEXT("Yamaç Mahallesi'ndeyiz. Garaj hemen tepede — parça yükseltmek istersen [G] ile uğra."), 5.5f, SelinCol);
		break;
	case 5:
		PostComms(TEXT("BAŞKOMİSER DEMİR"), TEXT("Tüm birimlere: Kuzey Geçidi dağ yolunda yasa dışı araç tespit edildi. Yolu kesin!"), 5.8f, DemirCol);
		break;
	default:
		break;
	}
}

void UBNStorySubsystem::NotifyReputationUpdated(int32 CurrentReputation)
{
	if (State.NextBossRank <= 0 || State.NextBossRank == AnnouncedReadyBossRank || CurrentReputation < 0)
	{
		return;
	}
	const FBNBossSpec* Boss = FBNStoryState::FindBossByRank(State.NextBossRank);
	if (Boss && State.CanChallengeBoss(State.NextBossRank))
	{
		AnnouncedReadyBossRank = State.NextBossRank;
		PostComms(TEXT("ARDA"),
			FString::Printf(TEXT("Sıradaki Black Numbers hedefi: #%02d %s (%s). Hazır olduğunda [B] dosyasından meydan oku!"),
				Boss->Rank, *Boss->Name, *Boss->DisciplineLabel),
			6.2f, FLinearColor(0.22f, 0.90f, 0.48f, 1.f));
	}
}

void UBNStorySubsystem::NotifyTotaledOrBlown(bool bBlownEngine)
{
	if (bBlownEngine)
	{
		PostComms(TEXT("TARIK"), TEXT("Pistonu fırlattın! Kırmızı bölgede o kadar tutma, yeşil bölgede [E] ile vites at!"),
			6.0f, FLinearColor(0.96f, 0.34f, 0.18f, 1.f));
	}
	else
	{
		PostComms(TEXT("SELİN"), TEXT("Şasi dağıldı! Çekiciyi gönderdim, aracı garajda hemen topluyoruz."),
			5.5f, FLinearColor(0.96f, 0.28f, 0.22f, 1.f));
	}
}

void UBNStorySubsystem::NotifyRaceOutcome(bool bWon, bool bDrag, const FString& RaceName)
{
	if (bWon)
	{
		PostComms(bDrag ? TEXT("TARIK") : TEXT("SELİN"),
			bDrag
				? FString::Printf(TEXT("Temiz vites geçişleri! %s düzlüğünde tozunu attırdın."), *RaceName)
				: FString::Printf(TEXT("%s cebinde! Şehirde adın kulaktan kulağa yayılıyor."), *RaceName),
			5.0f, FLinearColor(0.28f, 0.86f, 1.0f, 1.f));
	}
	else
	{
		PostComms(TEXT("ARDA"),
			FString::Printf(TEXT("%s kolay lokma değil. Garajda turbo ve lastik yükseltip tekrar dene."), *RaceName),
			5.0f, FLinearColor(0.96f, 0.72f, 0.16f, 1.f));
	}
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
