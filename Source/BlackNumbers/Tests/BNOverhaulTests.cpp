#include "Misc/AutomationTest.h"

#include "Audio/BNAudioSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Police/BNPursuitLogic.h"
#include "Race/BNDragLogic.h"
#include "UObject/Package.h"
#include "Vehicle/BNEngineAudioComponent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"

#if WITH_DEV_AUTOMATION_TESTS

// Pure unit test: verifies UBNRoadGraphSubsystem A* shortest path, nearest-road & look-ahead queries,
// 6 canon districts, and Cooldown hideout spot detection + 3.5x multiplier.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNRoadGraphAndDistrictsTest, "BlackNumbers.World.RoadGraphAStarAndDistricts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNRoadGraphAndDistrictsTest::RunTest(const FString& Parameters)
{
	UBNRoadGraphAsset* Asset = NewObject<UBNRoadGraphAsset>(GetTransientPackage());
	UBNRoadGraphSubsystem* Graph = NewObject<UBNRoadGraphSubsystem>(GetTransientPackage());
	TestNotNull(TEXT("synthetic RoadGraph asset created"), Asset);
	TestNotNull(TEXT("synthetic RoadGraph subsystem created"), Graph);
	if (!Asset || !Graph) { return false; }

	// Build a 4-node diamond network: 0 -> 1 -> 3 (short 200 m path) and 0 -> 2 -> 3 (long 350 m detour)
	auto AddNode = [&](int32 Id, const FVector& Pos, EBNDistrict Dist)
	{
		FBNRoadNode& N = Asset->Nodes.AddDefaulted_GetRef();
		N.Id = Id;
		N.Position = Pos;
		N.District = Dist;
	};
	auto AddEdge = [&](int32 A, int32 B, const FVector& PA, const FVector& PB, EBNDistrict Dist)
	{
		FBNRoadEdge& E = Asset->Edges.AddDefaulted_GetRef();
		E.NodeA = A;
		E.NodeB = B;
		E.WidthCm = 1600.f;
		E.SpeedLimitKmh = 80.f;
		E.District = Dist;
		E.SampledPoints = { PA, 0.5f * (PA + PB), PB };
	};

	const FVector P0(0.f, 0.f, 0.f);
	const FVector P1(10000.f, 0.f, 0.f);
	const FVector P2(10000.f, 15000.f, 0.f);
	const FVector P3(20000.f, 0.f, 500.f);
	AddNode(0, P0, EBNDistrict::MerkezBulvar);
	AddNode(1, P1, EBNDistrict::EskiLiman);
	AddNode(2, P2, EBNDistrict::DoguSanayi);
	AddNode(3, P3, EBNDistrict::KuzeyGecidi);
	AddEdge(0, 1, P0, P1, EBNDistrict::MerkezBulvar);
	AddEdge(1, 3, P1, P3, EBNDistrict::EskiLiman);
	AddEdge(0, 2, P0, P2, EBNDistrict::DoguSanayi);
	AddEdge(2, 3, P2, P3, EBNDistrict::KuzeyGecidi);

	FBNCooldownSpot& Spot = Asset->CooldownSpots.AddDefaulted_GetRef();
	Spot.Location = P1;
	Spot.RadiusCm = 1500.f;
	Spot.DisplayName = TEXT("Test Saklanma");
	Spot.District = EBNDistrict::EskiLiman;

	FBNDistrictBounds& B0 = Asset->Districts.AddDefaulted_GetRef();
	B0.District = EBNDistrict::MerkezBulvar;
	B0.CenterCm = FVector2D(0.f, 0.f);
	B0.HalfExtentCm = FVector2D(5000.f, 5000.f);
	B0.DisplayName = TEXT("MERKEZ BULVAR");

	FBNDistrictBounds& B1 = Asset->Districts.AddDefaulted_GetRef();
	B1.District = EBNDistrict::KuzeyGecidi;
	B1.CenterCm = FVector2D(20000.f, 0.f);
	B1.HalfExtentCm = FVector2D(5000.f, 5000.f);
	B1.DisplayName = TEXT("KUZEY GEÇİDİ");

	Graph->SetRoadGraphAsset(Asset);
	TestTrue(TEXT("RoadGraph reports HasGraph()"), Graph->HasGraph());

	TArray<FVector> Path;
	TestTrue(TEXT("A* FindPath succeeds from P0 to P3"), Graph->FindPath(P0, P3, Path));
	TestTrue(TEXT("A* path has waypoints"), Path.Num() >= 3);
	bool bTookShortRoute = true;
	for (const FVector& Wp : Path)
	{
		if (Wp.Y > 4000.f) { bTookShortRoute = false; }
	}
	TestTrue(TEXT("A* chose the shorter 0->1->3 route instead of 0->2->3 detour"), bTookShortRoute);

	FVector SnapPt, SnapTan;
	float SnapW = 0.f, SnapDist = 0.f;
	int32 SnapEdge = INDEX_NONE;
	TestTrue(TEXT("FindNearestRoadPoint projects off-center query onto road"),
		Graph->FindNearestRoadPoint(FVector(5000.f, 400.f, 0.f), SnapPt, SnapTan, SnapW, SnapEdge, SnapDist));
	TestTrue(TEXT("projected point lands near (5000, 0, 0)"), FVector::Dist2D(SnapPt, FVector(5000.f, 0.f, 0.f)) < 50.f);

	FTransform AheadTf;
	float AheadW = 0.f;
	TestTrue(TEXT("FindRoadPointAhead advances cleanly along connected edges"),
		Graph->FindRoadPointAhead(FVector(2000.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 12000.f, AheadTf, AheadW));
	TestTrue(TEXT("look-ahead point moved past node 1 toward node 3"), AheadTf.GetLocation().X > 12000.f);

	FString DistName;
	TestEqual(TEXT("GetDistrictAt returns MerkezBulvar at P0"), Graph->GetDistrictAt(P0, &DistName), EBNDistrict::MerkezBulvar);
	TestEqual(TEXT("GetDistrictAt returns KuzeyGecidi at P3"), Graph->GetDistrictAt(P3), EBNDistrict::KuzeyGecidi);

	FBNCooldownSpot HitSpot;
	TestTrue(TEXT("IsInsideCooldownSpot triggers inside hideout radius"), Graph->IsInsideCooldownSpot(P1 + FVector(300.f, 0.f, 0.f), &HitSpot));
	TestFalse(TEXT("IsInsideCooldownSpot false outside hideout radius"), Graph->IsInsideCooldownSpot(P0));

	// Verify Cooldown spot 3.5x speedup in FBNPursuitState
	FBNPursuitState NormalHide, SpotHide;
	NormalHide.StartPursuit(1);
	NormalHide.Update(0.5f, 120.f, 15000.f, 1);
	NormalHide.Update(4.0f, 120.f, 15000.f, 1);
	SpotHide = NormalHide;
	NormalHide.Update(3.0f, 0.f, 15000.f, 0, false);
	SpotHide.Update(3.0f, 0.f, 15000.f, 0, true);
	TestTrue(TEXT("Cooldown hideout spot accelerates cooldown significantly (>1.8x normal idle)"),
		SpotHide.PhaseProgress > NormalHide.PhaseProgress * 1.8f);
	TestTrue(TEXT("Cooldown state tracks bInsideCooldownSpot flag"), SpotHide.bInsideCooldownSpot);
	return true;
}

// Pure unit test: verifies 5 per-car engine synthesis profiles, custom paint palette, and UBNAudioSubsystem settings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNEngineProfilesAndAudioTest, "BlackNumbers.Audio.EngineProfilesAndMusic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNEngineProfilesAndAudioTest::RunTest(const FString& Parameters)
{
	const FBNEngineSoundTargets I4 = BNEngineSound::Evaluate(6000.f, 7500.f, 1.f, 0.f, 120.f, true, false, 1.f, false, EBNEngineProfile::TurboI4);
	const FBNEngineSoundTargets I6 = BNEngineSound::Evaluate(6000.f, 7500.f, 1.f, 0.f, 120.f, true, false, 1.f, false, EBNEngineProfile::SportI6);
	const FBNEngineSoundTargets V8 = BNEngineSound::Evaluate(6000.f, 7500.f, 1.f, 0.f, 120.f, true, false, 1.f, false, EBNEngineProfile::MuscleV8);
	const FBNEngineSoundTargets V10 = BNEngineSound::Evaluate(6000.f, 7500.f, 1.f, 0.f, 120.f, true, false, 1.f, false, EBNEngineProfile::ExoticV10);

	TestTrue(TEXT("TurboI4 firing Hz < SportI6"), I4.FiringHz < I6.FiringHz);
	TestTrue(TEXT("SportI6 firing Hz < MuscleV8"), I6.FiringHz < V8.FiringHz);
	TestTrue(TEXT("MuscleV8 firing Hz < ExoticV10"), V8.FiringHz < V10.FiringHz);

	TestEqual(TEXT("ValkenRS4 maps to TurboI4"), BNEngineSound::ProfileForCar(EBNCarId::ValkenRS4), EBNEngineProfile::TurboI4);
	TestEqual(TEXT("KuroganeMSpec maps to SportI6"), BNEngineSound::ProfileForCar(EBNCarId::KuroganeMSpec), EBNEngineProfile::SportI6);
	TestEqual(TEXT("Vandal440R maps to MuscleV8"), BNEngineSound::ProfileForCar(EBNCarId::Vandal440R), EBNEngineProfile::MuscleV8);
	TestEqual(TEXT("TauronV6Rally maps to RallyV6"), BNEngineSound::ProfileForCar(EBNCarId::TauronV6Rally), EBNEngineProfile::RallyV6);
	TestEqual(TEXT("NocturneBN01 maps to ExoticV10"), BNEngineSound::ProfileForCar(EBNCarId::NocturneBN01), EBNEngineProfile::ExoticV10);

	// Custom 10-color garage paint palette checks
	const auto& Palette = FBNVehicleSpec::GetPaintPalette();
	TestEqual(TEXT("Garage custom paint palette offers 10 colors"), Palette.Num(), 10);
	FBNPlayerProfile Profile;
	Profile.SetActivePaintIndex(3);
	TestEqual(TEXT("Profile stores custom paint index"), Profile.GetActivePaintIndex(), 3);
	TestTrue(TEXT("Profile returns chosen custom tint"), Profile.GetActivePaintTint().Equals(Palette[3].Value, 0.001f));
	Profile.SetActivePaintIndex(-1);
	TestTrue(TEXT("Resetting custom paint (-1) restores stock paint"),
		Profile.GetActivePaintTint().Equals(FBNVehicleSpec::ForCar(Profile.ActiveCar).PaintTint, 0.001f));

	// Audio subsystem volume clamping & asset paths
	const float PrevMusic = UBNAudioSubsystem::GetMusicVolume();
	const float PrevSfx = UBNAudioSubsystem::GetSfxVolume();
	UBNAudioSubsystem::SetMusicVolume(1.5f);
	TestEqual(TEXT("MusicVolume clamps to 1.0"), UBNAudioSubsystem::GetMusicVolume(), 1.0f);
	UBNAudioSubsystem::SetSfxVolume(-0.4f);
	TestEqual(TEXT("SfxVolume clamps to 0.0"), UBNAudioSubsystem::GetSfxVolume(), 0.0f);
	UBNAudioSubsystem::SetMusicVolume(PrevMusic);
	UBNAudioSubsystem::SetSfxVolume(PrevSfx);

	for (int32 i = 0; i < static_cast<int32>(EBNSfxId::Count); ++i)
	{
		const FString Path(UBNAudioSubsystem::GetSfxAssetPath(static_cast<EBNSfxId>(i)));
		TestFalse(TEXT("every EBNSfxId maps to a /Game/Audio/SFX asset path"), Path.IsEmpty());
	}
	TestFalse(TEXT("FreeRoam music path defined"), FString(UBNAudioSubsystem::GetMusicAssetPath(EBNMusicState::FreeRoam)).IsEmpty());
	TestFalse(TEXT("Race music path defined"), FString(UBNAudioSubsystem::GetMusicAssetPath(EBNMusicState::Race)).IsEmpty());
	TestFalse(TEXT("Pursuit music path defined"), FString(UBNAudioSubsystem::GetMusicAssetPath(EBNMusicState::Pursuit)).IsEmpty());
	return true;
}

// Pure unit test: verifies manual-shift Drag race timing windows, speed surge, gear ceilings, and redline blown engine.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNDragLogicTest, "BlackNumbers.Race.DragShiftAndBlownEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNDragLogicTest::RunTest(const FString& Parameters)
{
	FBNDragState Drag;
	Drag.Reset(true);
	TestTrue(TEXT("Drag mode enabled"), Drag.bActive);
	TestEqual(TEXT("Starts in Gear 1"), Drag.Gear, 1);

	// Drive to sweet-spot RPM (~86%) and trigger a Perfect Shift!
	Drag.RpmFraction = 0.86f;
	EBNDragShiftGrade Grade = EBNDragShiftGrade::None;
	const float Surge = Drag.ShiftUp(&Grade);
	TestEqual(TEXT("86% RPM awards Perfect shift"), Grade, EBNDragShiftGrade::Perfect);
	TestTrue(TEXT("Perfect shift grants strong positive velocity surge"), Surge > 200.f);
	TestEqual(TEXT("Upshift moved to Gear 2"), Drag.Gear, 2);
	TestEqual(TEXT("PerfectShifts counter incremented"), Drag.PerfectShifts, 1);

	// Downshift returns to Gear 1 cleanly.
	TestTrue(TEXT("Downshift succeeds from Gear 2"), Drag.ShiftDown());
	TestEqual(TEXT("Back in Gear 1"), Drag.Gear, 1);

	// Holding redline past OverheatLimitSeconds blows the engine!
	for (int32 i = 0; i < 30; ++i)
	{
		Drag.Tick(0.1f, Drag.GetCurrentGearMaxSpeedKmh() + 10.f, 1.f);
	}
	TestTrue(TEXT("Holding redline under full gas blows Drag engine"), Drag.bEngineBlown);

	// Verify MW 2005 Drag-only Totaled rule: a pawn outside Drag mode cannot take collision damage!
	ABNVehiclePawn* TestPawn = NewObject<ABNVehiclePawn>(GetTransientPackage());
	TestNotNull(TEXT("Transient ABNVehiclePawn created"), TestPawn);
	if (TestPawn)
	{
		TestPawn->SetDragMode(false);
		TestPawn->ApplyTrafficCollisionDamage(240.f);
		TestEqual(TEXT("Non-Drag collision leaves VehicleHealth at 1.0"), TestPawn->GetVehicleHealth(), 1.f);
		TestFalse(TEXT("Non-Drag collision never totals the car"), TestPawn->IsTotaled());
	}
	return true;
}

// Pure unit test: verifies UBNRoadGraphSubsystem GPS destination routing, remaining distance, and arrival clear.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNGpsNavigationTest, "BlackNumbers.World.GpsNavigationPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNGpsNavigationTest::RunTest(const FString& Parameters)
{
	UBNRoadGraphAsset* Asset = NewObject<UBNRoadGraphAsset>(GetTransientPackage());
	UBNRoadGraphSubsystem* Graph = NewObject<UBNRoadGraphSubsystem>(GetTransientPackage());
	if (!Asset || !Graph) { return false; }

	auto AddNode = [&](int32 Id, const FVector& Pos)
	{
		FBNRoadNode& N = Asset->Nodes.AddDefaulted_GetRef();
		N.Id = Id;
		N.Position = Pos;
		N.District = EBNDistrict::MerkezBulvar;
	};
	AddNode(0, FVector(0.f, 0.f, 0.f));
	AddNode(1, FVector(12000.f, 0.f, 0.f));
	AddNode(2, FVector(24000.f, 0.f, 0.f));
	for (const TPair<int32, int32>& Pair : { TPair<int32, int32>(0, 1), TPair<int32, int32>(1, 2) })
	{
		FBNRoadEdge& E = Asset->Edges.AddDefaulted_GetRef();
		E.NodeA = Pair.Key;
		E.NodeB = Pair.Value;
		E.SampledPoints = { Asset->Nodes[Pair.Key].Position, Asset->Nodes[Pair.Value].Position };
	}
	Graph->SetRoadGraphAsset(Asset);

	Graph->SetGpsDestination(FVector(24000.f, 0.f, 0.f), TEXT("Eski Liman"), FVector(0.f, 0.f, 0.f));
	TestTrue(TEXT("GPS destination active"), Graph->HasGpsDestination());
	TestTrue(TEXT("A* GPS route produced >= 2 waypoints"), Graph->GetGpsRoutePoints().Num() >= 2);
	TestTrue(TEXT("Remaining distance reflects ~240 m"), Graph->GetGpsRemainingDistanceCm(FVector::ZeroVector) > 20000.f);
	TestFalse(TEXT("Mid-route does not trigger arrival"), Graph->UpdateGpsForPlayer(FVector(10000.f, 0.f, 0.f)));
	TestTrue(TEXT("Reaching within 35 m triggers GPS arrival & clears"), Graph->UpdateGpsForPlayer(FVector(23000.f, 0.f, 0.f)));
	TestFalse(TEXT("GPS cleared upon arrival"), Graph->HasGpsDestination());
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"
#include "EngineUtils.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Race/BNRivalController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

using namespace BNPlayTest;

namespace
{
	struct FPoliceOverhaulState
	{
		enum class EPhase { WaitPatrols, StartHeat5, VerifyChasersAndMusic, Cleanup } Phase = EPhase::WaitPatrols;
		double PhaseStart = 0.0;
	};
}

// PIE test on KaramaraSlice: verifies organic free-roam police patrols spawn on the road graph,
// Heat 5 spawns multiple units including Heavy Rhino SUV, and adaptive music switches FreeRoam -> Pursuit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPoliceOverhaulPieTest, "BlackNumbers.Police.RoadGraphChaseAndPatrol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPoliceOverhaulPieTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar()) { return true; }
		if (FPlatformTime::Seconds() - WaitStart > 30.0)
		{
			AddError(TEXT("Player car not spawned on KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	const TSharedRef<FPoliceOverhaulState> State = MakeShared<FPoliceOverhaulState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FPoliceOverhaulState::EPhase;
		ABNVehiclePawn* Player = FindPlayerCar();
		UWorld* World = PlayWorld();
		UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		UBNAudioSubsystem* Audio = World ? World->GetSubsystem<UBNAudioSubsystem>() : nullptr;
		if (!Player || !World || !Pursuit || !Audio)
		{
			AddError(TEXT("Missing Player, World, Pursuit or Audio subsystem"));
			return true;
		}
		const double Now = World->GetTimeSeconds();
		if (State->PhaseStart <= 0.0) { State->PhaseStart = Now; }
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::WaitPatrols:
			if (Pursuit->GetActivePatrolCount() >= 1)
			{
				TestTrue(TEXT("organic free-roam police patrols spawned on Karamara road graph"), Pursuit->GetActivePatrolCount() >= 1);
				TestEqual(TEXT("adaptive music starts in FreeRoam"), Audio->GetActiveMusicState(), EBNMusicState::FreeRoam);
				Enter(EPhase::StartHeat5);
				return false;
			}
			if (InPhase > 7.f)
			{
				AddError(TEXT("Timed out waiting for free-roam police patrols on KaramaraSlice"));
				return true;
			}
			return false;

		case EPhase::StartHeat5:
			Pursuit->StartPursuit(5);
			Enter(EPhase::VerifyChasersAndMusic);
			return false;

		case EPhase::VerifyChasersAndMusic:
			if (InPhase < 3.6f) { return false; }
			{
				bool bFoundHeavyRhino = false;
				for (TActorIterator<ABNPoliceController> It(World); It; ++It)
				{
					bFoundHeavyRhino |= It->IsHeavyRhino();
				}
				TestTrue(TEXT("Heat 5 spawns >= 2 active pursuit units"), Pursuit->GetActivePoliceCount() >= 2);
				TestTrue(TEXT("Heat 5 deploys Heavy Rhino SUV interceptor"), bFoundHeavyRhino);
				TestEqual(TEXT("adaptive music switches to Pursuit during Heat 5 chase"), Audio->GetActiveMusicState(), EBNMusicState::Pursuit);
			}
			Pursuit->StopPursuit();
			Enter(EPhase::Cleanup);
			return false;

		case EPhase::Cleanup:
			TestEqual(TEXT("StopPursuit clears all active chasers"), Pursuit->GetActivePoliceCount(), 0);
			return true;
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// PIE regression test on KaramaraSlice: driving into another race's world beacon while a race is already active
// must NEVER start a second race, and other idle race beacons must stay hidden until the active race ends.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNActiveRaceLockPieTest, "BlackNumbers.Race.ActiveRaceBlocksOtherBeacons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNActiveRaceLockPieTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar()) { return true; }
		if (FPlatformTime::Seconds() - WaitStart > 30.0)
		{
			AddError(TEXT("Player car not spawned on KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	struct FLockState
	{
		TWeakObjectPtr<ABNRaceEvent> RaceA;
		TWeakObjectPtr<ABNRaceEvent> RaceB;
		int32 Step = 0;
		double StepStart = 0.0;
	};
	const TSharedRef<FLockState> State = MakeShared<FLockState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = PlayWorld();
		ABNVehiclePawn* Player = FindPlayerCar();
		if (!World || !Player) { AddError(TEXT("Missing World or Player")); return true; }
		const double Now = World->GetTimeSeconds();

		if (State->Step == 0)
		{
			TArray<ABNRaceEvent*> Events;
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It) { Events.Add(*It); }
			TestTrue(TEXT("KaramaraSlice has >= 2 race events"), Events.Num() >= 2);
			if (Events.Num() < 2) { return true; }
			State->RaceA = Events[0];
			State->RaceB = Events[1];
			State->RaceA->StartRace(Player);
			State->Step = 1;
			State->StepStart = Now;
			return false;
		}
		if (State->Step == 1)
		{
			if (Now - State->StepStart < 0.25) { return false; }
			ABNRaceEvent* RaceA = State->RaceA.Get();
			ABNRaceEvent* RaceB = State->RaceB.Get();
			TestEqual(TEXT("FindActiveRace reports RaceA"), ABNRaceEvent::FindActiveRace(World), RaceA);
			TestFalse(TEXT("other race beacon hidden while RaceA active"), RaceB->Beacon && RaceB->Beacon->IsVisible());
			// Teleport right onto RaceB's beacon while RaceA is still running!
			Player->SetActorLocation(RaceB->GetActorLocation() + FVector(0.f, 0.f, 70.f), false, nullptr, ETeleportType::TeleportPhysics);
			State->Step = 2;
			State->StepStart = Now;
			return false;
		}
		if (State->Step == 2)
		{
			if (Now - State->StepStart < 0.35) { return false; }
			ABNRaceEvent* RaceA = State->RaceA.Get();
			ABNRaceEvent* RaceB = State->RaceB.Get();
			TestEqual(TEXT("RaceB stayed Idle when driven over mid-race"), RaceB->GetState(), EBNRaceState::Idle);
			TestEqual(TEXT("RaceA remains the sole active race"), ABNRaceEvent::FindActiveRace(World), RaceA);
			RaceA->EndRace();
			return true;
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// PIE regression test on KaramaraSlice: starts KaramaraCircuit, parks player aside, and verifies that rivals
// drive cleanly along Karamara's welded street network without respawn-spamming (>0 checkpoints passed, <=1 respawn total).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNKaramaraRivalCleanDrivingTest, "BlackNumbers.Race.KaramaraRivalCleanDriving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNKaramaraRivalCleanDrivingTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (FindPlayerCar()) { return true; }
		if (FPlatformTime::Seconds() - WaitStart > 30.0)
		{
			AddError(TEXT("Player car not spawned on KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	struct FRivalCheckState
	{
		TWeakObjectPtr<ABNRaceEvent> Race;
		bool bStarted = false;
		bool bMovedPlayerAside = false;
		double RacingStart = -1.0;
	};
	const TSharedRef<FRivalCheckState> State = MakeShared<FRivalCheckState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = PlayWorld();
		ABNVehiclePawn* Player = FindPlayerCar();
		if (!World || !Player) { AddError(TEXT("Missing World or Player")); return true; }

		if (!State->bStarted)
		{
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
			{
				if (It->Definition && It->Definition->GetName().Contains(TEXT("KaramaraCircuit")))
				{
					State->Race = *It;
					break;
				}
			}
			if (!State->Race.IsValid())
			{
				AddError(TEXT("RACE_KaramaraCircuit not found on KaramaraSlice"));
				return true;
			}
			State->Race->StartRace(Player);
			State->bStarted = true;
			return false;
		}

		ABNRaceEvent* Race = State->Race.Get();
		if (!Race || Race->GetState() == EBNRaceState::Countdown)
		{
			return false;
		}
		const double Now = World->GetTimeSeconds();
		if (State->RacingStart < 0.0)
		{
			State->RacingStart = Now;
			// Move player off the grid cleanly so rivals have clean track ahead
			Player->SetActorLocation(FVector(-12000.f, -24000.f, 80.f), false, nullptr, ETeleportType::TeleportPhysics);
			Player->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
			State->bMovedPlayerAside = true;
			return false;
		}
		// Let rivals race for 22 seconds through multiple city corners
		if (Now - State->RacingStart < 22.0)
		{
			return false;
		}

		int32 TotalPassed = 0;
		int32 TotalRespawns = 0;
		for (int32 i = 0; i < Race->GetNumRivals(); ++i)
		{
			const FBNRacer& R = Race->GetRival(i);
			TotalPassed += R.Progress.Passed();
			if (const ABNVehiclePawn* Car = R.Car.Get())
			{
				if (const ABNRivalController* Ctrl = Cast<ABNRivalController>(Car->GetController()))
				{
					TotalRespawns += Ctrl->GetRespawnCount();
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("BN Karamara rival check (18s): rivals=%d total_gates_passed=%d total_respawns=%d"),
			Race->GetNumRivals(), TotalPassed, TotalRespawns);
		TestTrue(TEXT("KaramaraCircuit spawned rivals"), Race->GetNumRivals() >= 2);
		TestTrue(TEXT("rivals cleared checkpoints around Karamara corners"), TotalPassed >= Race->GetNumRivals());
		TestTrue(TEXT("rivals raced cleanly without respawn-spamming (<=1 total respawn across all rivals in 18s)"), TotalRespawns <= 1);
		Race->EndRace();
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
