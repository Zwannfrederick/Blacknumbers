#include "Misc/AutomationTest.h"
#include "Race/BNDriftScorer.h"
#include "Race/BNRaceProgress.h"
#include "Race/BNRivalController.h"
#include "Components/SplineComponent.h"
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
	TestEqual(TEXT("every counted checkpoint is tallied for ranking"), Circuit.Passed(), 6);
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
	TestEqual(TEXT("ordinal 1"), ABNHUD::Ordinal(1), FString(TEXT("1ST")));
	TestEqual(TEXT("ordinal 2"), ABNHUD::Ordinal(2), FString(TEXT("2ND")));
	TestEqual(TEXT("ordinal 3"), ABNHUD::Ordinal(3), FString(TEXT("3RD")));
	TestEqual(TEXT("ordinal 4"), ABNHUD::Ordinal(4), FString(TEXT("4TH")));
	TestEqual(TEXT("ordinal 12"), ABNHUD::Ordinal(12), FString(TEXT("12TH")));
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

// The rival's speed plan is plain physics: v = sqrt(grip * g * R) in a corner, braking distance before it,
// stopping distance before the end of an open line, no limit on a long straight.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNRivalSafeSpeedTest, "BlackNumbers.Race.RivalSafeSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNRivalSafeSpeedTest::RunTest(const FString& Parameters)
{
	constexpr float Grip = 1.f;
	constexpr float Brake = 0.8f;
	constexpr float G = 980.f;
	USplineComponent* Line = NewObject<USplineComponent>(GetTransientPackage());

	Line->SetSplinePoints({ FVector(0.f), FVector(100000.f, 0.f, 0.f) }, ESplineCoordinateSpace::World);
	Line->SetClosedLoop(false);
	TestTrue(TEXT("long straight: no limit early on"), ABNRivalController::SafeSpeed(*Line, 0.f, Grip, Brake) > 6000.f);
	const float StopFrom = 3000.f;
	TestTrue(TEXT("open line: can stop by its end"),
		ABNRivalController::SafeSpeed(*Line, 100000.f - StopFrom, Grip, Brake) <= FMath::Sqrt(2.f * Brake * G * StopFrom) + 1.f);

	constexpr float Radius = 4500.f;
	TArray<FVector> Circle;
	for (int32 i = 0; i < 32; ++i)
	{
		const float A = UE_TWO_PI * i / 32.f;
		Circle.Add(FVector(Radius * FMath::Cos(A), Radius * FMath::Sin(A), 0.f));
	}
	Line->SetSplinePoints(Circle, ESplineCoordinateSpace::World);
	Line->SetClosedLoop(true);
	const float Expected = FMath::Sqrt(Grip * G * Radius);
	const float OnCircle = ABNRivalController::SafeSpeed(*Line, 1000.f, Grip, Brake);
	UE_LOG(LogTemp, Display, TEXT("BN rival safe speed on r=%.0f: %.0f cm/s (expected %.0f)"), Radius, OnCircle, Expected);
	TestTrue(TEXT("constant corner: grip-limited speed"), FMath::Abs(OnCircle - Expected) < Expected * 0.1f);
	TestTrue(TEXT("more grip, more corner speed"), ABNRivalController::SafeSpeed(*Line, 1000.f, Grip * 1.2f, Brake) > OnCircle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNRivalCatchUpTest, "BlackNumbers.Race.RivalCatchUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNRivalCatchUpTest::RunTest(const FString& Parameters)
{
	// Gap > 0: rival ahead of the player along the racing line (cm).
	TestEqual(TEXT("close racing is untouched"), ABNRivalController::CatchUpScale(2000.f, 0.5f), 1.f);
	TestEqual(TEXT("close racing behind is untouched"), ABNRivalController::CatchUpScale(-2000.f, 0.5f), 1.f);
	const float FarBehind = ABNRivalController::CatchUpScale(-40000.f, 0.5f);
	const float FarAhead = ABNRivalController::CatchUpScale(60000.f, 0.5f);
	TestTrue(TEXT("a rival far behind speeds up"), FarBehind > 1.05f);
	TestTrue(TEXT("a rival far ahead eases off"), FarAhead < 0.95f);
	TestTrue(TEXT("catch-up is bounded"), FarBehind <= 1.17f && ABNRivalController::CatchUpScale(-1.0e7f, 1.f) <= 1.17f);
	TestTrue(TEXT("ease-off is bounded"), ABNRivalController::CatchUpScale(1.0e7f, 0.f) >= 0.85f);
	TestTrue(TEXT("skilled rivals ease off less"), ABNRivalController::CatchUpScale(60000.f, 1.f) > ABNRivalController::CatchUpScale(60000.f, 0.f));
	TestTrue(TEXT("more behind -> more catch-up"), ABNRivalController::CatchUpScale(-20000.f, 0.5f) < FarBehind);
	return true;
}

#endif
