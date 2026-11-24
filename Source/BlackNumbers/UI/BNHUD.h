#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/BNMenuWidget.h"
#include "UI/BNResultsOverlayWidget.h"
#include "UI/BNTitleScreenWidget.h"
#include "BNHUD.generated.h"

class ABNRaceEvent;
class ABNVehiclePawn;
class SWidget;
class UBNProgressionSubsystem;
class UBNPursuitSubsystem;
class UBNStorySubsystem;

/** What the dashboard shows, read straight from the car's physics state. */
struct FBNDashReadout
{
	int32 SpeedKmh = 0;
	FString Gear;
	int32 Rpm = 0;
	float RpmFraction = 0.f;
};

/**
 * Most Wanted-style racing HUD & native Slate screen host:
 * Title Screen, Pause/Garage/Dossier/Settings menu, Post-Race & Pursuit Summary overlays,
 * Storyboard cards, radial tachometer, minimap radar with traffic, and 3D world markers.
 */
UCLASS()
class BLACKNUMBERS_API ABNHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenMenu(EBNMenuTab Tab);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleMenu(EBNMenuTab Tab);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenTitleScreen();

	void DismissTitleScreen(bool bStartDriving);
	void ShowRaceResults(const FBNRaceResultSummary& Summary);
	void ShowPursuitResults(const FBNPursuitResultSummary& Summary);
	void DismissResultsOverlay(bool bRestoreControl = true);
	void RestartRaceFromResults(const FBNRaceResultSummary& Summary);

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsMenuOpen() const { return MenuWidget.IsValid() || TitleScreenWidget.IsValid() || ResultsOverlayWidget.IsValid(); }

	bool IsPauseMenuOpen() const { return MenuWidget.IsValid(); }
	bool IsTitleScreenOpen() const { return TitleScreenWidget.IsValid(); }
	bool IsResultsOverlayOpen() const { return ResultsOverlayWidget.IsValid(); }
	bool HasStoryCardOpen() const { return StoryCardWidget.IsValid(); }
	bool HandleMenuCloseOrAdvanceCard();

	static float GetPreferredEngineGain();
	static void SetPreferredEngineGain(float NewGain);

	static FBNDashReadout MakeReadout(const ABNVehiclePawn& Car);
	static FString GearLabel(int32 Gear);
	/** 1ST, 2ND, 3RD, 4TH, 11TH... */
	static FString Ordinal(int32 Place);
	/** m:ss.cc */
	static FString FormatRaceTime(float Seconds);

private:
	struct FTrackedRaceState
	{
		TWeakObjectPtr<ABNRaceEvent> Event;
		FString RaceName;
		FString DisciplineLabel;
		int32 BossRank = 0;
		int32 BossDriftTarget = 0;
		int32 PreRaceCash = 0;
		int32 PreRaceRep = 0;
		int32 TotalRacers = 1;
		float LastTimeLeft = 0.f;
		bool bIsDrift = false;
		bool bWasRacing = false;
		bool bResultsShown = false;
	};

	void DrawRace(const ABNRaceEvent& Race);
	void DrawPursuit(const UBNPursuitSubsystem& Pursuit, const UBNStorySubsystem* Story, bool bRaceActive);
	void DrawEconomyPill(const UBNProgressionSubsystem& Progression);
	void DrawRadar(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, const UBNPursuitSubsystem* Pursuit, const UBNStorySubsystem* Story);
	void DrawWorldMarkers(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, const UBNStorySubsystem* Story);
	void DrawDashboard(const ABNVehiclePawn& Car, const FBNDashReadout& Dash, bool bMenuBlocking);
	void DrawGaugeBar(const TCHAR* Label, float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColour, bool bActive);
	void DrawActionBanners(const ABNVehiclePawn& Car, float BaseY);
	void DrawCameraModePill(const ABNVehiclePawn& Car);
	void DrawCentred(const FString& Text, float Y, float Scale, const FLinearColor& Colour);
	void SyncStoryCardOverlay();
	void SyncShowcaseCamera(ABNVehiclePawn* Car) const;
	void UpdateRaceAndPursuitTracking(const UBNProgressionSubsystem* Prog, const UBNStorySubsystem* Story, const UBNPursuitSubsystem* Pursuit);
	void RemoveAllSlateOverlays();
	bool CanUseViewportSlate() const;
	void ApplyCursorMode(bool bUiActive) const;

	TSharedPtr<SBNTitleScreenWidget> TitleScreenWidget;
	TSharedPtr<SWidget> TitleScreenHost;
	TSharedPtr<SBNResultsOverlayWidget> ResultsOverlayWidget;
	TSharedPtr<SWidget> ResultsHost;
	TSharedPtr<SBNGameMenuWidget> MenuWidget;
	TSharedPtr<SWidget> MenuHost;
	TSharedPtr<SBNStoryCardWidget> StoryCardWidget;
	TSharedPtr<SWidget> StoryCardHost;
	FString LastShownCardKey;

	FTrackedRaceState TrackedRace;
	bool bTrackedPursuitActive = false;
	int32 TrackedPursuitPreRep = 0;
	bool bAppliedAudioGainToCar = false;
	int32 LastDistrictId = -1;
	FString DistrictBannerName;
	FString DistrictBannerSubtitle;
	double DistrictBannerUntil = 0.0;
};
