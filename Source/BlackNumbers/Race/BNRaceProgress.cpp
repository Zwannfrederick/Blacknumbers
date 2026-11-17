#include "Race/BNRaceProgress.h"

void FBNRaceProgress::Reset(int32 InNumCheckpoints, int32 InLaps, bool bInLoop)
{
	Checkpoints = FMath::Max(InNumCheckpoints, 1);
	Laps = bInLoop ? FMath::Max(InLaps, 1) : 1;
	bLoop = bInLoop;
	Next = (bLoop && Checkpoints > 1) ? 1 : 0;
	Lap = 1;
	bFinished = false;
	LapStart = 0.0;
	FinishAt = 0.0;
	LapTimesSeconds.Reset();
}

EBNRaceStep FBNRaceProgress::Pass(int32 Index, double Time)
{
	if (bFinished || Index != Next)
	{
		return EBNRaceStep::None;
	}

	const bool bLineCrossed = bLoop ? Index == 0 : Index == Checkpoints - 1;
	Next = (Next + 1) % Checkpoints;
	if (!bLineCrossed)
	{
		return EBNRaceStep::Checkpoint;
	}

	LapTimesSeconds.Add(Time - LapStart);
	LapStart = Time;
	if (Lap >= Laps)
	{
		bFinished = true;
		FinishAt = Time;
		return EBNRaceStep::Finished;
	}
	++Lap;
	return EBNRaceStep::Lap;
}
