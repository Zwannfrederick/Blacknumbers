#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "EngineUtils.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Race/BNRivalController.h"
#include "Tests/BNPlayTestUtils.h"

using namespace BNPlayTest;

namespace
{
	/** One scripted step: called every frame with seconds spent in it; returns true when done. */
	using FRaceStep = TFunction<bool(FAutomationTestBase&, ABNVehiclePawn&, ABNRaceEvent&, float)>;

	ABNRaceEvent* FindRace(EBNRaceDiscipline Discipline)
	{
		for (TActorIterator<ABNRaceEvent> It(PlayWorld()); It; ++It)
		{
			if (It->Definition && It->Definition->Discipline == Discipline)
			{
				return *It;
			}
		}
		return nullptr;
	}

	void RunRaceScript(FAutomationTestBase& Test, EBNRaceDiscipline Discipline, TArray<FRaceStep> Steps, float StepTimeoutSeconds = 20.f)
	{
		StartTestTrack(Test);
		struct FScript { TArray<FRaceStep> Steps; int32 Index = 0; double StepStart = -1.0; };
		const TSharedRef<FScript> Script = MakeShared<FScript>();
		Script->Steps = MoveTemp(Steps);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Discipline, Script, StepTimeoutSeconds]()
		{
			ABNVehiclePawn* Car = FindPlayerCar();
			ABNRaceEvent* Race = FindRace(Discipline);
			if (!Car || !Race)
			{
				Test.AddError(TEXT("Player car or race event missing from the test track"));
				return true;
			}
			const double Now = Car->GetWorld()->GetTimeSeconds();
			if (Script->StepStart < 0.0)
			{
				Script->StepStart = Now;
			}
			const float InStep = static_cast<float>(Now - Script->StepStart);
			if (InStep > StepTimeoutSeconds)
			{
				Test.AddError(FString::Printf(TEXT("Race script step %d timed out"), Script->Index));
				return true;
			}
			if (!Script->Steps[Script->Index](Test, *Car, *Race, InStep))
			{
				return false;
			}
			Script->StepStart = -1.0;
			return ++Script->Index >= Script->Steps.Num();
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}

	constexpr float FrameGapSeconds = 0.05f;

	void Teleport(ABNVehiclePawn& Car, const FVector& Location, float Yaw)
	{
		Car.SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	}

	/** Places the car just before gate Index, then just past it, a few frames apart, so the race sees a crossing. */
	void AddGateCrossing(TArray<FRaceStep>& Steps, int32 Index)
	{
		for (const float Offset : { -400.f, 400.f })
		{
			Steps.Add([Index, Offset](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float InStep)
			{
				if (InStep == 0.f)
				{
					const FTransform& Gate = Race.Definition->Checkpoints[Index];
					Teleport(Car, Gate.TransformPosition(FVector(Offset, 0.f, 60.f)), Gate.Rotator().Yaw);
				}
				return InStep >= FrameGapSeconds;
			});
		}
	}

	FRaceStep WaitForState(EBNRaceState Wanted)
	{
		return [Wanted](FAutomationTestBase&, ABNVehiclePawn&, ABNRaceEvent& Race, float) { return Race.GetState() == Wanted; };
	}
}

// Circuit: the countdown holds the car, an out-of-order gate does not count, two laps of gates finish the race.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNCircuitRaceTest, "BlackNumbers.Race.CircuitByGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNCircuitRaceTest::RunTest(const FString& Parameters)
{
	TArray<FRaceStep> Steps;
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn& Car, ABNRaceEvent& Race, float)
	{
		Race.StartRace(&Car);
		Car.GetChaosMovement()->SetThrottleInput(1.f); // the lock must override this
		Test.TestEqual(TEXT("race starts with a countdown"), Race.GetState(), EBNRaceState::Countdown);
		Test.TestEqual(TEXT("one car per rival skill on the grid"), Race.GetNumRivals(), Race.Definition->RivalSkills.Num());
		return true;
	});
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn& Car, ABNRaceEvent& Race, float)
	{
		if (Race.GetState() != EBNRaceState::Racing)
		{
			Test.TestTrue(TEXT("car held on the grid during the countdown"), FMath::Abs(Car.GetSpeedKmh()) < 3.f);
			return false;
		}
		Test.TestFalse(TEXT("controls unlocked at GO"), Car.IsInputLocked());
		return true;
	});
	AddGateCrossing(Steps, 2); // out of order: must be ignored
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Test.TestEqual(TEXT("an out-of-order gate does not count"), Race.GetProgress().NextCheckpoint(), 1);
		return true;
	});
	for (int32 Lap = 0; Lap < 2; ++Lap)
	{
		for (const int32 Gate : { 1, 2, 3, 0 })
		{
			AddGateCrossing(Steps, Gate);
		}
	}
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Test.TestEqual(TEXT("race finished"), Race.GetState(), EBNRaceState::Finished);
		Test.TestTrue(TEXT("both laps recorded"), Race.GetProgress().IsFinished() && Race.GetProgress().LapTimes().Num() == 2);
		Test.TestTrue(TEXT("finish time recorded"), Race.GetProgress().FinishTime() > 0.0);
		return true;
	});
	RunRaceScript(*this, EBNRaceDiscipline::Circuit, MoveTemp(Steps));
	return true;
}

// Sprint: driving into the beacon starts it (after leaving it once), gates in order finish it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNSprintRaceTest, "BlackNumbers.Race.SprintFromBeacon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNSprintRaceTest::RunTest(const FString& Parameters)
{
	TArray<FRaceStep> Steps;
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float InStep)
	{
		if (InStep == 0.f)
		{
			Teleport(Car, Race.GetActorLocation() + FVector(0.f, 3000.f, 60.f), 0.f); // well outside: arms the beacon
		}
		return InStep >= FrameGapSeconds;
	});
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float InStep)
	{
		if (InStep == 0.f)
		{
			Teleport(Car, Race.GetActorLocation() + FVector(0.f, 0.f, 60.f), 0.f);
		}
		return InStep >= FrameGapSeconds;
	});
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn& Car, ABNRaceEvent& Race, float)
	{
		Test.TestEqual(TEXT("driving into the beacon starts the countdown"), Race.GetState(), EBNRaceState::Countdown);
		Test.TestTrue(TEXT("car moved to the grid"),
			FVector::Dist2D(Car.GetActorLocation(), Race.Definition->Grid.GetLocation()) < 200.f);
		return true;
	});
	Steps.Add(WaitForState(EBNRaceState::Racing));
	for (const int32 Gate : { 0, 1, 2 })
	{
		AddGateCrossing(Steps, Gate);
	}
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Test.TestEqual(TEXT("sprint finished at the last gate"), Race.GetState(), EBNRaceState::Finished);
		Test.TestTrue(TEXT("sprint time recorded"), Race.GetProgress().IsFinished() && Race.GetProgress().FinishTime() > 0.0);
		Test.TestTrue(TEXT("the sprint has rivals"), Race.GetNumRivals() > 0);
		Test.TestEqual(TEXT("first to the line is placed 1st"), Race.GetPlayerPosition(), 1);
		return true;
	});
	RunRaceScript(*this, EBNRaceDiscipline::Sprint, MoveTemp(Steps));
	return true;
}

// Drift: a real handbrake drift on the skidpad during the race scores points; ending the race banks them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNDriftRaceTest, "BlackNumbers.Race.DriftScores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNDriftRaceTest::RunTest(const FString& Parameters)
{
	constexpr float EntryKmh = 70.f;
	constexpr float KickSeconds = 0.7f;
	constexpr float HoldSeconds = 3.f;
	constexpr float CountersteerFullLockDegrees = 30.f;

	TArray<FRaceStep> Steps;
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float) { Race.StartRace(&Car); return true; });
	Steps.Add(WaitForState(EBNRaceState::Racing));
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent&, float)
	{
		Car.GetChaosMovement()->SetThrottleInput(1.f);
		return Car.GetSpeedKmh() >= EntryKmh;
	});
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent&, float InStep)
	{
		UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
		Move->SetThrottleInput(0.5f);
		Move->SetSteeringInput(1.f);
		Move->SetHandbrakeInput(true);
		return InStep >= KickSeconds;
	});
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent&, float InStep)
	{
		UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
		Move->SetHandbrakeInput(false);
		Move->SetThrottleInput(0.6f);
		Move->SetSteeringInput(FMath::Clamp(Car.GetSlipAngleDegrees() / CountersteerFullLockDegrees, -1.f, 1.f));
		return InStep >= HoldSeconds;
	});
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Race.EndRace();
		UE_LOG(LogTemp, Display, TEXT("BN drift race test: %d points"), Race.GetDriftScore().Total());
		Test.TestEqual(TEXT("race over"), Race.GetState(), EBNRaceState::Finished);
		Test.TestTrue(TEXT("the drift scored points"), Race.GetDriftScore().Total() > 0);
		return true;
	});
	RunRaceScript(*this, EBNRaceDiscipline::Drift, MoveTemp(Steps));
	return true;
}

// Backspace / View gives up the race: results show it abandoned, the rivals go when the results close.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNAbandonRaceTest, "BlackNumbers.Race.AbandonEndsRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNAbandonRaceTest::RunTest(const FString& Parameters)
{
	TArray<FRaceStep> Steps;
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float) { Race.StartRace(&Car); return true; });
	Steps.Add(WaitForState(EBNRaceState::Racing));
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn& Car, ABNRaceEvent& Race, float)
	{
		Car.AbandonRace();
		Test.TestEqual(TEXT("abandoning ends the race"), Race.GetState(), EBNRaceState::Finished);
		Test.TestFalse(TEXT("an abandoned race is not a finish"), Race.GetProgress().IsFinished());
		Test.TestFalse(TEXT("controls are free again"), Car.IsInputLocked());
		return true;
	});
	Steps.Add(WaitForState(EBNRaceState::Idle));
	Steps.Add([](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Test.TestEqual(TEXT("rivals removed after the results"), Race.GetNumRivals(), 0);
		return true;
	});
	RunRaceScript(*this, EBNRaceDiscipline::Sprint, MoveTemp(Steps));
	return true;
}

// Rivals alone: after GO the player parks off the track and every rival drives both laps of the oval,
// corners included, without crashing out (no respawns) and in a sane time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNRivalsCircuitTest, "BlackNumbers.Race.RivalsFinishCircuit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNRivalsCircuitTest::RunTest(const FString& Parameters)
{
	constexpr float RaceTimeoutSeconds = 150.f;
	constexpr double MaxLapSeconds = 60.0;
	const FVector ParkedOffTrack(-18000.f, 0.f, 60.f);

	TArray<FRaceStep> Steps;
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent& Race, float) { Race.StartRace(&Car); return true; });
	Steps.Add(WaitForState(EBNRaceState::Racing));
	Steps.Add([ParkedOffTrack](FAutomationTestBase&, ABNVehiclePawn& Car, ABNRaceEvent&, float) { Teleport(Car, ParkedOffTrack, 0.f); return true; });
	Steps.Add([](FAutomationTestBase&, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		for (int32 i = 0; i < Race.GetNumRivals(); ++i)
		{
			if (!Race.GetRival(i).Progress.IsFinished())
			{
				return false;
			}
		}
		return true;
	});
	Steps.Add([MaxLapSeconds](FAutomationTestBase& Test, ABNVehiclePawn&, ABNRaceEvent& Race, float)
	{
		Test.TestTrue(TEXT("the oval has rivals"), Race.GetNumRivals() > 0);
		for (int32 i = 0; i < Race.GetNumRivals(); ++i)
		{
			const FBNRacer& Rival = Race.GetRival(i);
			const ABNVehiclePawn* Car = Rival.Car.Get();
			const ABNRivalController* Driver = Car ? Cast<ABNRivalController>(Car->GetController()) : nullptr;
			const TArray<double>& Laps = Rival.Progress.LapTimes();
			UE_LOG(LogTemp, Display, TEXT("BN rival %d (skill %.2f): %.2fs, laps %s, respawns %d"), i, Race.Definition->RivalSkills[i],
				Rival.Progress.FinishTime(), *FString::JoinBy(Laps, TEXT(" / "), [](double T) { return FString::Printf(TEXT("%.2f"), T); }),
				Driver ? Driver->GetRespawnCount() : -1);
			Test.TestNotNull(TEXT("rival still has its driver"), Driver);
			Test.TestEqual(TEXT("rival drove both laps"), Laps.Num(), 2);
			Test.TestEqual(TEXT("rival never crashed out"), Driver ? Driver->GetRespawnCount() : -1, 0);
			for (const double Lap : Laps)
			{
				Test.TestTrue(TEXT("rival lap in a sane time"), Lap > 10.0 && Lap < MaxLapSeconds);
			}
		}
		Test.TestEqual(TEXT("parked player is last"), Race.GetPlayerPosition(), Race.GetNumRivals() + 1);
		return true;
	});
	RunRaceScript(*this, EBNRaceDiscipline::Circuit, MoveTemp(Steps), RaceTimeoutSeconds);
	return true;
}

#endif
