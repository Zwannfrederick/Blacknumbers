#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABNHUD;
class ABNRaceEvent;

enum class EBNResultsOverlayMode : uint8
{
	RaceSummary,
	PursuitSummary,
};

/** Captured race outcome snapshot shown after an interactive race finishes. */
struct FBNRaceResultSummary
{
	TWeakObjectPtr<ABNRaceEvent> RaceEvent;
	FString RaceName;
	FString DisciplineLabel;
	bool bIsDrift = false;
	bool bIsDrag = false;
	bool bCompleted = false;
	bool bTotaled = false;
	bool bBlownEngine = false;
	int32 PerfectShifts = 0;
	int32 Position = 1;
	int32 TotalRacers = 1;
	float FinishTimeSeconds = 0.f;
	int32 DriftScore = 0;
	int32 BossDriftTarget = 0;
	int32 CashEarned = 0;
	int32 RepEarned = 0;
	int32 BossRank = 0;
	bool bBossVictory = false;
	FString BossName;
	FString BossVehicle;
};

/** Captured pursuit outcome snapshot shown when an interactive pursuit ends in Busted or Escaped. */
struct FBNPursuitResultSummary
{
	bool bEscaped = false;
	int32 HeatLevel = 1;
	FString HeatLabel;
	int32 UnitsDisabled = 0;
	int32 BreakersTriggered = 0;
	int32 PursuitCostDollars = 0;
	int32 TotalCityDamageDollars = 0;
	int32 Bounty = 0;
	int32 RepEarned = 0;
	float DurationSeconds = 0.f;
};

/**
 * Polished Slate post-race and pursuit-end summary modal overlay.
 */
class BLACKNUMBERS_API SBNResultsOverlayWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNResultsOverlayWidget)
		: _Mode(EBNResultsOverlayMode::RaceSummary)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
		SLATE_ARGUMENT(EBNResultsOverlayMode, Mode)
		SLATE_ARGUMENT(FBNRaceResultSummary, RaceSummary)
		SLATE_ARGUMENT(FBNPursuitResultSummary, PursuitSummary)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildRaceSummary();
	TSharedRef<SWidget> BuildPursuitSummary();
	TSharedRef<SWidget> MakeMetricTile(const TCHAR* Label, const FString& Value, const FLinearColor& ValueColor, const FString& SubNote = FString());

	TWeakObjectPtr<ABNHUD> OwnerHUD;
	EBNResultsOverlayMode Mode = EBNResultsOverlayMode::RaceSummary;
	FBNRaceResultSummary RaceSummary;
	FBNPursuitResultSummary PursuitSummary;
};
