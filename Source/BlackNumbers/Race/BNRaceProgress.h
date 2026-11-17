#pragma once

#include "CoreMinimal.h"

enum class EBNRaceStep : uint8
{
	None,       // not the checkpoint we were waiting for
	Checkpoint,
	Lap,
	Finished,
};

/**
 * Checkpoint order, laps and times for one racer. No actors, so it is unit tested directly.
 * Loop (circuit): checkpoint 0 is the start/finish line, crossed at the start; a lap ends back at 0.
 * Point-to-point (sprint): checkpoints are passed 0..N-1 in order and the last one is the finish.
 */
struct BLACKNUMBERS_API FBNRaceProgress
{
	void Reset(int32 InNumCheckpoints, int32 InLaps, bool bInLoop);

	/** Call when the racer crosses checkpoint Index at race time Time (seconds since the start). */
	EBNRaceStep Pass(int32 Index, double Time);

	int32 NextCheckpoint() const { return Next; }
	int32 CurrentLap() const { return FMath::Min(Lap, Laps); }
	int32 TotalLaps() const { return Laps; }
	int32 NumCheckpoints() const { return Checkpoints; }
	/** Checkpoints passed so far over all laps (the circuit start line included); ranks racers. */
	int32 Passed() const { return PassedCount; }
	bool IsFinished() const { return bFinished; }
	double FinishTime() const { return FinishAt; }
	const TArray<double>& LapTimes() const { return LapTimesSeconds; }

private:
	int32 Checkpoints = 0;
	int32 Laps = 1;
	bool bLoop = false;
	int32 Next = 0;
	int32 Lap = 1;
	int32 PassedCount = 0;
	bool bFinished = false;
	double LapStart = 0.0;
	double FinishAt = 0.0;
	TArray<double> LapTimesSeconds;
};
