#include "UI/BNHUD.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float Margin = 40.f;
	constexpr float BarWidth = 260.f;
	constexpr float BarHeight = 10.f;
	constexpr float SpeedScale = 2.5f;
	constexpr float LabelScale = 1.2f;
	/** Rev bar turns red above this fraction of max rpm. */
	constexpr float RedlineFraction = 0.85f;
	const FLinearColor DashColour(1.f, 0.82f, 0.45f);
	const FLinearColor RedlineColour(1.f, 0.2f, 0.1f);
	const FLinearColor BarBackground(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor RaceColour(1.f, 1.f, 1.f);
	const FLinearColor DriftColour(0.45f, 0.85f, 1.f);
	constexpr float RaceTop = 40.f;
	constexpr float RaceLine = 42.f;
	constexpr float CountdownScale = 5.f;
	constexpr float RaceScale = 1.6f;
}

FString ABNHUD::FormatRaceTime(float Seconds)
{
	const float Clamped = FMath::Max(Seconds, 0.f);
	const int32 Minutes = FMath::FloorToInt(Clamped / 60.f);
	const float Rest = Clamped - Minutes * 60.f;
	return FString::Printf(TEXT("%d:%05.2f"), Minutes, Rest);
}

void ABNHUD::DrawCentred(const FString& Text, float Y, float Scale, const FLinearColor& Colour)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetLargeFont(), Scale);
	DrawText(Text, Colour, (Canvas->ClipX - W) * 0.5f, Y, GEngine->GetLargeFont(), Scale);
}

void ABNHUD::DrawRace(const ABNRaceEvent& Race)
{
	const UBNRaceDefinition* Def = Race.Definition;
	if (!Def)
	{
		return;
	}
	const FBNRaceProgress& Progress = Race.GetProgress();
	const FBNDriftScorer& Drift = Race.GetDriftScore();
	const bool bDrift = Def->Discipline == EBNRaceDiscipline::Drift;
	float Y = RaceTop;
	DrawCentred(Def->DisplayName.ToString().ToUpper(), Y, RaceScale, RaceColour);
	Y += RaceLine;

	switch (Race.GetState())
	{
	case EBNRaceState::Countdown:
		DrawCentred(FString::FromInt(FMath::CeilToInt(Race.GetRaceTime())), Y + RaceLine, CountdownScale, DashColour);
		break;
	case EBNRaceState::Racing:
		if (bDrift)
		{
			DrawCentred(FString::Printf(TEXT("%d  pts    %s left"), Drift.Total(), *FormatRaceTime(Race.GetTimeLeft())), Y, RaceScale, RaceColour);
			if (Drift.IsDrifting())
			{
				DrawCentred(FString::Printf(TEXT("+%d  x%d"), FMath::RoundToInt(Drift.Current()), Drift.Multiplier()), Y + RaceLine, RaceScale * 1.4f, DriftColour);
			}
		}
		else
		{
			const FString Laps = Def->Discipline == EBNRaceDiscipline::Circuit
				? FString::Printf(TEXT("LAP %d/%d    "), Progress.CurrentLap(), Progress.TotalLaps()) : FString();
			DrawCentred(FString::Printf(TEXT("%s%s"), *Laps, *FormatRaceTime(Race.GetRaceTime())), Y, RaceScale, RaceColour);
		}
		break;
	case EBNRaceState::Finished:
		DrawCentred(bDrift ? FString::Printf(TEXT("SCORE  %d"), Drift.Total())
			: Progress.IsFinished() ? FString::Printf(TEXT("FINISHED  %s"), *FormatRaceTime(Progress.FinishTime()))
			: FString(TEXT("RACE ABANDONED")), Y, RaceScale * 1.4f, DashColour);
		break;
	default:
		break;
	}
}

FString ABNHUD::GearLabel(int32 Gear)
{
	return Gear < 0 ? TEXT("R") : Gear == 0 ? TEXT("N") : FString::FromInt(Gear);
}

FBNDashReadout ABNHUD::MakeReadout(const ABNVehiclePawn& Car)
{
	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	FBNDashReadout Out;
	Out.SpeedKmh = FMath::RoundToInt(FMath::Abs(Car.GetSpeedKmh()));
	Out.Gear = GearLabel(Move->GetCurrentGear());
	Out.Rpm = FMath::RoundToInt(Move->GetEngineRotationSpeed());
	const float MaxRpm = Move->GetEngineMaxRotationSpeed();
	Out.RpmFraction = MaxRpm > 0.f ? FMath::Clamp(Out.Rpm / MaxRpm, 0.f, 1.f) : 0.f;
	return Out;
}

void ABNHUD::DrawHUD()
{
	Super::DrawHUD();

	const ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (!Car || !Canvas)
	{
		return;
	}
	for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
	{
		if (It->GetState() != EBNRaceState::Idle)
		{
			DrawRace(**It);
			break;
		}
	}
	const FBNDashReadout Dash = MakeReadout(*Car);
	UFont* Font = GEngine->GetLargeFont();

	const float Right = Canvas->ClipX - Margin;
	const float Bottom = Canvas->ClipY - Margin;

	const FString Speed = FString::Printf(TEXT("%d"), Dash.SpeedKmh);
	float SpeedW = 0.f, SpeedH = 0.f;
	GetTextSize(Speed, SpeedW, SpeedH, Font, SpeedScale);
	DrawText(Speed, DashColour, Right - BarWidth, Bottom - BarHeight - SpeedH - 8.f, Font, SpeedScale);
	DrawText(TEXT("km/h"), DashColour, Right - BarWidth + SpeedW + 8.f, Bottom - BarHeight - 8.f - SpeedH * 0.45f, Font, LabelScale);

	float GearW = 0.f, GearH = 0.f;
	GetTextSize(Dash.Gear, GearW, GearH, Font, SpeedScale);
	DrawText(Dash.Gear, DashColour, Right - GearW, Bottom - BarHeight - GearH - 8.f, Font, SpeedScale);

	DrawRect(BarBackground, Right - BarWidth, Bottom - BarHeight, BarWidth, BarHeight);
	DrawRect(Dash.RpmFraction > RedlineFraction ? RedlineColour : DashColour,
		Right - BarWidth, Bottom - BarHeight, BarWidth * Dash.RpmFraction, BarHeight);
}
