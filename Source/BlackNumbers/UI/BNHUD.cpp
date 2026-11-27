#include "UI/BNHUD.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNEngineAudioComponent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "Widgets/SWeakWidget.h"

namespace
{
	constexpr float Margin = 32.f;
	constexpr float SpeedScale = 0.95f;  // numeral font (120 px cache) -> ~114 px digits, never upscaled
	constexpr float GearScale = 0.95f;
	constexpr float LabelScale = 0.88f;
	constexpr float RedlineFraction = 0.82f;
	constexpr int32 RpmSegments = 28;
	constexpr float ClusterWidth = 318.f;
	constexpr float RpmWedgeMinH = 5.f;
	constexpr float RpmWedgeMaxH = 30.f;

	// "Sodyum" HUD palette (shared with BNUITheme; police red/blue only for pursuit).
	const FLinearColor DashColour = BNUITheme::Accent;
	const FLinearColor RedlineColour = BNUITheme::Danger;
	const FLinearColor SweetSpotColour = BNUITheme::Ok;
	const FLinearColor NosColour = BNUITheme::Nos;
	const FLinearColor NosActiveColour(0.35f, 0.90f, 1.f, 1.f);
	const FLinearColor SpeedbreakerColour = BNUITheme::Bone;
	const FLinearColor SpeedbreakerActiveColour = BNUITheme::AccentHi;
	const FLinearColor SmokedPanelBg(0.0030f, 0.0033f, 0.0040f, 0.70f);
	const FLinearColor BarBackground(0.02f, 0.022f, 0.026f, 0.80f);
	const FLinearColor UnlitSegment(0.0030f, 0.0033f, 0.0040f, 0.55f);
	const FLinearColor LegendColour(0.8550f, 0.8228f, 0.7454f, 0.80f);
	const FLinearColor RaceColour = BNUITheme::Bone;
	const FLinearColor DriftColour = BNUITheme::Nos;
	const FLinearColor PoliceRedColour(0.96f, 0.05f, 0.04f, 1.f);
	const FLinearColor PoliceBlueColour(0.04f, 0.30f, 1.f, 1.f);
	const FLinearColor RadioBgColour = SmokedPanelBg;

	constexpr float RaceTop = 24.f;
	constexpr float RaceLine = 38.f;
	constexpr float PursuitBarWidth = 660.f;
	constexpr float PursuitBarHeight = 22.f;

	/** Tracks which GameInstance already showed the launch Title Screen (survives map loads, resets per new session). */
	TWeakObjectPtr<const UGameInstance> GTitleShownForGI;
	float GPreferredEngineGain = 0.50f;
}

float ABNHUD::GetPreferredEngineGain()
{
	return GPreferredEngineGain;
}

void ABNHUD::SetPreferredEngineGain(float NewGain)
{
	GPreferredEngineGain = FMath::Clamp(NewGain, 0.f, 1.f);
}

bool ABNHUD::CanUseViewportSlate() const
{
	return !FApp::IsUnattended() && !GIsAutomationTesting && GEngine && GEngine->GameViewport != nullptr;
}

void ABNHUD::ApplyCursorMode(bool bUiActive) const
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	PC->bShowMouseCursor = bUiActive;
	if (bUiActive)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ABNHUD::SyncShowcaseCamera(ABNVehiclePawn* Car) const
{
	if (!Car)
	{
		return;
	}
	if (IsTitleScreenOpen())
	{
		Car->SetShowcaseMode(true, -95.f);
	}
	else if (MenuWidget.IsValid() && MenuWidget->GetActiveTab() == EBNMenuTab::Garage)
	{
		Car->SetShowcaseMode(true, 0.f);
	}
	else if (Car->IsShowcaseMode())
	{
		Car->SetShowcaseMode(false, 0.f);
	}
}

void ABNHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllSlateOverlays();
	Super::EndPlay(EndPlayReason);
}

void ABNHUD::RemoveAllSlateOverlays()
{
	if (GEngine && GEngine->GameViewport)
	{
		for (const TSharedPtr<SWidget>& Host : { TitleScreenHost, ResultsHost, MenuHost, StoryCardHost, RaceBriefingHost, WorldMapHost })
		{
			if (Host.IsValid())
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(Host.ToSharedRef());
			}
		}
	}
	TitleScreenHost.Reset();
	TitleScreenWidget.Reset();
	ResultsHost.Reset();
	ResultsOverlayWidget.Reset();
	MenuHost.Reset();
	MenuWidget.Reset();
	StoryCardHost.Reset();
	StoryCardWidget.Reset();
	RaceBriefingHost.Reset();
	RaceBriefingWidget.Reset();
	WorldMapHost.Reset();
	WorldMapWidget.Reset();
	LastShownCardKey.Reset();
	if (bPausedForRaceMenu)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
		bPausedForRaceMenu = false;
	}
	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetShowcaseMode(false, 0.f);
	}
	ApplyCursorMode(false);
}

void ABNHUD::OpenMenu(EBNMenuTab Tab)
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	const bool bRaceActive = ABNRaceEvent::FindActiveRace(GetWorld()) != nullptr;
	if (bRaceActive && Tab != EBNMenuTab::Settings && Tab != EBNMenuTab::Controls)
	{
		Tab = EBNMenuTab::Races;
	}
	if (TitleScreenHost.IsValid())
	{
		DismissTitleScreen(false);
	}
	if (ResultsHost.IsValid())
	{
		DismissResultsOverlay(false);
	}
	if (Tab == EBNMenuTab::BlackNumbers)
	{
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->EnsureIntroShown(Cast<ABNVehiclePawn>(GetOwningPawn()));
		}
	}
	if (MenuWidget.IsValid())
	{
		MenuWidget->SetActiveTab(Tab);
		SyncShowcaseCamera(Cast<ABNVehiclePawn>(GetOwningPawn()));
		SyncStoryCardOverlay();
		return;
	}

	SAssignNew(MenuWidget, SBNGameMenuWidget)
		.OwnerHUD(this)
		.InitialTab(Tab);
	MenuHost = SNew(SWeakWidget).PossiblyNullContent(MenuWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(MenuHost.ToSharedRef(), 20);

	if (bRaceActive)
	{
		// Freeze rivals, physics & race clock cleanly during the Race Pause Menu without altering countdown input lock.
		UGameplayStatics::SetGamePaused(GetWorld(), true);
		bPausedForRaceMenu = true;
	}
	else if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetInputLocked(true);
		SyncShowcaseCamera(Car);
	}
	ApplyCursorMode(true);
	SyncStoryCardOverlay();
}

void ABNHUD::CloseMenu()
{
	const bool bWasGarageTab = MenuWidget.IsValid() && MenuWidget->GetActiveTab() == EBNMenuTab::Garage;
	const bool bWasPausedForRace = bPausedForRaceMenu;
	if (bPausedForRaceMenu)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
		bPausedForRaceMenu = false;
	}
	if (GEngine && GEngine->GameViewport && MenuHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuHost.ToSharedRef());
	}
	MenuHost.Reset();
	MenuWidget.Reset();

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (Car)
	{
		Car->SetShowcaseMode(false, 0.f);
	}
	if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		if (Prog->IsGarageOpen())
		{
			Prog->CloseGarage(Car);
		}
		else if (bWasGarageTab && Car)
		{
			Prog->ApplyProfileToCar(*Car);
		}
	}
	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		if (Story->IsDossierOpen())
		{
			Story->CloseDossier(Car);
		}
	}

	SyncStoryCardOverlay();
	if (!HasStoryCardOpen() && !IsMenuOpen())
	{
		if (Car && !bWasPausedForRace && !ABNRaceEvent::FindActiveRace(GetWorld()))
		{
			Car->SetInputLocked(false);
		}
		ApplyCursorMode(false);
	}
}

void ABNHUD::ToggleMenu(EBNMenuTab Tab)
{
	if (!MenuWidget.IsValid())
	{
		OpenMenu(Tab);
		return;
	}
	if (MenuWidget->GetActiveTab() == Tab)
	{
		CloseMenu();
	}
	else
	{
		MenuWidget->SetActiveTab(Tab);
		SyncShowcaseCamera(Cast<ABNVehiclePawn>(GetOwningPawn()));
	}
}

bool ABNHUD::HandleMenuCloseOrAdvanceCard()
{
	if (IsTitleScreenOpen())
	{
		OpenMenu(EBNMenuTab::BlackNumbers);
		return true;
	}
	if (IsRaceBriefingOpen())
	{
		CloseRaceBriefing(true);
		return true;
	}
	if (IsWorldMapOpen())
	{
		CloseWorldMap();
		return true;
	}
	if (IsResultsOverlayOpen())
	{
		DismissResultsOverlay(true);
		return true;
	}
	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		if (Story->HasActiveCard() && HasStoryCardOpen())
		{
			Story->DismissActiveCard(Cast<ABNVehiclePawn>(GetOwningPawn()));
			if (MenuWidget.IsValid())
			{
				if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
				{
					Car->SetInputLocked(true);
				}
			}
			SyncStoryCardOverlay();
			return true;
		}
	}
	if (MenuWidget.IsValid())
	{
		CloseMenu();
		return true;
	}
	OpenMenu(EBNMenuTab::Races);
	return true;
}

void ABNHUD::OpenRaceBriefing(ABNRaceEvent* Event)
{
	if (!CanUseViewportSlate() || !Event || !Event->Definition || IsMenuOpen())
	{
		return;
	}
	SAssignNew(RaceBriefingWidget, SBNRaceBriefingWidget)
		.OwnerHUD(this)
		.RaceEvent(Event);
	RaceBriefingHost = SNew(SWeakWidget).PossiblyNullContent(RaceBriefingWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(RaceBriefingHost.ToSharedRef(), 28);
	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetInputLocked(true);
	}
	ApplyCursorMode(true);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(RaceBriefingWidget);
	}
}

void ABNHUD::CloseRaceBriefing(bool bRestoreControl)
{
	if (GEngine && GEngine->GameViewport && RaceBriefingHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(RaceBriefingHost.ToSharedRef());
	}
	RaceBriefingHost.Reset();
	RaceBriefingWidget.Reset();
	if (bRestoreControl && !HasStoryCardOpen() && !IsMenuOpen())
	{
		if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
		{
			Car->SetInputLocked(false);
		}
		ApplyCursorMode(false);
	}
}

bool ABNHUD::TryOpenNearbyRaceBriefing()
{
	const ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (!Car || IsMenuOpen() || HasStoryCardOpen())
	{
		return false;
	}
	if (ABNRaceEvent* Nearby = ABNRaceEvent::FindNearbyIdleRace(GetWorld(), Car))
	{
		OpenRaceBriefing(Nearby);
		return true;
	}
	return false;
}

void ABNHUD::ToggleWorldMap()
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	if (WorldMapWidget.IsValid())
	{
		CloseWorldMap();
		return;
	}
	if (IsMenuOpen() || HasStoryCardOpen() || ABNRaceEvent::FindActiveRace(GetWorld()) != nullptr)
	{
		return;
	}
	SAssignNew(WorldMapWidget, SBNWorldMapWidget).OwnerHUD(this);
	WorldMapHost = SNew(SWeakWidget).PossiblyNullContent(WorldMapWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(WorldMapHost.ToSharedRef(), 26);
	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetInputLocked(true);
	}
	ApplyCursorMode(true);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(WorldMapWidget);
	}
}

void ABNHUD::CloseWorldMap()
{
	if (GEngine && GEngine->GameViewport && WorldMapHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(WorldMapHost.ToSharedRef());
	}
	WorldMapHost.Reset();
	WorldMapWidget.Reset();
	if (!HasStoryCardOpen() && !IsMenuOpen())
	{
		if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
		{
			Car->SetInputLocked(false);
		}
		ApplyCursorMode(false);
	}
}

void ABNHUD::ShowFreeRoamTotaledOverlay()
{
	FBNRaceResultSummary Sum;
	Sum.RaceName = TEXT("KARAMARA SERBEST SÜRÜŞ");
	Sum.DisciplineLabel = TEXT("AĞIR KAZA RAPORU");
	Sum.bTotaled = true;
	Sum.bCompleted = false;
	ShowRaceResults(Sum);
}

void ABNHUD::SyncStoryCardOverlay()
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	const bool bCareerOpen = MenuWidget.IsValid() && MenuWidget->GetActiveTab() == EBNMenuTab::BlackNumbers;
	const bool bSuppressed = IsTitleScreenOpen() || (IsPauseMenuOpen() && !bCareerOpen) || IsResultsOverlayOpen();
	const UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const FBNStoryCard* Card = (!bSuppressed && Story) ? Story->GetActiveCard() : nullptr;

	if (!Card)
	{
		if (StoryCardHost.IsValid())
		{
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(StoryCardHost.ToSharedRef());
			}
			StoryCardHost.Reset();
			StoryCardWidget.Reset();
			LastShownCardKey.Reset();
			if (!IsMenuOpen())
			{
				ApplyCursorMode(false);
			}
		}
		return;
	}

	if (!StoryCardWidget.IsValid())
	{
		SAssignNew(StoryCardWidget, SBNStoryCardWidget).OwnerHUD(this);
		StoryCardHost = SNew(SWeakWidget).PossiblyNullContent(StoryCardWidget.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(StoryCardHost.ToSharedRef(), 30);
	}
	const FString Key = FString::Printf(TEXT("%s_%d"), *Card->CardId, Story->GetQueuedCardCount());
	if (Key != LastShownCardKey)
	{
		LastShownCardKey = Key;
		StoryCardWidget->Refresh();
	}
	ApplyCursorMode(true);
}

FString ABNHUD::Ordinal(int32 Place)
{
	const int32 Teen = Place % 100;
	const int32 Last = Place % 10;
	const TCHAR* Suffix = (Teen >= 11 && Teen <= 13) ? TEXT("TH") : Last == 1 ? TEXT("ST") : Last == 2 ? TEXT("ND") : Last == 3 ? TEXT("RD") : TEXT("TH");
	return FString::Printf(TEXT("%d%s"), Place, Suffix);
}

FString ABNHUD::FormatRaceTime(float Seconds)
{
	const float Clamped = FMath::Max(Seconds, 0.f);
	const int32 Minutes = FMath::FloorToInt(Clamped / 60.f);
	const float Rest = Clamped - Minutes * 60.f;
	return FString::Printf(TEXT("%d:%05.2f"), Minutes, Rest);
}

void ABNHUD::DrawCentred(const FString& Text, float Y, float Scale, const FLinearColor& Colour, UFont* Font)
{
	UFont* Used = Font ? Font : BNUITheme::HudFont();
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Used, Scale);
	DrawTextShadowed(Text, Colour, (Canvas->ClipX - W) * 0.5f, Y, Used, Scale);
}

void ABNHUD::DrawTextShadowed(const FString& Text, const FLinearColor& Colour, float X, float Y, UFont* Font, float Scale)
{
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.6f * Colour.A), X + 1.5f, Y + 1.5f, Font, Scale);
	DrawText(Text, Colour, X, Y, Font, Scale);
}

void ABNHUD::DrawRace(const ABNRaceEvent& Race)
{
	const UBNRaceDefinition* Def = Race.Definition;
	if (!Def)
	{
		return;
	}
	UFont* Font = BNUITheme::HudFont();
	const FBNRaceProgress& Progress = Race.GetProgress();
	const FBNDriftScorer& Drift = Race.GetDriftScore();
	const bool bDrift = Def->Discipline == EBNRaceDiscipline::Drift;
	const bool bRivals = Race.GetNumRivals() > 0;
	const int32 BossRank = Race.GetActiveBossRank();
	const FString Header = BossRank > 0
		? FString::Printf(TEXT("KARA LİSTE #%02d  //  %s"), BossRank, *Def->DisplayName.ToString().ToUpper())
		: Def->DisplayName.ToString().ToUpper();

	// Authentic MW 2005 corner-anchored race telemetry over organic ink slash (NO clunky top-center SaaS box!)
	UFont* Digits = BNUITheme::HudFont(true);
	DrawTextShadowed(Header, BossRank > 0 ? DashColour : LegendColour, Margin, RaceTop, Font, 0.82f);

	switch (Race.GetState())
	{
	case EBNRaceState::Countdown:
		DrawTextShadowed(TEXT("HAZIRLAN"), RaceColour, Margin, RaceTop + 22.f, Digits, 0.65f);
		DrawCentred(FString::FromInt(FMath::CeilToInt(Race.GetRaceTime())), RaceTop + 96.f, 1.0f, DashColour, BNUITheme::HudNumeralFont());
		break;
	case EBNRaceState::Racing:
		if (bDrift)
		{
			const FString TargetPart = Race.GetActiveBossDriftTarget() > 0
				? FString::Printf(TEXT(" / %d"), Race.GetActiveBossDriftTarget()) : FString();
			DrawTextShadowed(FString::Printf(TEXT("%d%s PTS   %s"), Drift.Total(), *TargetPart, *FormatRaceTime(Race.GetTimeLeft())),
				RaceColour, Margin, RaceTop + 22.f, Digits, 0.75f);
			if (Drift.IsDrifting())
			{
				DrawCentred(FString::Printf(TEXT("+%d   x%d"), FMath::RoundToInt(Drift.Current()), Drift.Multiplier()), RaceTop + 88.f, 0.98f, DriftColour, Digits);
			}
		}
		else
		{
			const FString PosStr = bRivals ? FString::Printf(TEXT("POS %d/%d   •   "), Race.GetPlayerPosition(), Race.GetNumRivals() + 1) : FString();
			const FString LapStr = Def->Discipline == EBNRaceDiscipline::Circuit
				? FString::Printf(TEXT("TUR %d/%d   •   "), Progress.CurrentLap(), Progress.TotalLaps()) : FString();
			const FString GateStr = FString::Printf(TEXT("KAPI %d/%d"),
				FMath::Min(Progress.NextCheckpoint() + 1, Def->Checkpoints.Num()), FMath::Max(1, Def->Checkpoints.Num()));
			DrawTextShadowed(FString::Printf(TEXT("%s%s%s"), *PosStr, *LapStr, *GateStr), RaceColour, Margin, RaceTop + 22.f, Digits, 0.75f);

			// Race clock, top right, display numerals
			const FString TimeStr = FormatRaceTime(Race.GetRaceTime());
			float TW = 0.f, TH = 0.f;
			GetTextSize(TimeStr, TW, TH, Digits, 0.85f);
			DrawTextShadowed(TimeStr, RaceColour, Canvas->ClipX - Margin - TW, RaceTop, Digits, 0.85f);
		}
		break;
	case EBNRaceState::Finished:
		DrawCentred(bDrift ? FString::Printf(TEXT("SKOR  %d"), Drift.Total())
			: Progress.IsFinished() ? FString::Printf(TEXT("%s  —  %s"),
				bRivals ? *Ordinal(Race.GetPlayerPosition()) : TEXT("BİTİŞ"), *FormatRaceTime(Progress.FinishTime()))
			: FString(TEXT("YARIŞ İPTAL")), RaceTop + 60.f, 0.85f, DashColour, Digits);
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

void ABNHUD::DrawGaugeBar(const TCHAR* Label, float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColour, bool bActive)
{
	UFont* Font = BNUITheme::HudFont();
	const float LabelW = 74.f;
	const float BarX = X + LabelW;
	const float BarW = Width - LabelW;
	DrawText(Label, bActive ? FillColour : LegendColour, X, Y - 3.f, Font, 0.70f);
	DrawRect(BarBackground, BarX, Y, BarW, Height);
	const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
	if (Clamped > 0.f)
	{
		DrawRect(FillColour, BarX, Y, BarW * Clamped, Height);
	}
	DrawLine(BarX, Y, BarX + BarW, Y, FLinearColor(0.32f, 0.30f, 0.26f, 0.9f), 1.f);
	DrawLine(BarX, Y + Height, BarX + BarW, Y + Height, FLinearColor(0.32f, 0.30f, 0.26f, 0.9f), 1.f);
}

void ABNHUD::DrawDashboard(const ABNVehiclePawn& Car, const FBNDashReadout& Dash, bool bMenuBlocking)
{
	UFont* Digits = BNUITheme::HudFont(true);
	UFont* Label = BNUITheme::HudFont(false);
	const float Right = Canvas->ClipX - Margin;
	const float Bottom = Canvas->ClipY - Margin;
	const float Left = Right - ClusterWidth;

	const bool bCountdownLaunch = Car.IsInputLocked() && !bMenuBlocking;
	const bool bDrag = Car.IsDragModeActive();
	const bool bNos = Car.IsNosActive();
	const float LiveRpmFrac = bDrag ? FMath::Clamp(Car.GetDragState().RpmFraction, 0.f, 1.f) : Dash.RpmFraction;
	const FString ShownGear = bDrag ? FString::FromInt(Car.GetDragState().Gear) : Dash.Gear;
	const float ShiftFlash = Car.GetShiftFlashFraction();
	const float ShownFraction = FMath::Clamp(bCountdownLaunch ? Car.GetLaunchRevFraction() : LiveRpmFrac, 0.f, 1.f);

	// 1. NOS + Speedbreaker: two slim bars on the bottom edge of the cluster.
	const float BarH = 5.f;
	const float HalfW = (ClusterWidth - 14.f) * 0.5f;
	auto DrawResourceBar = [&](const TCHAR* Tag, float X, float Fraction, const FLinearColor& Col, bool bActive)
	{
		DrawText(Tag, bActive ? Col : LegendColour, X, Bottom - BarH - 18.f, Label, 0.62f);
		DrawRect(BarBackground, X, Bottom - BarH, HalfW, BarH);
		DrawRect(Col, X, Bottom - BarH, HalfW * FMath::Clamp(Fraction, 0.f, 1.f), BarH);
	};
	DrawResourceBar(TEXT("NOS"), Left, Car.GetNosFraction(), bNos ? NosActiveColour : NosColour, bNos);
	DrawResourceBar(TEXT("SPEEDBREAKER"), Left + HalfW + 14.f, Car.GetSpeedbreakerFraction(),
		Car.IsSpeedbreakerActive() ? SpeedbreakerActiveColour : SpeedbreakerColour, Car.IsSpeedbreakerActive());

	// 2. Speed: huge italic numerals, right aligned; KM/H tucked under the last digit.
	const FString SpeedStr = FString::FromInt(Dash.SpeedKmh);
	float SpeedW = 0.f, SpeedH = 0.f;
	UFont* Numeral = BNUITheme::HudNumeralFont();
	GetTextSize(SpeedStr, SpeedW, SpeedH, Numeral, SpeedScale);
	const float SpeedY = Bottom - 60.f - SpeedH * 0.92f;
	DrawText(SpeedStr, FLinearColor(0.f, 0.f, 0.f, 0.55f), Right - SpeedW + 2.f, SpeedY + 3.f, Numeral, SpeedScale);
	DrawText(SpeedStr, bNos ? NosActiveColour : RaceColour, Right - SpeedW, SpeedY, Numeral, SpeedScale);
	float UnitW = 0.f, UnitH = 0.f;
	GetTextSize(TEXT("KM/H"), UnitW, UnitH, Label, 0.66f);
	DrawText(TEXT("KM/H"), LegendColour, Right - UnitW, Bottom - 34.f - UnitH, Label, 0.66f);

	// 3. Gear: square slab at the cluster's left, flashes sodium on shifts.
	float GearW = 0.f, GearH = 0.f;
	GetTextSize(ShownGear, GearW, GearH, Digits, GearScale);
	const float GearBox = 58.f;
	const float GearX = Left;
	const float GearY = Bottom - 60.f - GearBox;
	const FLinearColor GearBg = FMath::Lerp(SmokedPanelBg, bNos ? NosActiveColour : DashColour, FMath::Clamp(ShiftFlash, 0.f, 1.f));
	DrawRect(GearBg, GearX, GearY, GearBox, GearBox);
	DrawRect(bNos ? NosActiveColour : DashColour, GearX, GearY + GearBox - 3.f, GearBox, 3.f);
	DrawText(ShownGear, ShiftFlash > 0.4f ? BNUITheme::InkSolid : RaceColour,
		GearX + (GearBox - GearW) * 0.5f, GearY + (GearBox - GearH) * 0.5f, Digits, GearScale);

	// 4. RPM: rising stepped wedge across the cluster top (taller = higher revs), redline in red.
	const float WedgeBottom = FMath::Min(SpeedY + SpeedH * 0.22f, GearY) - 8.f;
	const float SegGap = 3.f;
	const float SegW = (ClusterWidth - SegGap * (RpmSegments - 1)) / RpmSegments;
	for (int32 i = 0; i < RpmSegments; ++i)
	{
		const float T = (static_cast<float>(i) + 0.5f) / RpmSegments;
		const float H = FMath::Lerp(RpmWedgeMinH, RpmWedgeMaxH, FMath::Pow(T, 1.35f));
		const bool bLit = T <= ShownFraction;
		const bool bRed = T >= RedlineFraction;
		const bool bLaunch = bCountdownLaunch && T >= Car.Boost.LaunchSweetSpotMin && T <= Car.Boost.LaunchSweetSpotMax;
		FLinearColor Col = bLaunch ? SweetSpotColour : bRed ? RedlineColour : bNos ? NosActiveColour : RaceColour;
		if (!bLit)
		{
			Col = bLaunch ? FLinearColor(SweetSpotColour.R, SweetSpotColour.G, SweetSpotColour.B, 0.35f)
				: bRed ? FLinearColor(RedlineColour.R, RedlineColour.G, RedlineColour.B, 0.30f) : UnlitSegment;
		}
		DrawRect(Col, Left + i * (SegW + SegGap), WedgeBottom - H, SegW, H);
	}

	if (bCountdownLaunch)
	{
		DrawCentred(TEXT("GAZ VER (W)  —  KALKIŞ BÖLGESİ"), WedgeBottom - RpmWedgeMaxH - 34.f, LabelScale,
			Car.IsInLaunchSweetSpot() ? SweetSpotColour : DashColour);
	}
}

void ABNHUD::DrawPursuit(const UBNPursuitSubsystem& Pursuit, const UBNStorySubsystem* Story, bool bRaceActive)
{
	const FBNPursuitState& S = Pursuit.GetState();
	UFont* Font = BNUITheme::HudFont();
	const FString CostText = FString::Printf(TEXT("FATURA: $%s"), *FString::FormatAsNumber(S.CostToStateDollars));

	if (S.Phase == EBNPursuitPhase::None)
	{
		if (S.CostToStateDollars > 0 && !bRaceActive)
		{
			float CostW = 0.f, CostH = 0.f;
			GetTextSize(CostText, CostW, CostH, Font, 0.78f);
			DrawTextShadowed(CostText, LegendColour, Canvas->ClipX - Margin - CostW, Margin, Font, 0.78f);
		}
		return;
	}

	const float TopY = bRaceActive ? (RaceTop + 68.f) : (RaceTop + 2.f);
	const float RibbonW = PursuitBarWidth + 56.f;
	const float RibbonH = 92.f;
	const float RibbonX = (Canvas->ClipX - RibbonW) * 0.5f;
	const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(S.HeatLevel);
	const bool bFlash = FMath::Fmod(GetWorld()->GetTimeSeconds() * 6.5, 1.0) < 0.5;

	// Ink ribbon with a split red/blue siren edge that alternates on the strobe.
	DrawRect(SmokedPanelBg, RibbonX, TopY - 6.f, RibbonW, RibbonH);
	DrawRect(bFlash ? PoliceRedColour : PoliceBlueColour, RibbonX, TopY - 6.f, RibbonW * 0.5f, 3.f);
	DrawRect(bFlash ? PoliceBlueColour : PoliceRedColour, RibbonX + RibbonW * 0.5f, TopY - 6.f, RibbonW * 0.5f, 3.f);

	const float TrackX = (Canvas->ClipX - PursuitBarWidth) * 0.5f;
	const FString LeftInfo = FString::Printf(TEXT("%s  •  BİRİM: %d"), *Spec.Label, Pursuit.GetActivePoliceCount());
	DrawText(LeftInfo, Spec.bDemirInvolved ? PoliceRedColour : DashColour, TrackX + 18.f, TopY + 12.f, Font, 0.80f);

	const FString RightInfo = FString::Printf(TEXT("ÖDÜL: %s  •  %s"), *FString::FormatAsNumber(S.Bounty), *CostText);
	float RightW = 0.f, RightH = 0.f;
	GetTextSize(RightInfo, RightW, RightH, Font, 0.80f);
	DrawText(RightInfo, RaceColour, TrackX + PursuitBarWidth - RightW - 18.f, TopY + 12.f, Font, 0.80f);

	// Classic MW 2005 BUSTED <---- | ----> EVADE Tug-of-War Bar!
	const float MeterX = TrackX + 64.f;
	const float MeterW = PursuitBarWidth - 128.f;
	const float MeterY = TopY + 38.f;
	const float CenterX = MeterX + MeterW * 0.5f;
	DrawRect(BarBackground, MeterX, MeterY, MeterW, PursuitBarHeight);

	FString StatusLabel;
	if (S.Phase == EBNPursuitPhase::Pursuit)
	{
		// Pulls LEFT toward crimson as BustMeter builds; otherwise subtle pulsing center indicator
		if (S.BustMeter > 0.02f)
		{
			const float W = (MeterW * 0.5f) * FMath::Clamp(S.BustMeter, 0.f, 1.f);
			DrawRect(bFlash ? PoliceRedColour : FLinearColor(0.75f, 0.10f, 0.08f, 1.f), CenterX - W, MeterY + 2.f, W, PursuitBarHeight - 4.f);
			StatusLabel = TEXT("<<  YAKALANIYORSUN (BUSTED)  <<");
		}
		else
		{
			StatusLabel = FString::Printf(TEXT("TAKİP  %s  •  DEVRE DIŞI: %d"), *FormatRaceTime(S.PursuitTimeSeconds), S.UnitsDisabled);
		}
	}
	else if (S.Phase == EBNPursuitPhase::Evade)
	{
		const float W = (MeterW * 0.5f) * FMath::Clamp(S.EvadeMeter, 0.f, 1.f);
		DrawRect(NosColour, CenterX, MeterY + 2.f, W, PursuitBarHeight - 4.f);
		StatusLabel = TEXT(">>  KAÇIŞ  //  EVADE  >>");
	}
	else if (S.Phase == EBNPursuitPhase::Cooldown)
	{
		const float W = MeterW * FMath::Clamp(S.PhaseProgress, 0.f, 1.f);
		DrawRect(S.bInsideCooldownSpot ? SweetSpotColour : PoliceBlueColour, MeterX, MeterY + 2.f, W, PursuitBarHeight - 4.f);
		StatusLabel = S.bInsideCooldownSpot
			? FString::Printf(TEXT("SAKLANMA (4x): %s"), *S.ActiveCooldownSpotName.ToUpper())
			: FString(TEXT("SOĞUMA  //  SAKLAN"));
	}
	else if (S.Phase == EBNPursuitPhase::Busted)
	{
		DrawRect(PoliceRedColour, MeterX, MeterY + 2.f, MeterW, PursuitBarHeight - 4.f);
		StatusLabel = TEXT("YAKALANDIN  //  BUSTED");
	}
	else if (S.Phase == EBNPursuitPhase::Escaped)
	{
		DrawRect(DashColour, MeterX, MeterY + 2.f, MeterW, PursuitBarHeight - 4.f);
		StatusLabel = FString::Printf(TEXT("KAÇIŞ BAŞARILI  //  +%s ÖDÜL"), *FString::FormatAsNumber(S.Bounty));
	}

	// Center tick + left/right strobe labels ("BUSTED" / "EVADE")
	DrawRect(RaceColour, CenterX - 1.5f, MeterY - 2.f, 3.f, PursuitBarHeight + 4.f);
	DrawText(TEXT("BUST"), bFlash ? PoliceRedColour : LegendColour, TrackX + 12.f, MeterY + 3.f, Font, 0.72f);
	DrawText(TEXT("KAÇIŞ"), !bFlash ? PoliceBlueColour : LegendColour, TrackX + PursuitBarWidth - 56.f, MeterY + 3.f, Font, 0.72f);
	DrawCentred(StatusLabel, MeterY + 1.f, 0.86f, RaceColour);

	float ExtraY = TopY + RibbonH - 2.f;
	if (S.MilestoneBannerSeconds > 0.f && !S.MilestoneBannerText.IsEmpty())
	{
		DrawCentred(S.MilestoneBannerText, ExtraY, 0.95f, DashColour);
		ExtraY += 24.f;
	}
	if (Pursuit.IsPlayerTireSpiked())
	{
		DrawCentred(TEXT("DİKENLİ ŞERİT! LASTİKLER PATLADI — HIZ KISITLI!"), ExtraY, 0.92f, bFlash ? PoliceRedColour : DashColour);
		ExtraY += 24.f;
	}
	if (Story && Story->GetState().bFinalChaseActive)
	{
		const int32 DistA = FMath::RoundToInt(UBNStorySubsystem::GetDistanceToEscapeRouteCm(Cast<ABNVehiclePawn>(GetOwningPawn()), 0) / 100.f);
		const int32 DistB = FMath::RoundToInt(UBNStorySubsystem::GetDistanceToEscapeRouteCm(Cast<ABNVehiclePawn>(GetOwningPawn()), 1) / 100.f);
		DrawCentred(FString::Printf(TEXT("ÇIKIŞ ROTALARI:  [A] LİMAN %dm   •   [B] KUZEY %dm"), DistA, DistB), ExtraY, 0.88f, NosActiveColour);
		ExtraY += 24.f;
	}
	if (S.ActiveRadio.RemainingSeconds > 0.f && !S.ActiveRadio.Message.IsEmpty())
	{
		const FString RadioText = FString::Printf(TEXT("[%s] %s"), *S.ActiveRadio.Speaker, *S.ActiveRadio.Message);
		float RadioW = 0.f, RadioH = 0.f;
		GetTextSize(RadioText, RadioW, RadioH, Font, 0.82f);
		const float SlabW = RadioW + 40.f;
		DrawRect(SmokedPanelBg, (Canvas->ClipX - SlabW) * 0.5f, ExtraY - 2.f, SlabW, RadioH + 10.f);
		DrawCentred(RadioText, ExtraY + 3.f, 0.82f, LegendColour);
	}
}

void ABNHUD::DrawHUD()
{
	Super::DrawHUD();

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (!Car || !Canvas)
	{
		return;
	}

	if (!bAppliedAudioGainToCar && Car->EngineAudio)
	{
		Car->EngineAudio->MasterGain = GPreferredEngineGain;
		bAppliedAudioGainToCar = true;
	}

	UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr;

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (Prog)
	{
		Prog->TickFeedback(FrameDt);
		if (Prog->IsGarageOpen() && !IsMenuOpen())
		{
			OpenMenu(EBNMenuTab::Garage);
		}
	}
	if (Story)
	{
		Story->TickComms(FrameDt);
		if (Prog)
		{
			Story->NotifyReputationUpdated(Prog->GetProfile().Reputation);
		}
	}

	if (CanUseViewportSlate())
	{
		if (const UGameInstance* GI = GetGameInstance(); GI && GTitleShownForGI.Get() != GI)
		{
			GTitleShownForGI = GI;
			OpenTitleScreen();
		}
		else
		{
			UpdateRaceAndPursuitTracking(Prog, Story, Pursuit);
		}
	}
	SyncStoryCardOverlay();
	SyncShowcaseCamera(Car);

	if (IsMenuOpen())
	{
		return;
	}

	const ABNRaceEvent* ActiveRace = nullptr;
	for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
	{
		if (It->GetState() != EBNRaceState::Idle)
		{
			ActiveRace = *It;
			DrawRace(**It);
			break;
		}
	}

	const bool bRaceActive = ActiveRace != nullptr;
	const bool bDragBarVisible = bRaceActive && ActiveRace->GetState() == EBNRaceState::Racing && Car->IsDragModeActive();
	if (bDragBarVisible)
	{
		DrawDragShiftBar(*Car);
	}
	const bool bPursuitVisible = Pursuit && Pursuit->GetState().Phase != EBNPursuitPhase::None;
	if (Pursuit)
	{
		DrawPursuit(*Pursuit, Story, bRaceActive);
	}

	const float BannerY = RaceTop + (bDragBarVisible ? 166.f : (bPursuitVisible ? (bRaceActive ? 168.f : 108.f) : 96.f));
	DrawActionBanners(*Car, Pursuit, BannerY);

	if (Prog)
	{
		DrawEconomyPill(*Prog, Story, bRaceActive, bPursuitVisible);
	}

	DrawWorldMarkers(*Car, ActiveRace, Story);
	DrawRadar(*Car, ActiveRace, Pursuit, Story);
	DrawLiveCommsAndPrompts(*Car, ActiveRace, Story);
	DrawProTelemetry(*Car, ActiveRace, bPursuitVisible);
	const bool bMenuBlocking = HasStoryCardOpen() || (Prog && Prog->IsGarageOpen());
	DrawDashboard(*Car, MakeReadout(*Car), bMenuBlocking);
	DrawCameraModePill(*Car);
}
