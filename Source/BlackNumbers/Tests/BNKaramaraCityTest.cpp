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
#include "World/BNStreetLamp.h"

using namespace BNPlayTest;

namespace
{
	const FVector ViaductApproachStart(-25500.f, -24000.f, 60.f);
	const FVector TestPropLocation(-23700.f, -24000.f, 42.5f);
	constexpr float TargetRampMinZ = 250.f;
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
			TestTrue(TEXT("Karamara has >= 25 street lamps"), CountActors<ABNStreetLamp>(World) >= 25);
			TestTrue(TEXT("Karamara has Selin's Garage"), CountActors<ABNGarageZone>(World) >= 1);
			TestTrue(TEXT("Karamara has >= 4 Pursuit Breakers"), CountActors<ABNPursuitBreaker>(World) >= 4);
			TestTrue(TEXT("Karamara has >= 15 Breakable Props"), CountActors<ABNBreakableProp>(World) >= 15);

			bool bCircuit = false, bSprint = false, bDrift = false;
			for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
			{
				if (!It->Definition)
				{
					continue;
				}
				bCircuit |= It->Definition->Discipline == EBNRaceDiscipline::Circuit;
				bSprint |= It->Definition->Discipline == EBNRaceDiscipline::Sprint;
				bDrift |= It->Definition->Discipline == EBNRaceDiscipline::Drift;
			}
			TestTrue(TEXT("Karamara has Circuit race"), bCircuit);
			TestTrue(TEXT("Karamara has Sprint race"), bSprint);
			TestTrue(TEXT("Karamara has Drift race"), bDrift);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			State->SpawnedProp = World->SpawnActor<ABNBreakableProp>(ABNBreakableProp::StaticClass(), TestPropLocation, FRotator::ZeroRotator, Params);
			TestNotNull(TEXT("spawned test breakable prop"), State->SpawnedProp.Get());

			PlaceStopped(*Car, ViaductApproachStart, 0.f);
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
