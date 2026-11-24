#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageZone.h"
#include "Police/BNPursuitBreaker.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNBreakableProp.h"
#include "World/BNDayNightCycle.h"
#include "World/BNRoadGraph.h"
#include "World/BNStreetLamp.h"

using namespace BNPlayTest;

namespace
{
	// Straight North Viaduct on-ramp climbing north (-Y, Yaw=-90) from Node 1 (-12000,-22000,0) to Node 25 (-12000,-54000,1200)
	const FVector ViaductApproachStart(-12000.f, -22500.f, 70.f);
	const FVector TestPropLocation(-12000.f, -25200.f, 145.f);
	constexpr float ViaductApproachYaw = -90.f;
	constexpr float TargetRampMinZ = 350.f;
	constexpr float DriveTimeoutSeconds = 12.f;

	template <typename ActorType>
	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	struct FKaramaraTestState
	{
		enum class EPhase { VerifyCity, Settle, DriveRamp } Phase = EPhase::VerifyCity;
		double PhaseStart = 0.0;
		TWeakObjectPtr<ABNBreakableProp> SpawnedProp;
	};
}

// Loads /Game/Maps/KaramaraSlice, verifies city districts/races/garage/breakers/props,
// smashes a breakable prop into Sehir Faturasi, and drives up the West Viaduct ramp.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNKaramaraCityTest, "BlackNumbers.City.KaramaraSliceAndBreakables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNKaramaraCityTest::RunTest(const FString& Parameters)
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
			AddError(TEXT("Player was not spawned in KaramaraSlice"));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	const TSharedRef<FKaramaraTestState> State = MakeShared<FKaramaraTestState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FKaramaraTestState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		UWorld* World = PlayWorld();
		UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		if (!Car || !World || !Pursuit)
		{
			AddError(TEXT("Car, PlayWorld or PursuitSubsystem missing in KaramaraSlice"));
			return true;
		}

		const double Now = World->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::VerifyCity:
		{
			TestTrue(TEXT("Karamara has day/night cycle"), CountActors<ABNDayNightCycle>(World) >= 1);
			TestTrue(TEXT("Karamara has >= 60 street lamps across 6 districts"), CountActors<ABNStreetLamp>(World) >= 60);
			TestTrue(TEXT("Karamara has Selin's Garage"), CountActors<ABNGarageZone>(World) >= 1);
			TestTrue(TEXT("Karamara has 10 Pursuit Breakers"), CountActors<ABNPursuitBreaker>(World) >= 10);
			TestTrue(TEXT("Karamara has >= 50 Breakable Props"), CountActors<ABNBreakableProp>(World) >= 50);

			const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>();
			TestNotNull(TEXT("RoadGraphSubsystem present"), RoadGraph);
			if (RoadGraph)
			{
				TestTrue(TEXT("RoadGraph loaded DA_KaramaraRoadGraph"), RoadGraph->HasGraph());
				TestTrue(TEXT("RoadGraph has >= 40 nodes"), RoadGraph->GetNodes().Num() >= 40);
				TestTrue(TEXT("RoadGraph has >= 60 edges"), RoadGraph->GetEdges().Num() >= 60);
				TestTrue(TEXT("RoadGraph has 6 Cooldown spots"), RoadGraph->GetCooldownSpots().Num() >= 6);
				float MaxNodeZ = 0.f;
				for (const FBNRoadNode& N : RoadGraph->GetNodes())
				{
					MaxNodeZ = FMath::Max(MaxNodeZ, N.Position.Z);
				}
				TestTrue(TEXT("Kuzey Gecidi summit reaches >= 50 m (5000 cm)"), MaxNodeZ >= 5000.f);
			}

			int32 RaceCount = 0;
			bool bCircuit = false, bSprint = false, bDrift = false;
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
			{
				if (!It->Definition)
				{
					continue;
				}
				++RaceCount;
				bCircuit |= It->Definition->Discipline == EBNRaceDiscipline::Circuit;
				bSprint |= It->Definition->Discipline == EBNRaceDiscipline::Sprint;
				bDrift |= It->Definition->Discipline == EBNRaceDiscipline::Drift;
			}
			TestTrue(TEXT("Karamara has 9 unique races"), RaceCount >= 9);
			TestTrue(TEXT("Karamara has Circuit race"), bCircuit);
			TestTrue(TEXT("Karamara has Sprint race"), bSprint);
			TestTrue(TEXT("Karamara has Drift race"), bDrift);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			State->SpawnedProp = World->SpawnActor<ABNBreakableProp>(ABNBreakableProp::StaticClass(), TestPropLocation, FRotator::ZeroRotator, Params);
			TestNotNull(TEXT("spawned test breakable prop"), State->SpawnedProp.Get());

			PlaceStopped(*Car, ViaductApproachStart, ViaductApproachYaw);
			Enter(EPhase::Settle);
			return false;
		}
		case EPhase::Settle:
			if (InPhase >= 0.8f)
			{
				Car->GetChaosMovement()->SetThrottleInput(1.f);
				Enter(EPhase::DriveRamp);
			}
			return false;
		case EPhase::DriveRamp:
		{
			if (InPhase > DriveTimeoutSeconds)
			{
				AddError(FString::Printf(TEXT("Timed out climbing Kuzey Viyadugu ramp (Z=%.1f)"), Car->GetActorLocation().Z));
				return true;
			}
			const ABNBreakableProp* Prop = State->SpawnedProp.Get();
			if (Prop && Prop->IsBroken() && Car->GetActorLocation().Z >= TargetRampMinZ)
			{
				UE_LOG(LogTemp, Display, TEXT("BN Karamara test: prop broken (cost=$%d), climbed ramp to Z=%.1f cm"),
					Pursuit->GetCostToStateDollars(), Car->GetActorLocation().Z);
				TestTrue(TEXT("smashed prop recorded Sehir Faturasi"), Pursuit->GetCostToStateDollars() >= Prop->CollateralCostDollars);
				Car->GetChaosMovement()->SetThrottleInput(0.f);
				if (ABNBreakableProp* MutableProp = State->SpawnedProp.Get())
				{
					MutableProp->Destroy();
				}
				return true;
			}
			return false;
		}
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
