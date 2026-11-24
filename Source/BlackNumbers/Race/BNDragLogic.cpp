#include "Race/BNDragLogic.h"

namespace
{
	// Redline speeds (km/h) for Drag gears 1..6: crisp spacing so every gear shift feels urgent and rewarding.
	constexpr float GearCeilingKmh[FBNDragState::MaxGear] = { 74.f, 124.f, 174.f, 224.f, 274.f, 345.f };
	constexpr float GearFloorKmh[FBNDragState::MaxGear]   = {  0.f,  46.f,  88.f, 134.f, 182.f, 232.f };
}

void FBNDragState::Reset(bool bEnable)
{
	bActive = bEnable;
	Gear = MinGear;
	RpmFraction = 0.22f;
	OverheatSeconds = 0.f;
	bEngineBlown = false;
	LastShiftGrade = EBNDragShiftGrade::None;
	ShiftFeedbackRemaining = 0.f;
	PerfectShifts = 0;
	TotalShifts = 0;
}

float FBNDragState::GetCurrentGearMaxSpeedKmh() const
{
	const int32 Idx = FMath::Clamp(Gear - 1, 0, MaxGear - 1);
	return GearCeilingKmh[Idx];
}

void FBNDragState::Tick(float DeltaSeconds, float SpeedKmh, float ThrottleInput)
{
	if (!bActive || bEngineBlown)
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.2f);
	ShiftFeedbackRemaining = FMath::Max(0.f, ShiftFeedbackRemaining - Dt);

	const int32 Idx = FMath::Clamp(Gear - 1, 0, MaxGear - 1);
	const float FloorKmh = GearFloorKmh[Idx];
	const float CeilKmh = GearCeilingKmh[Idx];
	const float AbsSpeed = FMath::Abs(SpeedKmh);

	// Map speed within current gear window onto [0.18..1.04], adding responsive throttle rev bump at low speed.
	const float SpeedFrac = FMath::Clamp((AbsSpeed - FloorKmh) / FMath::Max(CeilKmh - FloorKmh, 20.f), 0.f, 1.08f);
	const float TargetRpm = FMath::Clamp(0.18f + SpeedFrac * 0.86f + (AbsSpeed < 20.f ? ThrottleInput * 0.28f : 0.f), 0.16f, 1.08f);
	RpmFraction = FMath::FInterpTo(RpmFraction, TargetRpm, Dt, TargetRpm > RpmFraction ? 12.f : 7.f);

	// Holding at/above redline under gas builds cylinder heat rapidly — ignore in top gear unless slamming limiter >335 km/h.
	const bool bRedlining = (RpmFraction >= 0.96f) && (ThrottleInput > 0.25f) && (Gear < MaxGear || AbsSpeed >= 332.f);
	if (bRedlining)
	{
		OverheatSeconds += Dt;
		if (OverheatSeconds >= OverheatLimitSeconds)
		{
			OverheatSeconds = OverheatLimitSeconds;
			bEngineBlown = true;
			LastShiftGrade = EBNDragShiftGrade::Overrev;
			ShiftFeedbackRemaining = 2.5f;
		}
	}
	else
	{
		OverheatSeconds = FMath::Max(0.f, OverheatSeconds - Dt * 0.95f);
	}
}

float FBNDragState::ShiftUp(EBNDragShiftGrade* OutGrade)
{
	if (!bActive || bEngineBlown || Gear >= MaxGear)
	{
		if (OutGrade)
		{
			*OutGrade = EBNDragShiftGrade::None;
		}
		return 0.f;
	}

	EBNDragShiftGrade Grade = EBNDragShiftGrade::Early;
	float ImpulseCmPerSec = 0.f;

	if (RpmFraction >= 0.80f && RpmFraction <= 0.94f)
	{
		Grade = EBNDragShiftGrade::Perfect;
		ImpulseCmPerSec = 310.f;
		++PerfectShifts;
	}
	else if ((RpmFraction >= 0.68f && RpmFraction < 0.80f) || (RpmFraction > 0.94f && RpmFraction <= 0.99f))
	{
		Grade = EBNDragShiftGrade::Good;
		ImpulseCmPerSec = 135.f;
	}
	else if (RpmFraction > 0.99f)
	{
		Grade = EBNDragShiftGrade::Overrev;
		ImpulseCmPerSec = 30.f;
	}
	else
	{
		Grade = EBNDragShiftGrade::Early;
		ImpulseCmPerSec = -80.f;
	}

	++Gear;
	++TotalShifts;
	RpmFraction = FMath::Clamp(RpmFraction - 0.34f, 0.24f, 0.72f);
	OverheatSeconds *= 0.25f;
	LastShiftGrade = Grade;
	ShiftFeedbackRemaining = 1.45f;

	if (OutGrade)
	{
		*OutGrade = Grade;
	}
	return ImpulseCmPerSec;
}

bool FBNDragState::ShiftDown()
{
	if (!bActive || bEngineBlown || Gear <= MinGear)
	{
		return false;
	}
	--Gear;
	RpmFraction = FMath::Clamp(RpmFraction + 0.26f, 0.35f, 0.96f);
	return true;
}

const TCHAR* FBNDragState::GradeLabel(EBNDragShiftGrade Grade)
{
	switch (Grade)
	{
	case EBNDragShiftGrade::Perfect: return TEXT("MÜKEMMEL VİTES!");
	case EBNDragShiftGrade::Good:    return TEXT("İYİ VİTES");
	case EBNDragShiftGrade::Early:   return TEXT("ERKEN VİTES");
	case EBNDragShiftGrade::Overrev: return TEXT("AŞIRI DEVİR!");
	default:                         return TEXT("");
	}
}
