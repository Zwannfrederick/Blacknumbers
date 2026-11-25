#include "Misc/AutomationTest.h"
#include "Police/BNPoliceSirenComponent.h"
#include "Police/BNPursuitLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPursuitLogicTest, "BlackNumbers.Police.PursuitLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPursuitLogicTest::RunTest(const FString& Parameters)
{
	FBNPursuitState State;
	TestEqual(TEXT("pursuit starts dormant"), State.Phase, EBNPursuitPhase::None);
	TestEqual(TEXT("Kanon §8 caps normal progression at Heat 5"), State.MaxAllowedHeatLevel, 5);

	State.StartPursuit(1);
	TestEqual(TEXT("StartPursuit enters Pursuit phase"), State.Phase, EBNPursuitPhase::Pursuit);
	TestEqual(TEXT("Heat begins at 1"), State.HeatLevel, 1);
	TestFalse(TEXT("initial dispatch line posted"), State.ActiveRadio.Message.IsEmpty());

	// Heat escalates via pursuit time and collateral damage, but never past MaxAllowedHeatLevel (5) before R4.
	State.Update(30.f, 110.f, 1500.f, 1);
	TestTrue(TEXT("Heat escalates with pursuit time"), State.HeatLevel >= 2);
	State.RecordCollateralDamage(500000, TEXT("Test"));
	TestEqual(TEXT("Heat 6 remains locked until identity reveal (R4)"), State.HeatLevel, 5);

	State.SetMaxAllowedHeat(6);
	State.RecordCollateralDamage(100000);
	TestEqual(TEXT("unlocking R4 allows Heat 6 escalation"), State.HeatLevel, 6);

	// Pursuit breaker updates collateral cost, disabled count, and radio.
	const int32 PrevCost = State.CostToStateDollars;
	State.RecordPursuitBreaker(25000, 2);
	TestEqual(TEXT("Pursuit Breaker adds collateral to Sehir Faturasi"), State.CostToStateDollars, PrevCost + 25000);
	TestEqual(TEXT("Pursuit Breaker records disabled units"), State.UnitsDisabled, 2);
	TestEqual(TEXT("Pursuit Breaker increments breaker count"), State.BreakersTriggered, 1);

	// Pinning at low speed fills BustMeter -> Busted; speeding away decays BustMeter.
	FBNPursuitState BustCheck;
	BustCheck.StartPursuit(1);
	BustCheck.Update(2.0f, 5.f, 700.f, 1);
	TestTrue(TEXT("pinned car builds BustMeter"), BustCheck.BustMeter > 0.4f);
	BustCheck.Update(2.0f, 95.f, 3000.f, 1);
	TestTrue(TEXT("pulling away decays BustMeter"), BustCheck.BustMeter < 0.1f);
	BustCheck.Update(5.0f, 0.f, 500.f, 1);
	TestEqual(TEXT("holding still near police triggers Busted"), BustCheck.Phase, EBNPursuitPhase::Busted);

	// Full Pursuit -> Evade -> Cooldown -> Escaped flow, with faster cooldown while stopped/hiding.
	FBNPursuitState EscapeCheck;
	EscapeCheck.StartPursuit(1);
	EscapeCheck.Update(0.5f, 120.f, 15000.f, 1);
	TestEqual(TEXT("breaking sight enters Evade"), EscapeCheck.Phase, EBNPursuitPhase::Evade);
	EscapeCheck.Update(4.0f, 120.f, 15000.f, 1);
	TestEqual(TEXT("staying out of sight enters Cooldown"), EscapeCheck.Phase, EBNPursuitPhase::Cooldown);

	FBNPursuitState MovingCooldown = EscapeCheck;
	FBNPursuitState HidingCooldown = EscapeCheck;
	MovingCooldown.Update(5.0f, 80.f, 15000.f, 0);
	HidingCooldown.Update(5.0f, 0.f, 15000.f, 0);
	TestTrue(TEXT("stopping/hiding cools down faster than speeding"), HidingCooldown.PhaseProgress > MovingCooldown.PhaseProgress);

	HidingCooldown.Update(10.0f, 0.f, 15000.f, 0);
	TestEqual(TEXT("completing Cooldown transitions to Escaped"), HidingCooldown.Phase, EBNPursuitPhase::Escaped);

	// Siren pitch math: Wail/Yelp bounds and Speedbreaker pitch drop.
	const float WailHz = BNPoliceSiren::EvaluateFrequency(1.1f, false, 1.f);
	const float YelpHz = BNPoliceSiren::EvaluateFrequency(0.18f, true, 1.f);
	const float SlowYelpHz = BNPoliceSiren::EvaluateFrequency(0.18f, true, 0.38f);
	TestTrue(TEXT("Wail pitch stays in expected band"), WailHz >= 600.f && WailHz <= 1250.f);
	TestTrue(TEXT("Yelp peaks higher than Wail"), YelpHz > 1200.f);
	TestTrue(TEXT("Speedbreaker lowers siren pitch"), SlowYelpHz < YelpHz * 0.75f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPursuitChatterAndRapSheetTest, "BlackNumbers.Police.ChatterAndRapSheet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPursuitChatterAndRapSheetTest::RunTest(const FString& Parameters)
{
	// Ambient radio chatter keeps the scanner alive between scripted events.
	FBNPursuitState Chatter;
	Chatter.StartPursuit(1);
	const int32 SerialAfterStart = Chatter.DispatchSerial;
	for (int32 i = 0; i < 100; ++i)
	{
		Chatter.Update(0.1f, 120.f, 1500.f, 1);
	}
	TestTrue(TEXT("idle pursuit posts ambient chatter within 10 s"), Chatter.DispatchSerial > SerialAfterStart);
	TestFalse(TEXT("ambient chatter has text"), Chatter.ActiveRadio.Message.IsEmpty());
	TestFalse(TEXT("ambient chatter has a speaker"), Chatter.ActiveRadio.Speaker.IsEmpty());

	// Rap sheet: each infraction is recorded once and adds bounty once.
	FBNPursuitState Sheet;
	Sheet.StartPursuit(1);
	TestEqual(TEXT("no infractions at pursuit start"), Sheet.CountInfractions(), 0);
	Sheet.Update(0.1f, 195.f, 1500.f, 1);
	TestTrue(TEXT("speeding above threshold is an infraction"), Sheet.HasInfraction(EBNInfraction::Speeding));
	TestTrue(TEXT("top speed tracked"), Sheet.TopSpeedKmh >= 195.f);

	const int32 BeforeReckless = Sheet.Bounty;
	Sheet.RecordCollateralDamage(800, TEXT("Lamba"));
	const int32 FirstGain = Sheet.Bounty - BeforeReckless;
	Sheet.RecordCollateralDamage(800, TEXT("Lamba"));
	TestTrue(TEXT("collateral damage is reckless driving"), Sheet.HasInfraction(EBNInfraction::Reckless));
	TestTrue(TEXT("first infraction adds bounty"), FirstGain > 0);
	TestEqual(TEXT("repeat infraction adds no extra bounty"), Sheet.Bounty - BeforeReckless, FirstGain);

	Sheet.RecordUnitDisabled();
	TestTrue(TEXT("disabling a unit is ramming police"), Sheet.HasInfraction(EBNInfraction::RammedPolice));
	Sheet.RecordPursuitBreaker(5000, 0);
	TestTrue(TEXT("pursuit breaker is property damage"), Sheet.HasInfraction(EBNInfraction::PropertyDamage));

	const int32 BeforeDodge = Sheet.Bounty;
	Sheet.RecordRoadblockDodged();
	Sheet.RecordSpikeStripDodged();
	TestEqual(TEXT("roadblock dodge counted"), Sheet.RoadblocksDodged, 1);
	TestEqual(TEXT("spike strip dodge counted"), Sheet.SpikeStripsDodged, 1);
	TestTrue(TEXT("dodges add bounty"), Sheet.Bounty > BeforeDodge);

	Sheet.Update(31.f, 120.f, 1500.f, 1);
	TestTrue(TEXT("long pursuit is resisting arrest"), Sheet.HasInfraction(EBNInfraction::ResistingArrest));

	Sheet.Update(0.5f, 120.f, 15000.f, 1);
	Sheet.Update(4.0f, 120.f, 15000.f, 1);
	Sheet.Update(60.0f, 0.f, 15000.f, 0);
	TestEqual(TEXT("pursuit escaped"), Sheet.Phase, EBNPursuitPhase::Escaped);
	TestTrue(TEXT("escaping is evading police"), Sheet.HasInfraction(EBNInfraction::Evading));
	TestEqual(TEXT("all six infractions on the sheet"), Sheet.CountInfractions(), 6);
	TestFalse(TEXT("infractions have Turkish labels"), FBNPursuitState::InfractionLabel(EBNInfraction::Evading).IsEmpty());

	// A new pursuit starts with a clean rap sheet.
	Sheet.StartPursuit(1);
	TestEqual(TEXT("new pursuit clears infractions"), Sheet.CountInfractions(), 0);
	TestEqual(TEXT("new pursuit clears dodges"), Sheet.RoadblocksDodged, 0);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "EngineUtils.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitBreaker.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

using namespace BNPlayTest;

namespace
{
	const FVector SkidpadSouth(25000.f, -8000.f, 60.f);
	constexpr float SkidpadYaw = 90.f;
	constexpr float CruiserSpawnTimeoutSeconds = 5.f;
	constexpr float DriveBreakerTimeoutSeconds = 12.f;

	struct FPursuitPieState
	{
		enum class EPhase { Setup, WaitCruiser, DriveBreaker, VerifyStopped } Phase = EPhase::Setup;
		double PhaseStart = 0.0;
		TWeakObjectPtr<ABNPursuitBreaker> SpawnedBreaker;
		TWeakObjectPtr<ABNVehiclePawn> CruiserCar;
		TWeakObjectPtr<ABNPoliceController> CruiserCtrl;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPursuitAndBreakerPieTest, "BlackNumbers.Police.PursuitAndBreakerPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPursuitAndBreakerPieTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FPursuitPieState> State = MakeShared<FPursuitPieState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FPursuitPieState::EPhase;
		ABNVehiclePawn* Player = FindPlayerCar();
		UWorld* World = PlayWorld();
		UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		if (!Player || !World || !Pursuit)
		{
			AddError(TEXT("Player car, PlayWorld or UBNPursuitSubsystem missing"));
			return true;
		}

		const double Now = World->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::Setup:
		{
			TestEqual(TEXT("pursuit subsystem starts dormant"), Pursuit->GetState().Phase, EBNPursuitPhase::None);
			TestEqual(TEXT("zero police spawned before pursuit"), Pursuit->GetActivePoliceCount(), 0);

			PlaceStopped(*Player, SkidpadSouth, SkidpadYaw);
			Pursuit->StartPursuit(1);
			Enter(EPhase::WaitCruiser);
			return false;
		}
		case EPhase::WaitCruiser:
		{
			if (InPhase > CruiserSpawnTimeoutSeconds)
			{
				Pursuit->StopPursuit();
				AddError(TEXT("Timed out waiting for police cruiser"));
				return true;
			}
			for (TActorIterator<ABNPoliceController> It(World); It; ++It)
			{
				if (ABNVehiclePawn* Cruiser = Cast<ABNVehiclePawn>(It->GetPawn()))
				{
					State->CruiserCtrl = *It;
					State->CruiserCar = Cruiser;
					break;
				}
			}
			if (!State->CruiserCtrl.IsValid() || !State->CruiserCar.IsValid())
			{
				return false;
			}

			TestTrue(TEXT("at least one active police cruiser spawned"), Pursuit->GetActivePoliceCount() >= 1);
			TestTrue(TEXT("police cruiser has visual lightbar"), State->CruiserCtrl->HasVisualLightbar());
			TestTrue(TEXT("police cruiser siren is active"), State->CruiserCtrl->IsSirenActive());

			// Place player, test Pursuit Breaker 24 m ahead (+Y), and cruiser 7 m behind so both chase into DisableRadiusCm.
			PlaceStopped(*Player, SkidpadSouth, SkidpadYaw);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABNPursuitBreaker* Breaker = World->SpawnActor<ABNPursuitBreaker>(
				ABNPursuitBreaker::StaticClass(), SkidpadSouth + FVector(0.f, 2400.f, -60.f), FRotator(0.f, SkidpadYaw, 0.f), Params);
			State->SpawnedBreaker = Breaker;
			if (!Breaker)
			{
				Pursuit->StopPursuit();
				AddError(TEXT("Failed to spawn test ABNPursuitBreaker"));
				return true;
			}

			PlaceStopped(*State->CruiserCar, SkidpadSouth + FVector(0.f, -700.f, 0.f), SkidpadYaw);
			Player->GetChaosMovement()->SetThrottleInput(1.f);
			Enter(EPhase::DriveBreaker);
			return false;
		}
		case EPhase::DriveBreaker:
		{
			ABNPursuitBreaker* Breaker = State->SpawnedBreaker.Get();
			if (InPhase > DriveBreakerTimeoutSeconds)
			{
				Pursuit->StopPursuit();
				AddError(TEXT("Player did not trigger test Pursuit Breaker in time"));
				return true;
			}
			if (!Breaker || !Breaker->IsCollapsed())
			{
				return false;
			}

			const FBNPursuitState& S = Pursuit->GetState();
			UE_LOG(LogTemp, Display, TEXT("BN pursuit breaker test: cost=$%d disabled=%d breakers=%d radio='%s'"),
				S.CostToStateDollars, S.UnitsDisabled, S.BreakersTriggered, *S.ActiveRadio.Message);
			TestTrue(TEXT("breaker collapse recorded Sehir Faturasi"), S.CostToStateDollars >= Breaker->CollateralCostDollars);
			TestTrue(TEXT("breaker disabled nearby pursuing police"), S.UnitsDisabled >= 1);
			TestTrue(TEXT("breaker increment recorded"), S.BreakersTriggered >= 1);
			TestFalse(TEXT("dispatch radio message populated"), S.ActiveRadio.Message.IsEmpty());
			TestTrue(TEXT("cruiser controller reports disabled"), State->CruiserCtrl.IsValid() && State->CruiserCtrl->IsUnitDisabled());

			Player->GetChaosMovement()->SetThrottleInput(0.f);
			Pursuit->StopPursuit();
			Enter(EPhase::VerifyStopped);
			return false;
		}
		case EPhase::VerifyStopped:
			TestEqual(TEXT("StopPursuit resets phase to None"), Pursuit->GetState().Phase, EBNPursuitPhase::None);
			TestEqual(TEXT("StopPursuit removes all active cruisers"), Pursuit->GetActivePoliceCount(), 0);
			if (ABNPursuitBreaker* Breaker = State->SpawnedBreaker.Get())
			{
				Breaker->Destroy();
			}
			return true;
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPursuitRaceIsolationPieTest, "BlackNumbers.Police.BreakerIgnoredDuringRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPursuitRaceIsolationPieTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		ABNVehiclePawn* Player = FindPlayerCar();
		UWorld* World = PlayWorld();
		UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		ABNRaceEvent* Race = nullptr;
		if (World)
		{
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
			{
				if (It->Definition)
				{
					Race = *It;
					break;
				}
			}
		}
		if (!Player || !World || !Pursuit || !Race)
		{
			AddError(TEXT("Player, PlayWorld, UBNPursuitSubsystem or ABNRaceEvent missing"));
			return true;
		}

		// 1. Free-roam without active pursuit: Pursuit Breaker stays dormant and never creates a phantom Evade chase.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABNPursuitBreaker* Breaker = World->SpawnActor<ABNPursuitBreaker>(
			ABNPursuitBreaker::StaticClass(), SkidpadSouth, FRotator::ZeroRotator, Params);
		if (!Breaker)
		{
			AddError(TEXT("Failed to spawn test breaker"));
			return true;
		}
		Breaker->TriggerCollapse(Player);
		TestFalse(TEXT("breaker stays dormant when no pursuit is running"), Breaker->IsCollapsed());
		TestEqual(TEXT("dormant breaker never enters Pursuit or Evade"), Pursuit->GetState().Phase, EBNPursuitPhase::None);

		// 2. Starting a race must immediately clear an active pursuit so Evade never fires during countdown/racing.
		Pursuit->StartPursuit(1);
		TestTrue(TEXT("pursuit active before starting race"), Pursuit->GetState().IsPursuitRunning());
		Race->StartRace(Player);
		TestEqual(TEXT("starting a race stops active pursuit immediately"), Pursuit->GetState().Phase, EBNPursuitPhase::None);
		TestEqual(TEXT("starting a race clears active police cruisers"), Pursuit->GetActivePoliceCount(), 0);

		// 3. Even direct breaker / subsystem triggers during an active race cannot start a pursuit or Evade.
		Breaker->TriggerCollapse(Player);
		Pursuit->OnPursuitBreakerTriggered(Breaker, Player);
		TestFalse(TEXT("breaker does not collapse during an active race"), Breaker->IsCollapsed());
		TestEqual(TEXT("pursuit remains None during an active race"), Pursuit->GetState().Phase, EBNPursuitPhase::None);

		Race->EndRace();
		Breaker->Destroy();
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
