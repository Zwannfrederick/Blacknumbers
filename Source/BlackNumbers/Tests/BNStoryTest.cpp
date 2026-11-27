#include "Misc/AutomationTest.h"
#include "Story/BNStoryTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNStoryProgressionLogicTest, "BlackNumbers.Story.CampaignProgressionLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNStoryProgressionLogicTest::RunTest(const FString& Parameters)
{
	const TArray<FBNBossSpec>& Roster = FBNStoryState::GetAllBosses();
	TestEqual(TEXT("BLACK NUMBERS canon roster has 16 members"), Roster.Num(), 16);

	TSet<FString> VehicleNames;
	TSet<FString> Programs;
	for (const FBNBossSpec& B : Roster)
	{
		TestTrue(TEXT("boss rank within 1..16"), B.Rank >= 1 && B.Rank <= 16);
		TestFalse(TEXT("boss name non-empty"), B.Name.IsEmpty());
		TestFalse(TEXT("boss org role non-empty"), B.OrgRole.IsEmpty());
		TestFalse(TEXT("boss fictional vehicle non-empty"), B.VehicleName.IsEmpty());
		VehicleNames.Add(B.VehicleName);
		TestEqual(TEXT("total program grows from two to five races"), B.Qualifiers.Num() + 1, B.TierLayer + 1);
		TSet<FName> Races;
		FString Program;
		for (FName Race : B.Qualifiers)
		{
			Races.Add(Race);
			Program += Race.ToString() + TEXT(" / ");
		}
		Races.Add(FName(*B.RaceAssetName));
		TestEqual(TEXT("no race repeats within a boss program"), Races.Num(), B.Qualifiers.Num() + 1);
		Programs.Add(Program + B.RaceAssetName);
	}
	TestEqual(TEXT("all 16 boss vehicles are distinct fictional models"), VehicleNames.Num(), 16);
	TestEqual(TEXT("each boss has a different ordered program"), Programs.Num(), 16);

	FBNStoryState State;
	State.Reset();
	TestEqual(TEXT("starts in Perde 1"), State.Act, EBNStoryAct::Perde1_Donus);
	TestEqual(TEXT("Tarik starts hostile"), State.TarikAttitude, EBNTarikAttitude::Hostile);
	TestEqual(TEXT("next challenger is #16"), State.NextBossRank, 16);
	TestEqual(TEXT("Heat cap starts at 5 (K. Vance)"), State.GetMaxAllowedHeat(), 5);
	TestFalse(TEXT("Tier 6 locked at start"), State.IsTier6Unlocked());
	TestFalse(TEXT("#01 Viktor locked until R8 evidence completes"), State.CanChallengeBoss(1));
	TestFalse(TEXT("first duel locked before qualifying"), State.IsDuelUnlocked(16));
	const FName FirstRace = State.GetNextQualifier();
	TestFalse(TEXT("other races do not qualify"), State.RecordQualifierVictory(FName(TEXT("RACE_Another"))));
	TestTrue(TEXT("winning the first qualifier satisfies Race Wins gate"), State.RecordQualifierVictory(FirstRace));
	TestFalse(TEXT("repeating the qualifier adds no progress"), State.RecordQualifierVictory(FirstRace));
	TestFalse(TEXT("MW 3-gate rule: duel still locked without Milestones + Bounty"), State.IsDuelUnlocked(16));
	TestTrue(TEXT("Speed Trap >= 165 km/h completes #16 Milestone"), State.EvaluateSpeedTrapMilestones(170.f) >= 1);
	TestTrue(TEXT("Milestone completion satisfies Milestones gate"), State.AreMilestonesComplete(16));
	State.BankPursuitBounty(10000);
	TestTrue(TEXT("Banking pursuit Bounty satisfies Total Bounty gate"), State.IsBountyComplete(16));
	TestTrue(TEXT("first duel unlocked once all 3 MW gates are met"), State.IsDuelUnlocked(16));

	TArray<FBNStoryCard> Cards;
	auto Beat = [&](int32 Rank)
	{
		Cards.Reset();
		const bool bOk = State.RecordBossVictory(Rank, Cards);
		TestTrue(FString::Printf(TEXT("defeated #%02d"), Rank), bOk);
	};

	Beat(16);
	TestEqual(TEXT("program progress resets for the next boss"), State.QualifiersWon, 0);
	TestEqual(TEXT("Perde 2 opens at #16 (R2)"), State.Act, EBNStoryAct::Perde2_Yukselis);
	Beat(15); Beat(14); Beat(13);
	Beat(12);
	TestEqual(TEXT("Tarik shifts to Suspicious after #12 Leyla (R3)"), State.TarikAttitude, EBNTarikAttitude::Suspicious);
	Beat(11); Beat(10); Beat(9);
	Beat(8);
	TestTrue(TEXT("R4 exposes Kerem identity at #08 Hakan"), State.bIdentityExposed);
	TestEqual(TEXT("R4 unlocks Heat 6-10"), State.GetMaxAllowedHeat(), 10);
	TestEqual(TEXT("Tarik shifts to DeepDoubt at #08"), State.TarikAttitude, EBNTarikAttitude::DeepDoubt);
	TestEqual(TEXT("Act enters Perde 3 at #08"), State.Act, EBNStoryAct::Perde3_Ifsa);
	Beat(7);
	Beat(6);
	TestTrue(TEXT("R5 exposes Selin's police badge at #06 Mirac"), State.bSelinExposed);
	Beat(5);
	TestTrue(TEXT("R6 reconciles Tarik at #05 Kaan"), State.bTarikReconciled);
	TestTrue(TEXT("R6 unlocks Tier 6"), State.IsTier6Unlocked());
	TestEqual(TEXT("Tarik becomes Reconciled at R6"), State.TarikAttitude, EBNTarikAttitude::Reconciled);
	TestEqual(TEXT("Act enters Perde 4 at R6"), State.Act, EBNStoryAct::Perde4_Zirve);
	TestFalse(TEXT("#01 Viktor still locked before R8 (#02)"), State.CanChallengeBoss(1));

	Beat(4); Beat(3); Beat(2);
	TestTrue(TEXT("R8 evidence completes at #02 Elena"), State.bEvidenceComplete);
	TestTrue(TEXT("#01 Viktor unlocks once R8 evidence completes"), State.CanChallengeBoss(1));

	Beat(1);
	TestTrue(TEXT("Viktor arrested at #01 victory"), State.bViktorArrested);
	TestTrue(TEXT("Special Tier Perde 5 chase begins after Viktor arrest"), State.bFinalChaseActive);
	TestEqual(TEXT("Special Tier allows Heat 11"), State.GetMaxAllowedHeat(), 11);
	bool bSawDeadOrAlive = false;
	for (const FBNStoryCard& C : Cards)
	{
		bSawDeadOrAlive |= C.Body.Contains(TEXT("OLU YA DA DIRI"));
	}
	TestTrue(TEXT("Demir's verbatim Special Tier order appears in Perde 5 card"), bSawDeadOrAlive);

	TArray<FBNStoryCard> EpilogCards;
	TestTrue(TEXT("Perde 5 Route A escape completes campaign"), State.CompleteFinalChaseEscape(0, 345000, EpilogCards));
	TestTrue(TEXT("campaign marked completed"), State.bCampaignCompleted);
	TestEqual(TEXT("Act reaches Epilog"), State.Act, EBNStoryAct::Epilog);
	TestEqual(TEXT("Tarik retires permanently in Epilog"), State.TarikAttitude, EBNTarikAttitude::Retired);
	TestEqual(TEXT("recorded FinalCityDamageDollars matches Sehir Faturasi"), State.FinalCityDamageDollars, 345000);

	bool bHasDamage = false;
	bool bHasTarikLine1 = false;
	bool bHasTarikLine2 = false;
	for (const FBNStoryCard& C : EpilogCards)
	{
		bHasDamage |= C.Body.Contains(TEXT("345,000"));
		bHasTarikLine1 |= C.Body.Contains(TEXT("O gunler eskide kaldi, adamim."));
		bHasTarikLine2 |= C.Body.Contains(TEXT("Dukkanimi terk et. Yoksa polisi ararim."));
	}
	TestTrue(TEXT("Epilogue displays real Sehir Faturasi damage"), bHasDamage);
	TestTrue(TEXT("Epilogue preserves Tarik canonical line 1"), bHasTarikLine1);
	TestTrue(TEXT("Epilogue preserves Tarik canonical line 2"), bHasTarikLine2);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Garage/BNGarageSubsystem.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNTrafficSubsystem.h"

using namespace BNPlayTest;

namespace
{
	struct FStoryPieState
	{
		enum class EPhase
		{
			InitChecks,
			StartBoss8,
			CrossGates,
			VerifyPostBoss8AndTier6,
			DriveTier6,
			VerifyPerde5Routes
		} Phase = EPhase::InitChecks;

		double PhaseStart = 0.0;
		TWeakObjectPtr<ABNRaceEvent> SprintEvent;
		int32 PlacedGateIndex = -1;
	};

	void PlaceCar(ABNVehiclePawn& Car, const FVector& Loc, float Yaw)
	{
		Car.SetActorLocationAndRotation(Loc, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNStoryTier6AndFinalChasePieTest, "BlackNumbers.Story.Tier6AndFinalChasePIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNStoryTier6AndFinalChasePieTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar())
		{
			return true;
		}
		if (FPlatformTime::Seconds() - WaitStart > 30.0)
		{
			AddError(TEXT("Player car not spawned in KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));

	const TSharedRef<FStoryPieState> State = MakeShared<FStoryPieState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FStoryPieState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		UWorld* World = PlayWorld();
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
		UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
		UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		if (!Car || !World || !Story || !Prog || !Pursuit)
		{
			AddError(TEXT("Missing Car, World, Story, Prog, or Pursuit subsystem"));
			return true;
		}

		const double Now = World->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::InitChecks:
		{
			Prog->ResetProfile();
			Story->ResetStory(false);
			Prog->AddCash(600000);
			for (int32 i = 0; i < 5; ++i)
			{
				Prog->TryBuyUpgrade(EBNUpgradeSlot::Engine, Car);
			}
			TestFalse(TEXT("Tier 6 Engine purchase fails before Tarik R6"), Prog->TryBuyUpgrade(EBNUpgradeSlot::Engine, Car));
			TestEqual(TEXT("Pursuit Heat cap is 5 before #08 R4"), Pursuit->GetState().MaxAllowedHeatLevel, 5);

			const FBNBossSpec* Boss8 = FBNStoryState::FindBossByRank(8);
			const FString WantedRace = Boss8 ? Boss8->RaceAssetName : FString();
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
			{
				if (It->Definition && (!WantedRace.IsEmpty()
					? It->Definition->GetName() == WantedRace
					: It->Definition->Discipline == EBNRaceDiscipline::Sprint))
				{
					State->SprintEvent = *It;
					break;
				}
			}
			if (!State->SprintEvent.IsValid())
			{
				AddError(TEXT("KaramaraSlice has no Sprint event for Boss #08 test"));
				return true;
			}

			Story->SkipToRank(8, World);
			for (const FBNBossSpec& Boss : FBNStoryState::GetAllBosses())
			{
				TestNotNull(TEXT("duel exists in Karamara"), UBNStorySubsystem::FindProgramRace(World, FName(*Boss.RaceAssetName)));
				for (FName Race : Boss.Qualifiers)
				{
					TestNotNull(TEXT("qualifier exists in Karamara"), UBNStorySubsystem::FindProgramRace(World, Race));
				}
			}
			ABNRaceEvent* FirstQualifier = UBNStorySubsystem::FindProgramRace(World, Story->GetState().GetNextQualifier());
			if (!FirstQualifier)
			{
				return true;
			}
			TestTrue(TEXT("career launches the first qualifier"), Story->ChallengeSelectedBoss(Car, 8));
			TestTrue(TEXT("the qualifier is active and the duel remains locked"),
				ABNRaceEvent::FindActiveRace(World) == FirstQualifier && !Story->GetState().IsDuelUnlocked(8));
			FirstQualifier->EndRace();
			TestEqual(TEXT("abandoning a qualifier adds no progress"), Story->GetState().QualifiersWon, 0);
			Story->NotifyStreetRaceWon(FirstQualifier->Definition->GetFName());
			UBNSaveGame* Save = NewObject<UBNSaveGame>();
			Story->WriteToSaveGame(*Save);
			Story->ReadFromSaveGame(*Save);
			TestEqual(TEXT("a partial program is restored from the save"), Story->GetState().QualifiersWon, 1);
			TestFalse(TEXT("partial program leaves duel locked after loading"), Story->GetState().IsDuelUnlocked(8));
			for (FName Race : Boss8->Qualifiers)
			{
				Story->NotifyStreetRaceWon(Race);
			}
			TestFalse(TEXT("Boss #08 duel stays locked until MW Milestones & Bounty are met"), Story->GetState().IsDuelUnlocked(8));
			Story->GrantDebugBountyAndMilestonesForCurrentBoss();
			TestTrue(TEXT("completing Races + Milestones + Bounty unlocks #08 duel"), Story->GetState().IsDuelUnlocked(8));
			Story->OpenDossier(Car);
			TestTrue(TEXT("Boss #08 challenge starts Sprint race cleanly"), Story->ChallengeSelectedBoss(Car));
			Enter(EPhase::StartBoss8);
			return false;
		}

		case EPhase::StartBoss8:
			if (State->SprintEvent->GetState() != EBNRaceState::Racing)
			{
				return InPhase > 8.f;
			}
			TestEqual(TEXT("Boss #08 spawns 1 rival"), State->SprintEvent->GetNumRivals(), 1);
			TestEqual(TEXT("Sprint event tracks ActiveBossRank == 8"), State->SprintEvent->GetActiveBossRank(), 8);
			State->PlacedGateIndex = -1;
			Car->GetChaosMovement()->SetThrottleInput(1.f);
			Enter(EPhase::CrossGates);
			return false;

		case EPhase::CrossGates:
		{
			if (State->SprintEvent->GetState() == EBNRaceState::Finished)
			{
				Car->GetChaosMovement()->SetThrottleInput(0.f);
				TestTrue(TEXT("Winning #08 sets IdentityExposed (R4)"), Story->GetState().bIdentityExposed);
				TestEqual(TEXT("Heat cap unlocked to 10 after #08 R4"), Pursuit->GetState().MaxAllowedHeatLevel, 10);
				Enter(EPhase::VerifyPostBoss8AndTier6);
				return false;
			}
			if (InPhase > 25.f)
			{
				Car->GetChaosMovement()->SetThrottleInput(0.f);
				AddError(TEXT("Timed out crossing Boss #08 Sprint checkpoints"));
				return true;
			}
			const int32 NextIdx = State->SprintEvent->GetProgress().NextCheckpoint();
			if (NextIdx != State->PlacedGateIndex && State->SprintEvent->Definition->Checkpoints.IsValidIndex(NextIdx))
			{
				State->PlacedGateIndex = NextIdx;
				const FTransform& Gate = State->SprintEvent->Definition->Checkpoints[NextIdx];
				PlaceCar(*Car, Gate.TransformPosition(FVector(-450.f, 0.f, 65.f)), Gate.Rotator().Yaw);
				Car->GetChaosMovement()->SetThrottleInput(1.f);
			}
			return false;
		}

		case EPhase::VerifyPostBoss8AndTier6:
			// Defeat down to #04 (past #05 Kaan R6 reconciliation) and verify Tier 6 unlock + driving stability.
			Story->SkipToRank(4, World);
			TestTrue(TEXT("R6 sets bTier6Unlocked true on ProgressionSubsystem"), Prog->GetProfile().bTier6Unlocked);
			TestTrue(TEXT("Tier 6 Engine purchase succeeds once reconciled"), Prog->TryBuyUpgrade(EBNUpgradeSlot::Engine, Car));
			TestEqual(TEXT("Engine tier reaches 6"), Prog->GetProfile().GetUpgradeTier(EBNUpgradeSlot::Engine), 6);

			// Keep civilian traffic & free-roam police patrols clear of the Tier 6 straight-line physics stability check.
			if (UBNTrafficSubsystem* Traffic = World->GetSubsystem<UBNTrafficSubsystem>())
			{
				Traffic->SetTrafficEnabled(false);
			}
			Pursuit->SetFreeRoamPatrolsEnabled(false);
			Pursuit->StopPursuit();
			PlaceCar(*Car, FVector(-26000.f, -4000.f, 70.f), 0.f);
			Car->GetChaosMovement()->SetThrottleInput(1.f);
			Enter(EPhase::DriveTier6);
			return false;

		case EPhase::DriveTier6:
			if (InPhase < 2.2f)
			{
				return false;
			}
			Car->GetChaosMovement()->SetThrottleInput(0.f);
			TestTrue(TEXT("Tier 6 car accelerates smoothly forward"), Car->GetSpeedKmh() > 55.f);
			TestTrue(TEXT("Tier 6 stays balanced (no vertical sky launch)"), FMath::Abs(Car->GetVelocity().Z) < 120.f);

			// Enter Perde 5 Special Tier and test Route 0 escape, then Route 1 escape.
			Story->SkipToRank(0, World);
			TestTrue(TEXT("StartFinalChase launches Heat 11 Special Tier"), Story->StartFinalChase(World));
			TestEqual(TEXT("Special Tier Heat level is 11"), Pursuit->GetState().HeatLevel, 11);
			TestTrue(TEXT("Special Tier posts Demir dead-or-alive dispatch"),
				Pursuit->GetState().ActiveRadio.Message.Contains(TEXT("ÖLÜ YA DA DİRİ"))
				|| Pursuit->GetState().ActiveRadio.Message.Contains(TEXT("OLU YA DA DIRI")));

			Pursuit->ReportCollateralDamage(125000, TEXT("Liman Bariyeri"));
			PlaceCar(*Car, UBNStorySubsystem::GetEscapeRouteLocation(World, 0) + FVector(0.f, 0.f, 60.f), 0.f);
			Enter(EPhase::VerifyPerde5Routes);
			return false;

		case EPhase::VerifyPerde5Routes:
			if (!Story->GetState().bCampaignCompleted && InPhase < 2.f)
			{
				return false;
			}
			TestTrue(TEXT("Route 0 (Liman Tunelleri) completes campaign"), Story->GetState().bCampaignCompleted);
			TestEqual(TEXT("EscapedRouteIndex recorded as 0"), Story->GetState().EscapedRouteIndex, 0);
			TestTrue(TEXT("FinalCityDamageDollars captures Sehir Faturasi >= 125000"), Story->GetState().FinalCityDamageDollars >= 125000);

			// Re-verify Route 1 (Kuzey Gecidi Servis Yolu) works just as cleanly.
			Story->SkipToRank(0, World);
			TestTrue(TEXT("Restart FinalChase for Route B check"), Story->StartFinalChase(World));
			PlaceCar(*Car, UBNStorySubsystem::GetEscapeRouteLocation(World, 1) + FVector(0.f, 0.f, 60.f), 0.f);
			TestTrue(TEXT("Route 1 (Kuzey Gecidi) triggers CheckFinalChaseEscape immediately"), Story->CheckFinalChaseEscape(Car));
			TestEqual(TEXT("EscapedRouteIndex recorded as 1"), Story->GetState().EscapedRouteIndex, 1);

			if (UBNTrafficSubsystem* Traffic = World->GetSubsystem<UBNTrafficSubsystem>())
			{
				Traffic->SetTrafficEnabled(true);
			}
			Pursuit->StopPursuit();
			Prog->ResetProfile();
			Story->ResetStory(false);
			return true;
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
