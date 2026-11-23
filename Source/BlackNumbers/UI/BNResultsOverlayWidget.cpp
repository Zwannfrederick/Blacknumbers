#include "UI/BNResultsOverlayWidget.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Garage/BNGarageSubsystem.h"
#include "Police/BNPursuitLogic.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Story/BNStoryTypes.h"
#include "UI/BNHUD.h"
#include "UI/BNMenuWidget.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Text/STextBlock.h"

void SBNResultsOverlayWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;
	Mode = InArgs._Mode;
	RaceSummary = InArgs._RaceSummary;
	PursuitSummary = InArgs._PursuitSummary;

	const FSlateBrush* VigFull = BNUITheme::GetVignetteFullBrush();
	const bool bHasVig = VigFull != BNUITheme::WhiteBrush();

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.015f, 0.017f, 0.022f, 0.72f))
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(VigFull)
			.ColorAndOpacity(bHasVig ? FLinearColor(1.f, 1.f, 1.f, 0.95f) : FLinearColor(0.f, 0.f, 0.f, 0.f))
		]
		// Dramatic Most Wanted full-width horizontal victory / busted cinema ribbon
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			Mode == EBNResultsOverlayMode::RaceSummary ? BuildRaceSummary() : BuildPursuitSummary()
		]
	];
}

TSharedRef<SWidget> SBNResultsOverlayWidget::MakeMetricTile(const TCHAR* Label, const FString& Value, const FLinearColor& ValueColor, const FString& SubNote)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(STextBlock).Text(FText::FromString(Label)).Font(BNUITheme::Font(9, true)).ColorAndOpacity(BNUITheme::SteelMuted)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
	[
		SNew(STextBlock).Text(FText::FromString(Value)).Font(BNUITheme::Font(18, true)).ColorAndOpacity(ValueColor)
	];

	if (!SubNote.IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(SubNote)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)
		];
	}

	return SNew(SBorder)
		.BorderImage(BNUITheme::GetPanelBevelBrush())
		.BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.96f))
		.Padding(FMargin(16.f, 12.f))
		[
			Box
		];
}

TSharedRef<SWidget> SBNResultsOverlayWidget::BuildRaceSummary()
{
	const FBNRaceResultSummary& R = RaceSummary;
	FString BannerText;
	FLinearColor AccentCol = BNUITheme::AmberBright;
	bool bDangerRibbon = false;

	if (!R.bCompleted)
	{
		BannerText = TEXT("YARIŞ İPTAL EDİLDİ");
		AccentCol = BNUITheme::CrimsonRed;
		bDangerRibbon = true;
	}
	else if (R.bIsDrift)
	{
		if (R.BossRank > 0 && R.bBossVictory)
		{
			BannerText = TEXT("DRİFT ZAFERİ // BLACKLIST");
			AccentCol = BNUITheme::AmberBright;
		}
		else if (R.BossRank > 0)
		{
			BannerText = TEXT("DRİFT HEDEFİ TUTMADI");
			AccentCol = BNUITheme::CrimsonRed;
			bDangerRibbon = true;
		}
		else
		{
			BannerText = TEXT("DRİFT TAMAMLANDI");
			AccentCol = BNUITheme::NosCyan;
		}
	}
	else if (R.Position == 1)
	{
		BannerText = TEXT("1. SIRA  //  ZAFER");
		AccentCol = BNUITheme::AmberBright;
	}
	else
	{
		BannerText = FString::Printf(TEXT("%d. SIRA  //  BİTİŞ"), R.Position);
		AccentCol = BNUITheme::NosCyan;
	}

	const FString SubHeader = R.DisciplineLabel.IsEmpty()
		? R.RaceName.ToUpper()
		: FString::Printf(TEXT("%s   //   %s"), *R.RaceName.ToUpper(), *R.DisciplineLabel);

	const FString PrimaryMetricLabel = R.bIsDrift ? TEXT("DRİFT PUANI") : TEXT("BİTİŞ SÜRESİ");
	const FString PrimaryMetricVal = R.bIsDrift
		? FString::Printf(TEXT("%s"), *FString::FormatAsNumber(R.DriftScore))
		: (R.bCompleted ? ABNHUD::FormatRaceTime(R.FinishTimeSeconds) : FString(TEXT("--:--.--")));
	const FString PrimarySubNote = (R.bIsDrift && R.BossDriftTarget > 0)
		? FString::Printf(TEXT("Hedef: %s puan"), *FString::FormatAsNumber(R.BossDriftTarget))
		: FString();

	const FString SecondaryMetricLabel = R.bIsDrift ? TEXT("DURUM") : TEXT("SIRALAMA");
	const FString SecondaryMetricVal = R.bIsDrift
		? (R.bCompleted ? TEXT("TAMAMLANDI") : TEXT("İPTAL"))
		: FString::Printf(TEXT("%d / %d"), R.Position, FMath::Max(1, R.TotalRacers));

	TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(STextBlock).Text(FText::FromString(SubHeader)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::SteelMuted)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 14.f))
	[
		SNew(STextBlock).Text(FText::FromString(BannerText)).Font(BNUITheme::Font(34, true)).ColorAndOpacity(AccentCol)
	];

	if (R.BossRank > 0)
	{
		const FString BossBadgeText = R.bBossVictory
			? FString::Printf(TEXT("BLACK NUMBERS #%02d YENİLDİ  —  %s (%s)"), R.BossRank, *R.BossName, *R.BossVehicle)
			: FString::Printf(TEXT("BLACK NUMBERS #%02d KARŞISINDA KAYBETTİN  —  %s"), R.BossRank, *R.BossName);
		Inner->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(R.bBossVictory ? FLinearColor(0.09f, 0.24f, 0.14f, 0.96f) : BNUITheme::CrimsonDarkCard)
			.Padding(FMargin(16.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(BossBadgeText))
				.Font(BNUITheme::Font(12, true))
				.ColorAndOpacity(R.bBossVictory ? BNUITheme::EmeraldGreen : BNUITheme::ChalkWhite)
				.AutoWrapText(true)
			]
		];
	}

	Inner->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 18.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			MakeMetricTile(*PrimaryMetricLabel, PrimaryMetricVal, BNUITheme::ChalkWhite, PrimarySubNote)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			MakeMetricTile(*SecondaryMetricLabel, SecondaryMetricVal, AccentCol)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			MakeMetricTile(TEXT("NAKİT ÖDÜLÜ"),
				FString::Printf(TEXT("+$%s"), *FString::FormatAsNumber(R.CashEarned)),
				R.CashEarned > 0 ? BNUITheme::EmeraldGreen : BNUITheme::SteelMuted)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			MakeMetricTile(TEXT("İTİBAR"),
				FString::Printf(TEXT("+%s"), *FString::FormatAsNumber(R.RepEarned)),
				R.RepEarned > 0 ? BNUITheme::NosCyan : BNUITheme::SteelMuted)
		]
	];

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (R.RaceEvent.IsValid())
	{
		const FBNRaceResultSummary Captured = R;
		Buttons->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))
		[
			SNew(SBox).MinDesiredWidth(220.f)
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("TEKRAR YARIŞ")), EBNButtonVariant::PrimaryAmber,
					FOnClicked::CreateLambda([this, Captured]()
					{
						if (OwnerHUD.IsValid())
						{
							OwnerHUD->RestartRaceFromResults(Captured);
						}
						return FReply::Handled();
					}), true, 12, FMargin(20.f, 12.f))
			]
		];
	}
	Buttons->AddSlot().AutoWidth()
	[
		SNew(SBox).MinDesiredWidth(240.f)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("SERBEST SÜRÜŞE DÖN")), EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this]()
				{
					if (OwnerHUD.IsValid())
					{
						OwnerHUD->DismissResultsOverlay(true);
					}
					return FReply::Handled();
				}), true, 12, FMargin(20.f, 12.f))
		]
	];
	Inner->AddSlot().AutoHeight()[Buttons];

	auto MakeTrim = [bDangerRibbon]() -> TSharedRef<SWidget>
	{
		if (bDangerRibbon)
		{
			return SNew(SBox).HeightOverride(8.f)[SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::CrimsonRed)];
		}
		return BNUITheme::MakeHazardBar(9.f);
	};

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()[MakeTrim()]
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SBorder)
		.BorderImage(BNUITheme::GetBrushedMetalBrush())
		.BorderBackgroundColor(FLinearColor(0.85f, 0.85f, 0.85f, 0.96f))
		.Padding(FMargin(48.f, 28.f))
		[
			SNew(SBox).MaxDesiredWidth(840.f).HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(820.f)[Inner]
			]
		]
	]
	+ SVerticalBox::Slot().AutoHeight()[MakeTrim()];
}

TSharedRef<SWidget> SBNResultsOverlayWidget::BuildPursuitSummary()
{
	const FBNPursuitResultSummary& P = PursuitSummary;
	const FLinearColor AccentCol = P.bEscaped ? BNUITheme::AmberBright : BNUITheme::CrimsonRed;
	const FString BannerText = P.bEscaped ? TEXT("KAÇIŞ BAŞARILI  //  EVADED") : TEXT("YAKALANDIN  //  BUSTED");
	const FString SubLine = FString::Printf(TEXT("KARAMARA POLİS RAPORU   •   %s   •   SÜRE %s"),
		*P.HeatLabel, *ABNHUD::FormatRaceTime(P.DurationSeconds));
	const FString HeatDisplay = P.HeatLevel >= 11 ? TEXT("ÖZEL KADEME") : FString::Printf(TEXT("HEAT %d"), P.HeatLevel);
	const FString ThisPursuitNote = P.PursuitCostDollars > 0
		? FString::Printf(TEXT("Bu takip: +$%s"), *FString::FormatAsNumber(P.PursuitCostDollars))
		: FString();

	TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(STextBlock).Text(FText::FromString(SubLine)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::NosCyan)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 16.f))
	[
		SNew(STextBlock).Text(FText::FromString(BannerText)).Font(BNUITheme::Font(34, true)).ColorAndOpacity(AccentCol)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			MakeMetricTile(TEXT("ULAŞILAN HEAT"), HeatDisplay, P.HeatLevel >= 6 ? BNUITheme::CrimsonRed : BNUITheme::AmberGold)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			MakeMetricTile(TEXT("DEVRE DIŞI BİRİM"), FString::Printf(TEXT("%d"), P.UnitsDisabled), BNUITheme::ChalkWhite)
		]
		+ SHorizontalBox::Slot().FillWidth(1.25f)
		[
			MakeMetricTile(TEXT("ŞEHİR FATURASI"),
				FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(P.TotalCityDamageDollars)),
				BNUITheme::AmberBright, ThisPursuitNote)
		]
	];

	if (P.bEscaped && (P.RepEarned > 0 || P.Bounty > 0))
	{
		Inner->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.09f, 0.24f, 0.14f, 0.96f))
			.Padding(FMargin(16.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("KAÇIŞ ÖDÜLÜ:  +%s İtibar   •   Ödül Puanı: %s"),
					*FString::FormatAsNumber(P.RepEarned), *FString::FormatAsNumber(P.Bounty))))
				.Font(BNUITheme::Font(11, true))
				.ColorAndOpacity(BNUITheme::EmeraldGreen)
			]
		];
	}

	Inner->AddSlot().AutoHeight().HAlign(HAlign_Left)
	[
		SNew(SBox).MinDesiredWidth(240.f)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("DEVAM ET [ESC]")),
				P.bEscaped ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::DangerCrimson,
				FOnClicked::CreateLambda([this]()
				{
					if (OwnerHUD.IsValid())
					{
						OwnerHUD->DismissResultsOverlay(true);
					}
					return FReply::Handled();
				}), true, 12, FMargin(20.f, 12.f))
		]
	];

	auto MakeTrim = [&P]() -> TSharedRef<SWidget>
	{
		if (!P.bEscaped)
		{
			return SNew(SBox).HeightOverride(8.f)[SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::CrimsonRed)];
		}
		return BNUITheme::MakeHazardBar(9.f);
	};

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()[MakeTrim()]
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SBorder)
		.BorderImage(BNUITheme::GetBrushedMetalBrush())
		.BorderBackgroundColor(FLinearColor(0.85f, 0.85f, 0.85f, 0.96f))
		.Padding(FMargin(48.f, 28.f))
		[
			SNew(SBox).MaxDesiredWidth(820.f).HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(760.f)[Inner]
			]
		]
	]
	+ SVerticalBox::Slot().AutoHeight()[MakeTrim()];
}

namespace
{
	FString DisciplineLabelFor(const UBNRaceDefinition* Def)
	{
		if (!Def)
		{
			return FString();
		}
		switch (Def->Discipline)
		{
		case EBNRaceDiscipline::Drift: return TEXT("DRİFT");
		case EBNRaceDiscipline::Circuit: return TEXT("PİST TURU");
		default: return TEXT("SPRINT");
		}
	}
}

void ABNHUD::ShowRaceResults(const FBNRaceResultSummary& Summary)
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	if (ResultsHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ResultsHost.ToSharedRef());
		ResultsHost.Reset();
		ResultsOverlayWidget.Reset();
	}

	SAssignNew(ResultsOverlayWidget, SBNResultsOverlayWidget)
		.OwnerHUD(this)
		.Mode(EBNResultsOverlayMode::RaceSummary)
		.RaceSummary(Summary);
	ResultsHost = SNew(SWeakWidget).PossiblyNullContent(ResultsOverlayWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(ResultsHost.ToSharedRef(), 35);

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetInputLocked(true);
	}
	ApplyCursorMode(true);
	SyncStoryCardOverlay();
}

void ABNHUD::ShowPursuitResults(const FBNPursuitResultSummary& Summary)
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	if (ResultsHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ResultsHost.ToSharedRef());
		ResultsHost.Reset();
		ResultsOverlayWidget.Reset();
	}

	SAssignNew(ResultsOverlayWidget, SBNResultsOverlayWidget)
		.OwnerHUD(this)
		.Mode(EBNResultsOverlayMode::PursuitSummary)
		.PursuitSummary(Summary);
	ResultsHost = SNew(SWeakWidget).PossiblyNullContent(ResultsOverlayWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(ResultsHost.ToSharedRef(), 35);

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
	{
		Car->SetInputLocked(true);
	}
	ApplyCursorMode(true);
	SyncStoryCardOverlay();
}

void ABNHUD::DismissResultsOverlay(bool bRestoreControl)
{
	if (GEngine && GEngine->GameViewport && ResultsHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ResultsHost.ToSharedRef());
	}
	ResultsHost.Reset();
	ResultsOverlayWidget.Reset();

	if (!bRestoreControl)
	{
		return;
	}

	SyncStoryCardOverlay();
	if (!HasStoryCardOpen() && !IsMenuOpen())
	{
		if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn()))
		{
			Car->SetInputLocked(false);
		}
		ApplyCursorMode(false);
	}
}

void ABNHUD::RestartRaceFromResults(const FBNRaceResultSummary& Summary)
{
	DismissResultsOverlay(false);
	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	ABNRaceEvent* Race = Summary.RaceEvent.Get();
	if (!Car || !Race)
	{
		DismissResultsOverlay(true);
		return;
	}
	ApplyCursorMode(false);
	TrackedRace = FTrackedRaceState();

	if (Summary.BossRank > 0)
	{
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			if (Story->GetState().CanChallengeBoss(Summary.BossRank))
			{
				Story->OpenDossier(Car);
				Story->MoveDossierSelection((16 - Summary.BossRank) - Story->GetSelectedBossIndex());
				if (Story->ChallengeSelectedBoss(Car))
				{
					return;
				}
			}
		}
	}
	Race->StartRace(Car);
}

void ABNHUD::UpdateRaceAndPursuitTracking(const UBNProgressionSubsystem* Prog, const UBNStorySubsystem* Story, const UBNPursuitSubsystem* Pursuit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ABNRaceEvent* RunningOrFinished = nullptr;
	for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
	{
		if (It->GetState() != EBNRaceState::Idle)
		{
			RunningOrFinished = *It;
			break;
		}
	}

	if (!RunningOrFinished)
	{
		TrackedRace = FTrackedRaceState();
	}
	else if (RunningOrFinished->GetState() == EBNRaceState::Countdown || RunningOrFinished->GetState() == EBNRaceState::Racing)
	{
		if (TrackedRace.Event.Get() != RunningOrFinished || TrackedRace.bResultsShown)
		{
			TrackedRace = FTrackedRaceState();
			TrackedRace.Event = RunningOrFinished;
			TrackedRace.RaceName = RunningOrFinished->Definition ? RunningOrFinished->Definition->DisplayName.ToString() : TEXT("YARIŞ");
			TrackedRace.DisciplineLabel = DisciplineLabelFor(RunningOrFinished->Definition);
			TrackedRace.bIsDrift = RunningOrFinished->Definition && RunningOrFinished->Definition->Discipline == EBNRaceDiscipline::Drift;
			TrackedRace.BossRank = RunningOrFinished->GetActiveBossRank();
			TrackedRace.BossDriftTarget = RunningOrFinished->GetActiveBossDriftTarget();
			TrackedRace.PreRaceCash = Prog ? Prog->GetProfile().Cash : 0;
			TrackedRace.PreRaceRep = Prog ? Prog->GetProfile().Reputation : 0;
		}
		TrackedRace.TotalRacers = FMath::Max(TrackedRace.TotalRacers, RunningOrFinished->GetNumRivals() + 1);
		if (RunningOrFinished->GetState() == EBNRaceState::Racing)
		{
			TrackedRace.bWasRacing = true;
			TrackedRace.LastTimeLeft = RunningOrFinished->GetTimeLeft();
		}
	}
	else if (RunningOrFinished->GetState() == EBNRaceState::Finished
		&& TrackedRace.Event.Get() == RunningOrFinished && !TrackedRace.bResultsShown)
	{
		TrackedRace.bResultsShown = true;
		const int32 CurCash = Prog ? Prog->GetProfile().Cash : TrackedRace.PreRaceCash;
		const int32 CurRep = Prog ? Prog->GetProfile().Reputation : TrackedRace.PreRaceRep;

		FBNRaceResultSummary Sum;
		Sum.RaceEvent = RunningOrFinished;
		Sum.RaceName = !TrackedRace.RaceName.IsEmpty()
			? TrackedRace.RaceName
			: (RunningOrFinished->Definition ? RunningOrFinished->Definition->DisplayName.ToString() : TEXT("YARIŞ"));
		Sum.DisciplineLabel = !TrackedRace.DisciplineLabel.IsEmpty()
			? TrackedRace.DisciplineLabel
			: DisciplineLabelFor(RunningOrFinished->Definition);
		Sum.bIsDrift = TrackedRace.bIsDrift || (RunningOrFinished->Definition && RunningOrFinished->Definition->Discipline == EBNRaceDiscipline::Drift);
		Sum.Position = RunningOrFinished->GetPlayerPosition();
		Sum.TotalRacers = FMath::Max(TrackedRace.TotalRacers, RunningOrFinished->GetNumRivals() + 1);
		Sum.FinishTimeSeconds = static_cast<float>(RunningOrFinished->GetProgress().FinishTime());
		Sum.DriftScore = RunningOrFinished->GetDriftScore().Total();
		Sum.BossDriftTarget = TrackedRace.BossDriftTarget;
		Sum.CashEarned = FMath::Max(0, CurCash - TrackedRace.PreRaceCash);
		Sum.RepEarned = FMath::Max(0, CurRep - TrackedRace.PreRaceRep);
		Sum.bCompleted = Sum.bIsDrift
			? (TrackedRace.bWasRacing && (TrackedRace.LastTimeLeft <= 0.5f || Sum.CashEarned > 0 || Sum.RepEarned > 0))
			: RunningOrFinished->GetProgress().IsFinished();
		Sum.BossRank = TrackedRace.BossRank;
		Sum.bBossVictory = Sum.BossRank > 0 && Sum.bCompleted
			&& (Sum.bIsDrift ? Sum.DriftScore >= Sum.BossDriftTarget : Sum.Position == 1);
		if (const FBNBossSpec* Boss = Sum.BossRank > 0 ? FBNStoryState::FindBossByRank(Sum.BossRank) : nullptr)
		{
			Sum.BossName = Boss->Name;
			Sum.BossVehicle = Boss->VehicleName;
		}
		ShowRaceResults(Sum);
	}

	const bool bAnyRaceNonIdle = RunningOrFinished != nullptr;
	if (Pursuit)
	{
		const FBNPursuitState& PS = Pursuit->GetState();
		if (PS.IsPursuitRunning())
		{
			if (!bTrackedPursuitActive)
			{
				bTrackedPursuitActive = true;
				TrackedPursuitPreRep = Prog ? Prog->GetProfile().Reputation : 0;
			}
		}
		else if (bTrackedPursuitActive && (PS.Phase == EBNPursuitPhase::Busted || PS.Phase == EBNPursuitPhase::Escaped))
		{
			bTrackedPursuitActive = false;
			if (!bAnyRaceNonIdle && !IsMenuOpen() && (!Story || !Story->GetState().bCampaignCompleted))
			{
				FBNPursuitResultSummary PSum;
				PSum.bEscaped = PS.Phase == EBNPursuitPhase::Escaped;
				PSum.HeatLevel = PS.HeatLevel;
				PSum.HeatLabel = FBNHeatTierSpec::ForLevel(PS.HeatLevel).Label;
				PSum.UnitsDisabled = PS.UnitsDisabled;
				PSum.BreakersTriggered = PS.BreakersTriggered;
				PSum.PursuitCostDollars = PS.PursuitCostDollars;
				PSum.TotalCityDamageDollars = PS.CostToStateDollars;
				PSum.Bounty = PS.Bounty;
				PSum.DurationSeconds = PS.PursuitTimeSeconds;
				PSum.RepEarned = (PSum.bEscaped && Prog) ? FMath::Max(0, Prog->GetProfile().Reputation - TrackedPursuitPreRep) : 0;
				ShowPursuitResults(PSum);
			}
		}
		else if (PS.Phase == EBNPursuitPhase::None)
		{
			bTrackedPursuitActive = false;
		}
	}
}
