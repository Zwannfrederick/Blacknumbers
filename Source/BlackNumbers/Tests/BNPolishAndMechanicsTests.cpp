#include "Misc/AutomationTest.h"

#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStoryTypes.h"
#include "Vehicle/BNVehicleBoost.h"
#include "World/BNDayNightCycle.h"
#include "World/BNRoadGraph.h"
#include "World/BNWorldActivities.h"

#if WITH_DEV_AUTOMATION_TESTS

// Pure unit test: verifies 6-borough territory control, District Rulers (#08/#07/#06/#05/#03/#01),
// and monotonic liberation progression 0 -> 6 as Blacklist bosses fall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNTerritoryAndDistrictRulersTest, "BlackNumbers.Story.TerritoryAndDistrictRulers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNTerritoryAndDistrictRulersTest::RunTest(const FString& Parameters)
{
	const TArray<FBNBossSpec>& Bosses = FBNStoryState::GetAllBosses();
	TestEqual(TEXT("Blacklist has 16 bosses"), Bosses.Num(), 16);

	int32 RulerCount = 0;
	TSet<EBNDistrict> CoveredDistricts;
	for (const FBNBossSpec& B : Bosses)
	{
		const EBNDistrict Dist = FBNStoryState::GetDistrictForBoss(B.Rank);
		TestEqual(TEXT("Boss HomeDistrict matches GetDistrictForBoss"), B.HomeDistrict, Dist);
		CoveredDistricts.Add(Dist);
		if (FBNStoryState::IsDistrictRulerRank(B.Rank))
		{
			++RulerCount;
		}
	}
	TestEqual(TEXT("All 6 districts covered by Blacklist crews"), CoveredDistricts.Num(), 6);
	TestEqual(TEXT("Exactly 6 District Rulers across Karamara"), RulerCount, 6);

	FBNStoryState State;
	State.Reset();
	TestEqual(TEXT("Zero districts liberated at campaign start"), State.GetLiberatedDistrictCount(), 0);

	TArray<FBNStoryCard> Cards;
	int32 PrevLiberated = 0;
	for (int32 Rank = 16; Rank >= 1; --Rank)
	{
		Cards.Reset();
		TestTrue(TEXT("RecordBossVictory succeeds in order"), State.RecordBossVictory(Rank, Cards));
		const int32 NowLiberated = State.GetLiberatedDistrictCount();
		TestTrue(TEXT("Liberated district count never decreases"), NowLiberated >= PrevLiberated);
		if (FBNStoryState::IsDistrictRulerRank(Rank))
		{
			const FBNDistrictTerritoryInfo Info = State.GetDistrictTerritory(FBNStoryState::GetDistrictForBoss(Rank));
			TestTrue(TEXT("Ruler defeat liberates borough territory"), Info.bDistrictLiberated && Info.bRulerDefeated);
		}
		PrevLiberated = NowLiberated;
	}
	TestEqual(TEXT("All 6 boroughs liberated after defeating #01 Viktor Serez"), State.GetLiberatedDistrictCount(), 6);
	return true;
}

// Pure unit test: Slipstream drafting math, Difficulty rival auto-tier scaling, and single-source payout parity.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNSlipstreamDifficultyAndPayoutsTest, "BlackNumbers.Mechanics.SlipstreamDifficultyAndPayouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNSlipstreamDifficultyAndPayoutsTest::RunTest(const FString& Parameters)
{
	FBNBoostTuning Tuning;
	FBNBoostState Boost;
	Boost.NosFraction = 0.50f;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Boost.UpdateSlipstream(0.1f, true, false, true, 175.f, Tuning);
	}
	TestTrue(TEXT("Slipstream engages in draft pocket above 120 km/h"), Boost.bSlipstreaming);
	TestTrue(TEXT("Slipstream builds meaningful charge"), Boost.SlipstreamCharge > 0.7f);
	TestTrue(TEXT("Slipstream delivers forward acceleration boost"), Boost.GetSlipstreamAccelCmPerSec2(Tuning) > 200.f);
	TestTrue(TEXT("Slipstream recharges NOS tank"), Boost.NosFraction > 0.55f);

	for (int32 Step = 0; Step < 25; ++Step)
	{
		Boost.UpdateSlipstream(0.1f, false, false, true, 175.f, Tuning);
	}
	TestFalse(TEXT("Slipstream disengages after leaving draft pocket"), Boost.bSlipstreaming);
	TestEqual(TEXT("Slipstream charge decays to 0"), Boost.SlipstreamCharge, 0.f);

	// Difficulty scaling: Easy < StreetAutoTier < Legend, and higher player upgrade tier raises AutoTier rivals
	float SkillEasy = 0.f, SkillStreetLow = 0.f, SkillStreetHigh = 0.f, SkillLegend = 0.f;
	const FBNComputedVehicleStats EasyStats = ABNRaceEvent::ComputeScaledRivalStats(EBNCarId::KuroganeMSpec, 1, EBNDifficulty::Easy, 0.85f, SkillEasy);
	const FBNComputedVehicleStats StreetLow = ABNRaceEvent::ComputeScaledRivalStats(EBNCarId::KuroganeMSpec, 1, EBNDifficulty::StreetAutoTier, 0.85f, SkillStreetLow);
	const FBNComputedVehicleStats StreetHigh = ABNRaceEvent::ComputeScaledRivalStats(EBNCarId::KuroganeMSpec, 4, EBNDifficulty::StreetAutoTier, 0.85f, SkillStreetHigh);
	const FBNComputedVehicleStats LegendStats = ABNRaceEvent::ComputeScaledRivalStats(EBNCarId::KuroganeMSpec, 4, EBNDifficulty::Legend, 0.85f, SkillLegend);

	TestTrue(TEXT("Easy skill < StreetAutoTier skill"), SkillEasy < SkillStreetLow);
	TestTrue(TEXT("StreetAutoTier skill < Legend skill"), SkillStreetHigh < SkillLegend);
	TestTrue(TEXT("Higher player upgrade tier scales AutoTier rival engine torque"), StreetHigh.MaxTorqueNm > StreetLow.MaxTorqueNm);
	TestTrue(TEXT("Legend rival torque >= Easy rival torque"), LegendStats.MaxTorqueNm >= EasyStats.MaxTorqueNm);

	// Payout single-source-of-truth parity between EstimateRacePayout and AwardRaceResult
	for (const bool bDrift : { false, true })
	{
		for (int32 Place = 1; Place <= 4; ++Place)
		{
			int32 EstCash = 0, EstRep = 0;
			FBNPlayerProfile::EstimateRacePayout(bDrift, Place, 4, 12500, EstCash, EstRep);
			FBNPlayerProfile Profile;
			int32 ActCash = 0, ActRep = 0;
			Profile.AwardRaceResult(Place, 4, bDrift, 12500, ActCash, ActRep);
			TestEqual(TEXT("EstimateRacePayout cash matches AwardRaceResult"), EstCash, ActCash);
			TestEqual(TEXT("EstimateRacePayout rep matches AwardRaceResult"), EstRep, ActRep);
		}
	}
	return true;
}

// Pure unit test: Personal Bests, Paint Finish, Rim Paint, Underglow, and Difficulty round-trip via UBNSaveGame.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPersonalBestAndSaveRoundTripTest, "BlackNumbers.Mechanics.PersonalBestAndSaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPersonalBestAndSaveRoundTripTest::RunTest(const FString& Parameters)
{
	FBNPlayerProfile Src;
	Src.SetActivePaintFinish(EBNPaintFinish::MatteCarbon);
	Src.SetActiveRimPaintIndex(3);
	Src.SetActiveUnderglowIndex(2);
	Src.Difficulty = EBNDifficulty::Legend;
	Src.BestRaceTimes.Add(TEXT("RACE_KaramaraCircuit"), 74.25f);
	Src.BestDriftScores.Add(TEXT("RACE_SanayiDrift"), 42800);
	Src.BestSpeedTrapKmh.Add(TEXT("TRAP_Viyaduk"), 248.5f);
	Src.BestJumpMeters.Add(TEXT("JUMP_Sanayi"), 61.4f);

	UBNSaveGame* SaveObj = Cast<UBNSaveGame>(UGameplayStatics::CreateSaveGameObject(UBNSaveGame::StaticClass()));
	TestNotNull(TEXT("UBNSaveGame object created"), SaveObj);
	if (!SaveObj) { return false; }

	for (int32 c = 0; c < FBNUpgradeSpec::NumCars; ++c)
	{
		SaveObj->PaintFinishes.Add(static_cast<uint8>(Src.PaintFinishes[c]));
		SaveObj->RimPaintIndices.Add(Src.RimPaintIndices[c]);
		SaveObj->UnderglowIndices.Add(Src.UnderglowIndices[c]);
	}
	SaveObj->Difficulty = static_cast<uint8>(Src.Difficulty);
	SaveObj->BestRaceTimes = Src.BestRaceTimes;
	SaveObj->BestDriftScores = Src.BestDriftScores;
	SaveObj->BestSpeedTrapKmh = Src.BestSpeedTrapKmh;
	SaveObj->BestJumpMeters = Src.BestJumpMeters;

	const FString TempSlot = TEXT("BN_PolishTest_TempSlot");
	TestTrue(TEXT("Temp SaveGame writes to disk"), UGameplayStatics::SaveGameToSlot(SaveObj, TempSlot, 0));

	UBNSaveGame* Loaded = Cast<UBNSaveGame>(UGameplayStatics::LoadGameFromSlot(TempSlot, 0));
	UGameplayStatics::DeleteGameInSlot(TempSlot, 0);
	TestNotNull(TEXT("Temp SaveGame reloads from disk"), Loaded);
	if (!Loaded) { return false; }

	TestEqual(TEXT("PaintFinish persisted"), static_cast<EBNPaintFinish>(Loaded->PaintFinishes[0]), EBNPaintFinish::MatteCarbon);
	TestEqual(TEXT("RimPaintIndex persisted"), Loaded->RimPaintIndices[0], 3);
	TestEqual(TEXT("UnderglowIndex persisted"), Loaded->UnderglowIndices[0], 2);
	TestEqual(TEXT("Difficulty persisted"), static_cast<EBNDifficulty>(Loaded->Difficulty), EBNDifficulty::Legend);
	TestTrue(TEXT("BestRaceTime persisted"), FMath::IsNearlyEqual(Loaded->BestRaceTimes.FindRef(TEXT("RACE_KaramaraCircuit")), 74.25f, 0.01f));
	TestEqual(TEXT("BestDriftScore persisted"), Loaded->BestDriftScores.FindRef(TEXT("RACE_SanayiDrift")), 42800);
	TestTrue(TEXT("BestSpeedTrapKmh persisted"), FMath::IsNearlyEqual(Loaded->BestSpeedTrapKmh.FindRef(TEXT("TRAP_Viyaduk")), 248.5f, 0.01f));
	TestTrue(TEXT("BestJumpMeters persisted"), FMath::IsNearlyEqual(Loaded->BestJumpMeters.FindRef(TEXT("JUMP_Sanayi")), 61.4f, 0.01f));
	return true;
}

// Pure unit test: Location-driven asphalt wetness & regional sun azimuth / atmosphere variation across 6 boroughs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNDistrictAtmosphereAndWeatherTest, "BlackNumbers.World.DistrictAtmosphereAndWeather",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNDistrictAtmosphereAndWeatherTest::RunTest(const FString& Parameters)
{
	const FBNDistrictAtmosphereProfile Liman = ABNDayNightCycle::GetDistrictAtmosphere(EBNDistrict::EskiLiman);
	const FBNDistrictAtmosphereProfile Viyaduk = ABNDayNightCycle::GetDistrictAtmosphere(EBNDistrict::KuzeyViyadugu);
	const FBNDistrictAtmosphereProfile Gecit = ABNDayNightCycle::GetDistrictAtmosphere(EBNDistrict::KuzeyGecidi);
	const FBNDistrictAtmosphereProfile Yamac = ABNDayNightCycle::GetDistrictAtmosphere(EBNDistrict::YamacMahallesi);

	TestTrue(TEXT("Eski Liman coastal tarmac is wetter than elevated Kuzey Viyadugu"), Liman.Wetness > Viyaduk.Wetness + 0.3f);
	TestTrue(TEXT("District sun azimuth shifts meaningfully across city boroughs"),
		FMath::Abs(Liman.SunYawOffsetDeg - Yamac.SunYawOffsetDeg) >= 15.f);
	TestTrue(TEXT("Mountain pass fog density differs from viaduct"),
		FMath::Abs(Gecit.FogDensityScale - Viyaduk.FogDensityScale) > 0.1f);

	const float AutoDry = ABNDayNightCycle::ComputeEffectiveWetness(EBNDistrict::KuzeyViyadugu, EBNWeatherMode::AutoDistrict);
	const float Damp = ABNDayNightCycle::ComputeEffectiveWetness(EBNDistrict::KuzeyViyadugu, EBNWeatherMode::WetDamp);
	const float Rain = ABNDayNightCycle::ComputeEffectiveWetness(EBNDistrict::KuzeyViyadugu, EBNWeatherMode::LightRain);
	TestTrue(TEXT("WetDamp elevates wetness over dry viaduct AutoDistrict"), Damp > AutoDry + 0.25f);
	TestTrue(TEXT("LightRain saturates wetness above WetDamp"), Rain > Damp + 0.15f);
	return true;
}

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

// PIE test: verifies 4-mode camera + Look Back, Paint/Rim/Underglow (without polluting CarLights),
// Speed Traps, Stunt Jumps, Khronos GrandTourer mesh, sculpted aero meshes, and CC0 world assets.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPolishVisualsAndActivitiesPIETest, "BlackNumbers.Visuals.CustomizationCamerasAndActivitiesPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPolishVisualsAndActivitiesPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/KaramaraSlice")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WaitStart]()
	{
		if (BNPlayTest::FindPlayerCar())
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
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.6f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		ABNVehiclePawn* Car = BNPlayTest::FindPlayerCar();
		UWorld* World = BNPlayTest::PlayWorld();
		TestNotNull(TEXT("Player car present"), Car);
		TestNotNull(TEXT("PIE world present"), World);
		if (!Car || !World)
		{
			return true;
		}

		// 1. Open-world activities placed across Karamara
		int32 TrapCount = 0;
		for (TActorIterator<ABNSpeedTrap> It(World); It; ++It) { ++TrapCount; }
		int32 JumpCount = 0;
		for (TActorIterator<ABNStuntJump> It(World); It; ++It) { ++JumpCount; }
		TestTrue(TEXT("Karamara has 6 roadside Speed Traps"), TrapCount >= 6);
		TestTrue(TEXT("Karamara has 4 Stunt Jump ramps"), JumpCount >= 4);

		// 2. 4-mode camera cycle (Near -> Far -> Hood -> Bumper -> Near) + Look Back
		Car->CycleChaseDistance(); // 0 -> 1 (Far)
		TestFalse(TEXT("Cam index 1 is Far (not Hood)"), Car->IsHoodCamera());
		Car->CycleChaseDistance(); // 1 -> 2 (Hood)
		TestTrue(TEXT("Cam index 2 is Hood"), Car->IsHoodCamera());
		Car->CycleChaseDistance(); // 2 -> 3 (Bumper)
		TestFalse(TEXT("Cam index 3 is Bumper (not Hood)"), Car->IsHoodCamera());
		TestTrue(TEXT("Cam index 3 is Bumper"), Car->IsBumperCamera());
		Car->CycleChaseDistance(); // 3 -> 0 (Near)
		TestFalse(TEXT("Cam cycles cleanly back to Near"), Car->IsBumperCamera() || Car->IsHoodCamera());

		Car->SetLookBackWanted(true);
		TestTrue(TEXT("Look Back flag activates"), Car->IsLookingBack());
		Car->SetLookBackWanted(false);
		TestFalse(TEXT("Look Back flag releases cleanly"), Car->IsLookingBack());

		// 3. Paint finish, Rim paint, and Underglow without polluting CarLights
		const int32 BaseCarLights = Car->CarLights.Num();
		Car->ApplyPaintFinish(EBNPaintFinish::Satin);
		Car->ApplyRimPaint(FLinearColor(0.84f, 0.64f, 0.24f, 1.f));
		Car->ApplyUnderglow(FLinearColor(0.10f, 0.78f, 1.0f, 1.f), true);
		TestEqual(TEXT("Underglow and showcase lights do not pollute CarLights array"), Car->CarLights.Num(), BaseCarLights);
		Car->ApplyUnderglow(FLinearColor::Black, false);

		// 4. Verify new visual assets exist in /Game/
		TestTrue(TEXT("Khronos CarConcept GrandTourer mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/GrandTourer/SM_BN_GrandTourer")));
		TestTrue(TEXT("CC0 BMW M6 (Valken RS-4) mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/External/SM_Ext_ValkenRS4")));
		TestTrue(TEXT("CC0 Pontiac GTO (Vandal 440R) mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/External/SM_Ext_Vandal440R")));
		TestTrue(TEXT("CC-BY BMW X5 (Tauron V6 Rally) mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/External/SM_Ext_TauronV6Rally")));
		TestTrue(TEXT("CC-BY BMW X5 (Heavy Rhino SUV) mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/External/SM_Ext_HeavyRhino")));
		TestTrue(TEXT("3D modular GlassTower building mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Bldg_GlassTower")));
		TestTrue(TEXT("3D modular AptBrick building mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Bldg_AptBrick")));
		TestTrue(TEXT("3D modular YamacVillaA building mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Bldg_YamacVillaA")));
		TestTrue(TEXT("3D modular FactoryHall building mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Bldg_FactoryHall")));
		TestTrue(TEXT("Sculpted GT wing glTF mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Aero_GTWing")));
		TestTrue(TEXT("Traffic SUV glTF mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Traffic_SUV")));
		TestTrue(TEXT("Cloud sky dome glTF mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_SkyDome")));
		TestTrue(TEXT("Stunt jump ramp glTF mesh exists"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_JumpRamp")));
		TestTrue(TEXT("Poly Haven CC0 fire hydrant mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/CC0/SM_CC0_FireHydrant")));
		TestTrue(TEXT("Poly Haven CC0 iron gate mesh imported"), FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/CC0/SM_CC0_IronGate")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
