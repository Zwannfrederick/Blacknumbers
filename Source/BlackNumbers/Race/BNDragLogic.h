#pragma once

#include "CoreMinimal.h"

/** Shift timing grade awarded during manual-shift Drag races. */
enum class EBNDragShiftGrade : uint8
{
	None = 0,
	Early,
	Good,
	Perfect,
	Overrev,
};

/**
 * Pure, deterministic NFS Underground/Most Wanted manual-shift Drag logic:
 * - Tracks manual gears 1..6 with distinct redline ceilings so the player must shift to accelerate.
 * - Evaluates shift timing (Early / Good / Perfect / Overrev) and returns forward impulse surge.
 * - Accumulates engine overheat when redlining under throttle; holding redline too long blows the engine!
 */
struct BLACKNUMBERS_API FBNDragState
{
	static constexpr int32 MinGear = 1;
	static constexpr int32 MaxGear = 6;
	static constexpr float OverheatLimitSeconds = 2.20f;

	bool bActive = false;
	int32 Gear = 1;
	float RpmFraction = 0.22f;
	float OverheatSeconds = 0.f;
	bool bEngineBlown = false;
	EBNDragShiftGrade LastShiftGrade = EBNDragShiftGrade::None;
	float ShiftFeedbackRemaining = 0.f;
	int32 PerfectShifts = 0;
	int32 TotalShifts = 0;

	void Reset(bool bEnable);
	void Tick(float DeltaSeconds, float SpeedKmh, float ThrottleInput);

	/** Shifts up one gear, returning forward velocity surge (cm/s) for Perfect/Good shifts. */
	float ShiftUp(EBNDragShiftGrade* OutGrade = nullptr);

	/** Shifts down one gear if above Gear 1. */
	bool ShiftDown();

	/** Current gear redline speed ceiling in km/h (player must upshift to go faster). */
	float GetCurrentGearMaxSpeedKmh() const;

	float GetOverheatFraction() const
	{
		return FMath::Clamp(OverheatSeconds / OverheatLimitSeconds, 0.f, 1.f);
	}

	static const TCHAR* GradeLabel(EBNDragShiftGrade Grade);
};
