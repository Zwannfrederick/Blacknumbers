#include "Misc/AutomationTest.h"
#include "Race/BNDriftScorer.h"
#include "Race/BNRaceProgress.h"
#include "UI/BNHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNRaceProgressTest, "BlackNumbers.Race.Progress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNRaceProgressTest::RunTest(const FString& Parameters)
{
	// Circuit: checkpoint 0 is the start/finish line, crossed at the start; 3 checkpoints, 2 laps.
	FBNRaceProgress Circuit;
	Circuit.Reset(3, 2, true);
	TestEqual(TEXT("circuit starts heading for checkpoint 1"), Circuit.NextCheckpoint(), 1);
	TestEqual(TEXT("wrong checkpoint is ignored"), Circuit.Pass(2, 1.0), EBNRaceStep::None);
	TestEqual(TEXT("next checkpoint counts"), Circuit.Pass(1, 2.0), EBNRaceStep::Checkpoint);
	TestEqual(TEXT("second checkpoint counts"), Circuit.Pass(2, 4.0), EBNRaceStep::Checkpoint);
	TestEqual(TEXT("crossing the line ends lap 1"), Circuit.Pass(0, 10.0), EBNRaceStep::Lap);
	TestEqual(TEXT("now on lap 2"), Circuit.CurrentLap(), 2);
	Circuit.Pass(1, 12.0);
	Circuit.Pass(2, 15.0);
	TestEqual(TEXT("last crossing finishes"), Circuit.Pass(0, 22.5), EBNRaceStep::Finished);
	TestTrue(TEXT("finished"), Circuit.IsFinished());
	TestEqual(TEXT("two lap times"), Circuit.LapTimes().Num(), 2);
	TestEqual(TEXT("lap 2 time"), Circuit.LapTimes()[1], 12.5);
	TestEqual(TEXT("finish time"), Circuit.FinishTime(), 22.5);
	TestEqual(TEXT("nothing counts after the finish"), Circuit.Pass(1, 30.0), EBNRaceStep::None);
	TestEqual(TEXT("lap shown does not run past the last"), Circuit.CurrentLap(), 2);

	// Sprint: checkpoints in order, the last one is the finish.
	FBNRaceProgress Sprint;
	Sprint.Reset(3, 1, false);
	TestEqual(TEXT("sprint starts heading for checkpoint 0"), Sprint.NextCheckpoint(), 0);
	TestEqual(TEXT("sprint checkpoint"), Sprint.Pass(0, 3.0), EBNRaceStep::Checkpoint);
	TestEqual(TEXT("sprint checkpoint 2"), Sprint.Pass(1, 6.0), EBNRaceStep::Checkpoint);
	TestEqual(TEXT("sprint finish"), Sprint.Pass(2, 9.0), EBNRaceStep::Finished);
	TestEqual(TEXT("sprint time"), Sprint.FinishTime(), 9.0);

	TestEqual(TEXT("race clock format"), ABNHUD::FormatRaceTime(83.456f), FString(TEXT("1:23.46")));
	TestEqual(TEXT("race clock under a minute"), ABNHUD::FormatRaceTime(9.5f), FString(TEXT("0:09.50")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNDriftScorerTest, "BlackNumbers.Race.DriftScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNDriftScorerTest::RunTest(const FString& Parameters)
{
	constexpr float Dt = 0.1f;
	auto Drive = [](FBNDriftScorer& Scorer, float Seconds, float Slip, float Speed)
	{
		for (float T = 0.f; T < Seconds - KINDA_SMALL_NUMBER; T += Dt)
		{
			Scorer.Update(Dt, Slip, Speed);
		}
	};

	FBNDriftScorer Scorer;
	Drive(Scorer, 2.f, 5.f, 80.f);
	TestEqual(TEXT("gripping scores nothing"), Scorer.Current(), 0.f);
	Drive(Scorer, 2.f, 30.f, 10.f);
	TestEqual(TEXT("slow sideways shuffling scores nothing"), Scorer.Current(), 0.f);

	Drive(Scorer, 1.f, 30.f, 60.f);
	const float OneSecond = Scorer.Current();
	TestTrue(TEXT("a drift scores"), OneSecond > 0.f);
	TestEqual(TEXT("nothing banked mid-drift"), Scorer.Total(), 0);

	Drive(Scorer, 4.f, 30.f, 60.f);
	TestTrue(TEXT("the combo multiplier grows with a held drift"), Scorer.Multiplier() > 1);

	FBNDriftScorer Steep;
	Drive(Steep, 1.f, 45.f, 60.f);
	TestTrue(TEXT("a steeper angle scores more"), Steep.Current() > OneSecond);

	Drive(Scorer, 2.f, 0.f, 60.f);
	TestTrue(TEXT("ending the drift banks the points"), Scorer.Total() > 0);
	TestEqual(TEXT("and clears the running drift"), Scorer.Current(), 0.f);

	const int32 Banked = Scorer.Total();
	Drive(Scorer, 2.f, 30.f, 60.f);
	Scorer.OnImpact();
	TestEqual(TEXT("hitting a wall loses the unbanked drift"), Scorer.Current(), 0.f);
	Drive(Scorer, 2.f, 0.f, 60.f);
	TestEqual(TEXT("banked points survive the impact"), Scorer.Total(), Banked);

	FBNDriftScorer Timeout;
	Drive(Timeout, 3.f, 30.f, 60.f);
	Timeout.Finish();
	TestTrue(TEXT("the race ending banks a drift still running"), Timeout.Total() > 0);

	FBNDriftScorer Spin;
	Drive(Spin, 1.f, 30.f, 60.f);
	Drive(Spin, 0.5f, 120.f, 40.f);
	TestEqual(TEXT("spinning out loses the drift"), Spin.Current(), 0.f);
	Drive(Spin, 2.f, 0.f, 40.f);
	TestEqual(TEXT("a spun drift banks nothing"), Spin.Total(), 0);
	return true;
}

#endif
