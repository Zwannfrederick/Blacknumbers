#include "UI/BNHUD.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
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
	constexpr float SpeedScale = 2.30f;
	constexpr float GearScale = 1.35f;
	constexpr float LabelScale = 0.88f;
	constexpr float RedlineFraction = 0.82f;
	constexpr int32 TachArcSegments = 36;
	constexpr float TachStartDeg = 135.f;
	constexpr float TachSweepDeg = 270.f;
	constexpr float TachDiameter = 204.f;
	constexpr float TachRadius = TachDiameter * 0.5f;

	const FLinearColor DashColour(0.96f, 0.72f, 0.16f, 1.f);
	const FLinearColor RedlineColour(0.96f, 0.20f, 0.14f, 1.f);
	const FLinearColor SweetSpotColour(0.18f, 0.90f, 0.45f, 1.f);
	const FLinearColor NosColour(0.16f, 0.78f, 1.f, 1.f);
	const FLinearColor NosActiveColour(0.55f, 0.94f, 1.f, 1.f);
	const FLinearColor SpeedbreakerColour(0.95f, 0.72f, 0.22f, 1.f);
	const FLinearColor SpeedbreakerActiveColour(1.f, 0.92f, 0.55f, 1.f);
	const FLinearColor SmokedPanelBg(0.032f, 0.036f, 0.045f, 0.82f);
	const FLinearColor BarBackground(0.08f, 0.09f, 0.11f, 0.90f);
	const FLinearColor FrameTrimColour(0.95f, 0.66f, 0.08f, 0.90f);
	const FLinearColor LegendColour(0.84f, 0.82f, 0.76f, 0.92f);
	const FLinearColor RaceColour(0.96f, 0.94f, 0.90f, 1.f);
	const FLinearColor DriftColour(0.28f, 0.86f, 1.f, 1.f);
	const FLinearColor PoliceRedColour(0.96f, 0.18f, 0.14f, 1.f);
	const FLinearColor PoliceBlueColour(0.18f, 0.58f, 1.f, 1.f);
	const FLinearColor RadioBgColour(0.025f, 0.028f, 0.036f, 0.88f);

	constexpr float RaceTop = 26.f;
	constexpr float RaceLine = 38.f;
	constexpr float PursuitBarWidth = 610.f;
	constexpr float PursuitBarHeight = 24.f;
	constexpr float CountdownScale = 4.6f;
	constexpr float RaceScale = 1.45f;

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
		for (const TSharedPtr<SWidget>& Host : { TitleScreenHost, ResultsHost, MenuHost, StoryCardHost })
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
	if (MenuWidget.IsValid())
	{
		MenuWidget->SetActiveTab(Tab);
		SyncShowcaseCamera(Cast<ABNVehiclePawn>(GetOwningPawn()));
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
		if (Car && !bWasPausedForRace)
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
		DismissTitleScreen(true);
		return true;
	}
	if (IsResultsOverlayOpen())
	{
		DismissResultsOverlay(true);
		return true;
	}
	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		if (Story->HasActiveCard() && !MenuWidget.IsValid())
		{
			Story->DismissActiveCard(Cast<ABNVehiclePawn>(GetOwningPawn()));
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

void ABNHUD::SyncStoryCardOverlay()
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	const bool bSuppressed = IsTitleScreenOpen() || IsPauseMenuOpen() || IsResultsOverlayOpen();
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
	const bool bRivals = Race.GetNumRivals() > 0;
	const int32 BossRank = Race.GetActiveBossRank();
	const FString Header = BossRank > 0
		? FString::Printf(TEXT("BLACK NUMBERS #%02d  //  %s"), BossRank, *Def->DisplayName.ToString().ToUpper())
		: Def->DisplayName.ToString().ToUpper();

	// Sleek smoked Most Wanted race header ribbon so telemetry pops over bright skies
	const float RibbonW = 540.f;
	const float RibbonX = (Canvas->ClipX - RibbonW) * 0.5f;
	DrawRect(SmokedPanelBg, RibbonX, RaceTop - 6.f, RibbonW, 74.f);
	DrawRect(BossRank > 0 ? DashColour : FrameTrimColour, RibbonX, RaceTop - 6.f, RibbonW, 3.f);

	DrawCentred(Header, RaceTop + 2.f, LabelScale * 1.05f, BossRank > 0 ? DashColour : LegendColour);
	const float SubY = RaceTop + 28.f;

	switch (Race.GetState())
	{
	case EBNRaceState::Countdown:
		DrawCentred(TEXT("HAZIRLAN..."), SubY, LabelScale * 1.15f, DashColour);
		DrawCentred(FString::FromInt(FMath::CeilToInt(Race.GetRaceTime())), RaceTop + 90.f, CountdownScale, DashColour);
		break;
	case EBNRaceState::Racing:
		if (bDrift)
		{
			const FString TargetPart = Race.GetActiveBossDriftTarget() > 0
				? FString::Printf(TEXT(" / HEDEF %d"), Race.GetActiveBossDriftTarget()) : FString();
			DrawCentred(FString::Printf(TEXT("%d%s PTS    |    %s"), Drift.Total(), *TargetPart, *FormatRaceTime(Race.GetTimeLeft())), SubY, RaceScale * 0.92f, RaceColour);
			if (Drift.IsDrifting())
			{
				DrawCentred(FString::Printf(TEXT("+%d   x%d"), FMath::RoundToInt(Drift.Current()), Drift.Multiplier()), RaceTop + 82.f, RaceScale * 1.35f, DriftColour);
			}
		}
		else
		{
			const FString Position = bRivals
				? FString::Printf(TEXT("POS %d/%d   |   "), Race.GetPlayerPosition(), Race.GetNumRivals() + 1) : FString();
			const FString Laps = Def->Discipline == EBNRaceDiscipline::Circuit
				? FString::Printf(TEXT("TUR %d/%d   |   "), Progress.CurrentLap(), Progress.TotalLaps()) : FString();
			const FString GateStr = FString::Printf(TEXT("KAPI %d/%d   |   "),
				FMath::Min(Progress.NextCheckpoint() + 1, Def->Checkpoints.Num()), FMath::Max(1, Def->Checkpoints.Num()));
			DrawCentred(FString::Printf(TEXT("%s%s%s%s"), *Position, *Laps, *GateStr, *FormatRaceTime(Race.GetRaceTime())), SubY, RaceScale * 0.85f, RaceColour);
		}
		break;
	case EBNRaceState::Finished:
		DrawCentred(bDrift ? FString::Printf(TEXT("SKOR  %d"), Drift.Total())
			: Progress.IsFinished() ? FString::Printf(TEXT("%s  —  %s"),
				bRivals ? *Ordinal(Race.GetPlayerPosition()) : TEXT("BİTİŞ"), *FormatRaceTime(Progress.FinishTime()))
			: FString(TEXT("YARIŞ İPTAL")), SubY, RaceScale * 1.05f, DashColour);
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
	UFont* Font = GEngine->GetLargeFont();
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
	UFont* Font = GEngine->GetLargeFont();
	const float Cx = Canvas->ClipX - Margin - TachRadius - 4.f;
	const float Cy = Canvas->ClipY - Margin - TachRadius - 36.f;

	// Authentic Most Wanted circular metallic tachometer dial texture
	if (UTexture2D* TachTex = BNUITheme::GetTachRingTexture())
	{
		DrawTexture(TachTex, Cx - TachRadius, Cy - TachRadius, TachDiameter, TachDiameter,
			0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Translucent);
	}
	else
	{
		DrawRect(SmokedPanelBg, Cx - TachRadius, Cy - TachRadius, TachDiameter, TachDiameter);
	}

	const bool bCountdownLaunch = Car.IsInputLocked() && !bMenuBlocking;
	const float ShownFraction = FMath::Clamp(bCountdownLaunch ? Car.GetLaunchRevFraction() : Dash.RpmFraction, 0.f, 1.f);
	const float ArcRadius = TachRadius * 0.855f;

	// Dynamic glowing RPM sweep arc along the inside of the metallic bezel
	for (int32 i = 0; i < TachArcSegments; ++i)
	{
		const float T0 = static_cast<float>(i) / TachArcSegments;
		const float T1 = static_cast<float>(i + 1) / TachArcSegments;
		const float A0 = FMath::DegreesToRadians(TachStartDeg + T0 * TachSweepDeg);
		const float A1 = FMath::DegreesToRadians(TachStartDeg + T1 * TachSweepDeg);
		const bool bLit = T0 < ShownFraction;
		const bool bRed = T0 >= RedlineFraction;
		const bool bLaunch = bCountdownLaunch && T0 >= Car.Boost.LaunchSweetSpotMin && T0 <= Car.Boost.LaunchSweetSpotMax;
		if (!bLit && !bLaunch)
		{
			continue;
		}
		const FLinearColor SegCol = bLaunch ? (bLit ? SweetSpotColour : FLinearColor(0.18f, 0.90f, 0.45f, 0.40f))
			: bRed ? RedlineColour : Car.IsNosActive() ? NosActiveColour : DashColour;
		DrawLine(Cx + FMath::Cos(A0) * ArcRadius, Cy + FMath::Sin(A0) * ArcRadius,
			Cx + FMath::Cos(A1) * ArcRadius, Cy + FMath::Sin(A1) * ArcRadius, SegCol, bLit ? 5.5f : 3.5f);
	}

	// Analog tachometer needle sweeping cleanly over the tick ring
	const float NeedleRad = FMath::DegreesToRadians(TachStartDeg + ShownFraction * TachSweepDeg);
	const float CosN = FMath::Cos(NeedleRad);
	const float SinN = FMath::Sin(NeedleRad);
	const FLinearColor NeedleCol = ShownFraction >= RedlineFraction ? RedlineColour : Car.IsNosActive() ? NosActiveColour : DashColour;
	DrawLine(Cx + CosN * 48.f, Cy + SinN * 48.f, Cx + CosN * (TachRadius * 0.87f), Cy + SinN * (TachRadius * 0.87f),
		FLinearColor(NeedleCol.R, NeedleCol.G, NeedleCol.B, 0.45f), 6.f);
	DrawLine(Cx + CosN * 46.f, Cy + SinN * 46.f, Cx + CosN * (TachRadius * 0.89f), Cy + SinN * (TachRadius * 0.89f),
		RaceColour, 2.5f);

	if (bCountdownLaunch)
	{
		DrawCentred(TEXT("GAZ VER (W)  —  KALKIŞ BÖLGESİ"), Cy - TachRadius - 26.f, LabelScale,
			Car.IsInLaunchSweetSpot() ? SweetSpotColour : DashColour);
	}

	// Center digital KM/H + Gear badge inside the smoked dial core
	const FString SpeedStr = FString::Printf(TEXT("%d"), Dash.SpeedKmh);
	float SpeedW = 0.f, SpeedH = 0.f;
	GetTextSize(SpeedStr, SpeedW, SpeedH, Font, SpeedScale);
	DrawText(SpeedStr, RaceColour, Cx - SpeedW * 0.5f, Cy - SpeedH * 0.65f, Font, SpeedScale);

	float UnitW = 0.f, UnitH = 0.f;
	GetTextSize(TEXT("KM/H"), UnitW, UnitH, Font, 0.72f);
	DrawText(TEXT("KM/H"), DashColour, Cx - UnitW * 0.5f, Cy + SpeedH * 0.16f, Font, 0.72f);

	float GearW = 0.f, GearH = 0.f;
	GetTextSize(Dash.Gear, GearW, GearH, Font, GearScale);
	const float GearBoxW = FMath::Max(GearW + 18.f, 36.f);
	const float GearY = Cy + 42.f;
	DrawRect(BarBackground, Cx - GearBoxW * 0.5f, GearY, GearBoxW, GearH + 4.f);
	DrawRect(Car.IsNosActive() ? NosActiveColour : FrameTrimColour, Cx - GearBoxW * 0.5f, GearY, GearBoxW, 2.f);
	DrawText(Dash.Gear, Car.IsNosActive() ? NosActiveColour : DashColour, Cx - GearW * 0.5f, GearY + 2.f, Font, GearScale);

	// Sleek NOS & Speedbreaker gauge dock tucked directly beneath the circular dial
	const float DockW = TachDiameter + 6.f;
	const float DockX = Cx - DockW * 0.5f;
	const float DockY = Cy + TachRadius + 4.f;
	DrawRect(SmokedPanelBg, DockX, DockY, DockW, 38.f);
	DrawRect(FrameTrimColour, DockX, DockY, 3.f, 38.f);
	DrawGaugeBar(TEXT("NOS [SHF]"), DockX + 8.f, DockY + 6.f, DockW - 14.f, 9.f, Car.GetNosFraction(),
		Car.IsNosActive() ? NosActiveColour : NosColour, Car.IsNosActive());
	DrawGaugeBar(TEXT("SPD [X]"), DockX + 8.f, DockY + 22.f, DockW - 14.f, 9.f, Car.GetSpeedbreakerFraction(),
		Car.IsSpeedbreakerActive() ? SpeedbreakerActiveColour : SpeedbreakerColour, Car.IsSpeedbreakerActive());
}

void ABNHUD::DrawPursuit(const UBNPursuitSubsystem& Pursuit, const UBNStorySubsystem* Story, bool bRaceActive)
{
	const FBNPursuitState& S = Pursuit.GetState();
	UFont* Font = GEngine->GetLargeFont();
	const FString CostText = FString::Printf(TEXT("ŞEHİR FATURASI: $%s"), *FString::FormatAsNumber(S.CostToStateDollars));

	if (S.Phase == EBNPursuitPhase::None)
	{
		if (S.CostToStateDollars > 0)
		{
			float CostW = 0.f, CostH = 0.f;
			GetTextSize(CostText, CostW, CostH, Font, 0.80f);
			DrawRect(SmokedPanelBg, Canvas->ClipX - Margin - CostW - 18.f, Margin - 4.f, CostW + 18.f, 26.f);
			DrawRect(FrameTrimColour, Canvas->ClipX - Margin - 3.f, Margin - 4.f, 3.f, 26.f);
			DrawText(CostText, DashColour, Canvas->ClipX - Margin - CostW - 10.f, Margin, Font, 0.80f);
		}
		return;
	}

	const float TopY = bRaceActive ? (RaceTop + 86.f) : RaceTop;
	const float BarX = (Canvas->ClipX - PursuitBarWidth) * 0.5f;
	const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(S.HeatLevel);
	const bool bFlash = FMath::Fmod(GetWorld()->GetTimeSeconds() * 6.0, 1.0) < 0.5;

	DrawRect(SmokedPanelBg, BarX - 16.f, TopY - 8.f, PursuitBarWidth + 32.f, 64.f);
	// Most Wanted red/blue siren strobe endcaps
	DrawRect(bFlash ? PoliceRedColour : PoliceBlueColour, BarX - 16.f, TopY - 8.f, 6.f, 64.f);
	DrawRect(bFlash ? PoliceBlueColour : PoliceRedColour, BarX + PursuitBarWidth + 10.f, TopY - 8.f, 6.f, 64.f);
	DrawRect(FrameTrimColour, BarX - 10.f, TopY - 8.f, PursuitBarWidth + 20.f, 2.f);

	const FString LeftInfo = FString::Printf(TEXT("%s   //   BİRİM: %d"), *Spec.Label, Pursuit.GetActivePoliceCount());
	DrawText(LeftInfo, Spec.bDemirInvolved ? PoliceRedColour : DashColour, BarX, TopY - 2.f, Font, LabelScale);

	float CostW = 0.f, CostH = 0.f;
	GetTextSize(CostText, CostW, CostH, Font, LabelScale);
	DrawText(CostText, RaceColour, BarX + PursuitBarWidth - CostW, TopY - 2.f, Font, LabelScale);

	const float MeterY = TopY + 23.f;
	DrawRect(BarBackground, BarX, MeterY, PursuitBarWidth, PursuitBarHeight);

	FString StatusLabel;
	FLinearColor FillColour = PoliceBlueColour;
	float FillFraction = 0.f;
	switch (S.Phase)
	{
	case EBNPursuitPhase::Pursuit:
		FillFraction = S.BustMeter > 0.02f ? S.BustMeter : 1.f;
		FillColour = bFlash ? PoliceRedColour : PoliceBlueColour;
		StatusLabel = S.BustMeter > 0.02f ? TEXT("YAKALANIYORSUN  //  BUSTED RİSKİ") : FString::Printf(TEXT("POLİS TAKİBİ   %s"), *FormatRaceTime(S.PursuitTimeSeconds));
		break;
	case EBNPursuitPhase::Evade:
		FillFraction = S.EvadeMeter;
		FillColour = NosColour;
		StatusLabel = TEXT("KAÇIŞ  //  EVADE");
		break;
	case EBNPursuitPhase::Cooldown:
		FillFraction = S.PhaseProgress;
		FillColour = S.bInsideCooldownSpot ? SweetSpotColour : PoliceBlueColour;
		StatusLabel = S.bInsideCooldownSpot
			? FString::Printf(TEXT("SAKLANMA NOKTASI (3.5x): %s"), *S.ActiveCooldownSpotName.ToUpper())
			: FString(TEXT("SOĞUMA  //  SAKLAN"));
		break;
	case EBNPursuitPhase::Busted:
		FillFraction = 1.f;
		FillColour = PoliceRedColour;
		StatusLabel = TEXT("YAKALANDIN");
		break;
	case EBNPursuitPhase::Escaped:
		FillFraction = 1.f;
		FillColour = DashColour;
		StatusLabel = TEXT("KAÇIŞ BAŞARILI");
		break;
	default:
		break;
	}

	if (FillFraction > 0.f)
	{
		DrawRect(FillColour, BarX, MeterY, PursuitBarWidth * FMath::Clamp(FillFraction, 0.f, 1.f), PursuitBarHeight);
	}
	DrawCentred(StatusLabel, MeterY + 1.f, LabelScale * 1.02f, RaceColour);

	float ExtraY = TopY + 62.f;
	if (Pursuit.IsPlayerTireSpiked())
	{
		DrawCentred(TEXT("DİKENLİ ŞERİT! LASTİKLER HASARLI (HIZ KISITLI)"), ExtraY, LabelScale * 1.02f,
			bFlash ? PoliceRedColour : DashColour);
		ExtraY += 24.f;
	}
	if (Story && Story->GetState().bFinalChaseActive)
	{
		const int32 DistA = FMath::RoundToInt(UBNStorySubsystem::GetDistanceToEscapeRouteCm(Cast<ABNVehiclePawn>(GetOwningPawn()), 0) / 100.f);
		const int32 DistB = FMath::RoundToInt(UBNStorySubsystem::GetDistanceToEscapeRouteCm(Cast<ABNVehiclePawn>(GetOwningPawn()), 1) / 100.f);
		DrawCentred(FString::Printf(TEXT("ÇIKIŞ ROTALARI:  [A] LİMAN %dm   |   [B] KUZEY %dm"), DistA, DistB), ExtraY, LabelScale, NosActiveColour);
		ExtraY += 26.f;
	}
	if (S.ActiveRadio.RemainingSeconds > 0.f && !S.ActiveRadio.Message.IsEmpty())
	{
		const FString RadioText = FString::Printf(TEXT("[%s] %s"), *S.ActiveRadio.Speaker, *S.ActiveRadio.Message);
		float RadioW = 0.f, RadioH = 0.f;
		GetTextSize(RadioText, RadioW, RadioH, Font, LabelScale);
		const float RadioBoxW = FMath::Max(PursuitBarWidth, RadioW + 28.f);
		const float RadioX = (Canvas->ClipX - RadioBoxW) * 0.5f;
		DrawRect(RadioBgColour, RadioX, ExtraY, RadioBoxW, RadioH + 10.f);
		DrawRect(NosColour, RadioX, ExtraY, 4.f, RadioH + 10.f);
		DrawCentred(RadioText, ExtraY + 4.f, LabelScale, NosActiveColour);
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

	if (Prog)
	{
		Prog->TickFeedback(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
		if (Prog->IsGarageOpen() && !IsMenuOpen())
		{
			OpenMenu(EBNMenuTab::Garage);
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
	const bool bPursuitVisible = Pursuit && Pursuit->GetState().Phase != EBNPursuitPhase::None;
	if (Pursuit)
	{
		DrawPursuit(*Pursuit, Story, bRaceActive);
	}

	const float BannerY = RaceTop + (bPursuitVisible ? (bRaceActive ? 168.f : 108.f) : 96.f);
	DrawActionBanners(*Car, Pursuit, BannerY);

	if (Prog)
	{
		DrawEconomyPill(*Prog);
	}

	DrawWorldMarkers(*Car, ActiveRace, Story);
	DrawRadar(*Car, ActiveRace, Pursuit, Story);
	const bool bMenuBlocking = HasStoryCardOpen() || (Prog && Prog->IsGarageOpen());
	DrawDashboard(*Car, MakeReadout(*Car), bMenuBlocking);
	DrawCameraModePill(*Car);
}
