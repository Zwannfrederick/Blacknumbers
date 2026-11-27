#include "UI/BNMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Garage/BNGarageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Story/BNStoryTypes.h"
#include "UI/BNHUD.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNDayNightCycle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

const FLinearColor SBNGameMenuWidget::BackdropColor(0.015f, 0.017f, 0.022f, 0.62f);
const FLinearColor SBNGameMenuWidget::PanelBgColor = BNUITheme::Panel;
const FLinearColor SBNGameMenuWidget::CardBgColor = BNUITheme::Card;
const FLinearColor SBNGameMenuWidget::CardActiveBgColor = BNUITheme::AccentDark;
const FLinearColor SBNGameMenuWidget::AccentAmberColor = BNUITheme::Accent;
const FLinearColor SBNGameMenuWidget::AccentCyanColor = BNUITheme::Nos;
const FLinearColor SBNGameMenuWidget::AccentGreenColor = BNUITheme::Ok;
const FLinearColor SBNGameMenuWidget::MutedTextColor = BNUITheme::Muted;
const FLinearColor SBNGameMenuWidget::WhiteTextColor = BNUITheme::Bone;

const FSlateBrush* SBNGameMenuWidget::WhiteBrush()
{
	return BNUITheme::WhiteBrush();
}

FSlateFontInfo SBNGameMenuWidget::MenuFont(int32 Size, bool bBold)
{
	return BNUITheme::Font(Size, bBold);
}

ABNVehiclePawn* SBNGameMenuWidget::GetPlayerCar(const TWeakObjectPtr<ABNHUD>& HUD)
{
	return HUD.IsValid() ? Cast<ABNVehiclePawn>(HUD->GetOwningPawn()) : nullptr;
}

UWorld* SBNGameMenuWidget::GetHUDWorld(const TWeakObjectPtr<ABNHUD>& HUD)
{
	return HUD.IsValid() ? HUD->GetWorld() : nullptr;
}

UGameInstance* SBNGameMenuWidget::GetGI(const TWeakObjectPtr<ABNHUD>& HUD)
{
	UWorld* W = GetHUDWorld(HUD);
	return W ? W->GetGameInstance() : nullptr;
}

void SBNGameMenuWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;
	ActiveTab = InArgs._InitialTab;

	ChildSlot
	[
		SAssignNew(RootSlot, SVerticalBox)
	];

	Refresh();
}

FReply SBNGameMenuWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && OwnerHUD.IsValid())
	{
		OwnerHUD->HandleMenuCloseOrAdvanceCard();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SBNGameMenuWidget::SetActiveTab(EBNMenuTab NewTab)
{
	if (ActiveTab == EBNMenuTab::Garage && NewTab != EBNMenuTab::Garage && PreviewedCar.IsSet())
	{
		if (ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD))
		{
			if (const UBNProgressionSubsystem* Prog = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
			{
				Prog->ApplyProfileToCar(*Car);
			}
		}
	}
	ActiveTab = NewTab;
	PreviewedCar.Reset();
	ActionFeedback.Reset();
	Refresh();
}

void SBNGameMenuWidget::Refresh()
{
	if (!RootSlot.IsValid())
	{
		return;
	}
	RootSlot->ClearChildren();

	ABNRaceEvent* ActiveRace = ABNRaceEvent::FindActiveRace(GetHUDWorld(OwnerHUD));
	if (ActiveRace && (ActiveTab == EBNMenuTab::Garage || ActiveTab == EBNMenuTab::BlackNumbers))
	{
		ActiveTab = EBNMenuTab::Races;
	}

	TSharedRef<SWidget> TabBody = SNullWidget::NullWidget;
	switch (ActiveTab)
	{
	case EBNMenuTab::Garage:
		TabBody = BuildGarageTab();
		break;
	case EBNMenuTab::BlackNumbers:
		TabBody = BuildBlackNumbersTab();
		break;
	case EBNMenuTab::Settings:
		TabBody = BuildSettingsTab();
		break;
	case EBNMenuTab::Controls:
		TabBody = BuildControlsTab();
		break;
	default:
		TabBody = ActiveRace ? BuildRacePauseView(*ActiveRace) : BuildRacesTab();
		break;
	}

	const bool bTransparentSafehouse = (!ActiveRace && ActiveTab == EBNMenuTab::Garage);
	const bool bFullBleedBlacklist = (!ActiveRace && ActiveTab == EBNMenuTab::BlackNumbers);
	const FSlateBrush* VigFull = BNUITheme::GetVignetteFullBrush();
	const bool bHasVig = VigFull != BNUITheme::WhiteBrush();

	if (bFullBleedBlacklist)
	{
		// Full-bleed Blacklist: flat ink over the live world, nothing decorative competing with the rival.
		RootSlot->AddSlot().FillHeight(1.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Ink)
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				// Ultra-minimal top strip so user can still switch Garage / Races / Settings / Resume without cluttering the Blacklist art!
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(36.f, 14.f, 36.f, 4.f))
				[
					BuildHeaderBar()
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(36.f, 6.f, 36.f, 20.f))
				[
					TabBody
				]
			]
		];
		return;
	}

	if (bTransparentSafehouse)
	{
		// Live 3D Safehouse Garage: leave center wide open for the rotating 3D showcase car!
		RootSlot->AddSlot().FillHeight(1.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(VigFull)
				.ColorAndOpacity(bHasVig ? FLinearColor(1.f, 1.f, 1.f, 0.85f) : FLinearColor(0.f, 0.f, 0.f, 0.f))
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(BNUITheme::Panel)
					.Padding(FMargin(32.f, 14.f, 32.f, 10.f))
					[
						BuildHeaderBar()
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(24.f, 16.f, 24.f, 20.f))
				[
					TabBody
				]
			]
		];
		return;
	}

	// Races / Settings / Controls / Pause: flat ink shell over the live world.
	RootSlot->AddSlot().FillHeight(1.f)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Ink)
		]
		+ SOverlay::Slot().Padding(FMargin(44.f, 22.f, 44.f, 26.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildHeaderBar()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 14.f))
			[
				BNUITheme::MakeAccentRule(2.f)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				TabBody
			]
		]
	];
}

TSharedRef<SWidget> SBNGameMenuWidget::MakeStyledButton(const FText& Label, FLinearColor BgColor, FOnClicked OnClicked, bool bEnabled)
{
	EBNButtonVariant Variant = EBNButtonVariant::SecondarySteel;
	if (BgColor.R > 0.72f && BgColor.G > 0.38f && BgColor.B < 0.25f)
	{
		Variant = EBNButtonVariant::PrimaryAmber;
	}
	else if (BgColor.R > 0.45f && BgColor.G < 0.25f)
	{
		Variant = EBNButtonVariant::DangerCrimson;
	}
	else if (BgColor.G > 0.42f && BgColor.R < 0.28f && BgColor.B < 0.40f)
	{
		Variant = EBNButtonVariant::ConfirmGreen;
	}
	else if (BgColor.B > 0.60f && BgColor.R < 0.25f)
	{
		Variant = EBNButtonVariant::CyanIntel;
	}
	return BNUITheme::MakeMWButton(Label, Variant, OnClicked, bEnabled);
}

TSharedRef<SWidget> SBNGameMenuWidget::MakeActionButton(const FText& Label, FLinearColor BgColor, FOnClicked OnClicked, bool bEnabled)
{
	return MakeStyledButton(Label, BgColor, OnClicked, bEnabled);
}

TSharedRef<SWidget> SBNGameMenuWidget::MakeStatBar(const FString& Label, const FString& ValueText, float Fraction, FLinearColor FillColor)
{
	return BNUITheme::MakeSegmentedStatBar(Label, ValueText, Fraction, FillColor);
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildHeaderBar()
{
	UGameInstance* GI = GetGI(OwnerHUD);
	const UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const int32 Cash = Prog ? Prog->GetProfile().Cash : 0;
	const int32 Rep = Prog ? Prog->GetProfile().Reputation : 0;
	const int32 Defeated = Story ? Story->GetState().GetDefeatedCount() : 0;
	const FString ActStr = Story ? FBNStoryState::ActLabel(Story->GetState().Act) : TEXT("KARAMARA");

	auto TabBtn = [this](EBNMenuTab Tab, const TCHAR* Label) -> TSharedRef<SWidget>
	{
		const bool bCurrent = ActiveTab == Tab;
		return BNUITheme::MakeMWButton(
			FText::FromString(Label),
			bCurrent ? EBNButtonVariant::ActiveTab : EBNButtonVariant::IdleTab,
			FOnClicked::CreateLambda([this, Tab]()
			{
				SetActiveTab(Tab);
				return FReply::Handled();
			}),
			true, 13, FMargin(18.f, 10.f));
	};

	const ABNRaceEvent* ActiveRace = ABNRaceEvent::FindActiveRace(GetHUDWorld(OwnerHUD));
	if (ActiveRace)
	{
		const FString RaceTitle = ActiveRace->Definition
			? ActiveRace->Definition->DisplayName.ToString().ToUpper() : TEXT("AKTİF YARIŞ");
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("YARIŞ DURAKLATILDI")))
					.Font(BNUITheme::DisplayFont(34))
					.ColorAndOpacity(BNUITheme::Bone)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("BLACK NUMBERS  //  %s"), *RaceTitle)))
					.Font(BNUITheme::Font(11, true))
					.ColorAndOpacity(BNUITheme::Accent)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("DEVAM ET [ESC]")), EBNButtonVariant::PrimaryAmber,
					FOnClicked::CreateLambda([this]()
					{
						if (OwnerHUD.IsValid())
						{
							OwnerHUD->CloseMenu();
						}
						return FReply::Handled();
					}), true, 13, FMargin(18.f, 10.f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::Races, TEXT("DURAKLAT"))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::Settings, TEXT("AYARLAR"))]
			+ SHorizontalBox::Slot().AutoWidth()[TabBtn(EBNMenuTab::Controls, TEXT("KONTROLLER"))]
		];
	}

	const int32 TotalBounty = Story ? Story->GetState().TotalBounty : 0;

	if (ActiveTab == EBNMenuTab::BlackNumbers)
	{
		// Ultra-clean single-row navigation strip so full-bleed MW Blacklist art dominates 100% of the canvas
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))[TabBtn(EBNMenuTab::BlackNumbers, TEXT("KARA LİSTE"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))[TabBtn(EBNMenuTab::Races, TEXT("YARIŞLAR"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))[TabBtn(EBNMenuTab::Garage, TEXT("GARAJ"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))[TabBtn(EBNMenuTab::Settings, TEXT("AYARLAR"))]
		+ SHorizontalBox::Slot().AutoWidth()[TabBtn(EBNMenuTab::Controls, TEXT("KONTROLLER"))]
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("ANA EKRAN")), EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this]() { if (OwnerHUD.IsValid()) { OwnerHUD->OpenTitleScreen(); } return FReply::Handled(); }),
				true, 11, FMargin(12.f, 7.f))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("SÜRÜŞE DÖN [ESC]")), EBNButtonVariant::PrimaryAmber,
				FOnClicked::CreateLambda([this]() { if (OwnerHUD.IsValid()) { OwnerHUD->CloseMenu(); } return FReply::Handled(); }),
				true, 11, FMargin(14.f, 7.f))
		];
	}

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 20.f, 0.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ActiveTab == EBNMenuTab::Garage ? TEXT("SELİN'İN GARAJI")
						: ActiveTab == EBNMenuTab::Settings ? TEXT("AYARLAR")
						: ActiveTab == EBNMenuTab::Controls ? TEXT("KONTROLLER")
						: TEXT("SOKAK YARIŞLARI")))
					.Font(BNUITheme::DisplayFont(34))
					.ColorAndOpacity(BNUITheme::Bone)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("KARAMARA  //  %s  •  KARA LİSTE %d/16"), *ActStr, Defeated)))
					.Font(BNUITheme::Font(11, true))
					.ColorAndOpacity(BNUITheme::Accent)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				BNUITheme::MakePanel(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 14.f, 0.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(Cash))))
						.Font(BNUITheme::Font(14))
						.ColorAndOpacity(BNUITheme::Bone)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 14.f, 0.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("ÖDÜL %s"), *FString::FormatAsNumber(TotalBounty))))
						.Font(BNUITheme::Font(13))
						.ColorAndOpacity(BNUITheme::Accent)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s İTİBAR"), *FString::FormatAsNumber(Rep))))
						.Font(BNUITheme::Font(12, true))
						.ColorAndOpacity(BNUITheme::Nos)
					],
					FMargin(14.f, 6.f), FLinearColor::Transparent)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("ANA EKRAN")), EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this]()
				{
					if (OwnerHUD.IsValid())
					{
						OwnerHUD->OpenTitleScreen();
					}
					return FReply::Handled();
				}), true, 12, FMargin(16.f, 10.f))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("SÜRÜŞE DÖN [ESC]")), EBNButtonVariant::PrimaryAmber,
				FOnClicked::CreateLambda([this]()
				{
					if (OwnerHUD.IsValid())
					{
						OwnerHUD->CloseMenu();
					}
					return FReply::Handled();
				}), true, 12, FMargin(16.f, 10.f))
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::Races, TEXT("YARIŞLAR"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::Garage, TEXT("SELİN'İN GARAJI"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::BlackNumbers, TEXT("BLACK NUMBERS LİSTESİ"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[TabBtn(EBNMenuTab::Settings, TEXT("AYARLAR"))]
		+ SHorizontalBox::Slot().AutoWidth()[TabBtn(EBNMenuTab::Controls, TEXT("KONTROLLER"))]
	];
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildRacePauseView(ABNRaceEvent& ActiveRace)
{
	const UBNRaceDefinition* Def = ActiveRace.Definition;
	const FString RaceTitle = Def ? Def->DisplayName.ToString() : TEXT("Karamara Yarışı");
	const bool bDrift = Def && Def->Discipline == EBNRaceDiscipline::Drift;
	const FString StatusLine = bDrift
		? FString::Printf(TEXT("DRİFT SKORU: %d PTS   •   KALAN SÜRE: %s"),
			ActiveRace.GetDriftScore().Total(), *ABNHUD::FormatRaceTime(ActiveRace.GetTimeLeft()))
		: FString::Printf(TEXT("SIRA: %d/%d   •   KAPI: %d/%d   •   SÜRE: %s"),
			ActiveRace.GetPlayerPosition(), ActiveRace.GetNumRivals() + 1,
			FMath::Min(ActiveRace.GetProgress().NextCheckpoint() + 1, Def ? FMath::Max(1, Def->Checkpoints.Num()) : 1),
			Def ? FMath::Max(1, Def->Checkpoints.Num()) : 1,
			*ABNHUD::FormatRaceTime(ActiveRace.GetRaceTime()));

	const TWeakObjectPtr<ABNRaceEvent> WeakRace = &ActiveRace;

	auto PauseBtn = [](const TCHAR* Title, const TCHAR* Sub, bool bPrimary, bool bDanger, FOnClicked OnClick) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.HeightOverride(58.f)
			[
				BNUITheme::MakeNavBladeButton(FText::FromString(Title), FText::FromString(Sub), bPrimary, OnClick, bDanger)
			];
	};

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(520.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BNUITheme::MakeAccentRule(5.f)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(BNUITheme::WhiteBrush())
				.BorderBackgroundColor(BNUITheme::Panel)
				.Padding(FMargin(24.f, 20.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(RaceTitle.ToUpper()))
						.Font(BNUITheme::DisplayFont(28))
						.ColorAndOpacity(BNUITheme::Bone)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 18.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(StatusLine))
						.Font(BNUITheme::Font(13, true))
						.ColorAndOpacity(BNUITheme::Nos)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
					[
						PauseBtn(TEXT("YARIŞA DEVAM ET"), TEXT(""), true, false,
							FOnClicked::CreateLambda([this]()
							{
								if (OwnerHUD.IsValid())
								{
									OwnerHUD->CloseMenu();
								}
								return FReply::Handled();
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
					[
						PauseBtn(TEXT("YENİDEN BAŞLAT"), TEXT(""), false, false,
							FOnClicked::CreateLambda([this, WeakRace]()
							{
								if (OwnerHUD.IsValid())
								{
									OwnerHUD->CloseMenu();
								}
								if (WeakRace.IsValid())
								{
									WeakRace->RestartActiveRace();
								}
								return FReply::Handled();
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
					[
						PauseBtn(TEXT("YARIŞTAN ÇIK"), TEXT(""), false, true,
							FOnClicked::CreateLambda([this, WeakRace]()
							{
								if (OwnerHUD.IsValid())
								{
									OwnerHUD->CloseMenu();
								}
								if (WeakRace.IsValid())
								{
									WeakRace->EndRace();
								}
								return FReply::Handled();
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
						[
							BNUITheme::MakeMWButton(FText::FromString(TEXT("AYARLAR")), EBNButtonVariant::SecondarySteel,
								FOnClicked::CreateLambda([this]() { SetActiveTab(EBNMenuTab::Settings); return FReply::Handled(); }))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
						[
							BNUITheme::MakeMWButton(FText::FromString(TEXT("KONTROLLER")), EBNButtonVariant::SecondarySteel,
								FOnClicked::CreateLambda([this]() { SetActiveTab(EBNMenuTab::Controls); return FReply::Handled(); }))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							BNUITheme::MakeMWButton(FText::FromString(TEXT("ANA EKRAN")), EBNButtonVariant::SecondarySteel,
								FOnClicked::CreateLambda([this]()
								{
									if (OwnerHUD.IsValid())
									{
										OwnerHUD->OpenTitleScreen();
									}
									return FReply::Handled();
								}))
						]
					]
				]
			]
		]
	]
	+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)];
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildRacesTab()
{
	UWorld* World = GetHUDWorld(OwnerHUD);
	UGameInstance* GI = GetGI(OwnerHUD);
	UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;

	TSharedRef<SVerticalBox> RaceList = SNew(SVerticalBox);

	if (Story)
	{
		const FBNStoryState& S = Story->GetState();
		if (S.bFinalChaseActive)
		{
			RaceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(BNUITheme::DangerDark)
				.Padding(FMargin(16.f, 14.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("ÖZEL KADEME KOVALAMACA AKTİF: Polisleri atlat ve Liman Tünelleri (Rota A) veya Kuzey Geçidi (Rota B) çıkışına ulaş!")))
					.Font(BNUITheme::Font(12, true))
					.ColorAndOpacity(BNUITheme::Bone)
					.AutoWrapText(true)
				]
			];
		}
		else if (const FBNBossSpec* NextBoss = FBNStoryState::FindBossByRank(S.NextBossRank))
		{
			RaceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeAccentRule(5.f)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(BNUITheme::WhiteBrush())
					.BorderBackgroundColor(BNUITheme::AccentDark)
					.Padding(FMargin(18.f, 14.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 18.f, 0.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("#%02d"), NextBoss->Rank)))
							.Font(BNUITheme::DisplayFont(42))
							.ColorAndOpacity(BNUITheme::Accent)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("SIRADAKİ RAKİP  //  %s"), *NextBoss->Name.ToUpper())))
								.Font(BNUITheme::DisplayFont(22))
								.ColorAndOpacity(BNUITheme::Bone)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("%s   •   %d/%d ELEME   •   %s"),
									*NextBoss->VehicleName, S.QualifiersWon, NextBoss->Qualifiers.Num(), *NextBoss->DisciplineLabel)))
								.Font(BNUITheme::Font(12, false))
								.ColorAndOpacity(BNUITheme::Muted)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							BNUITheme::MakeMWButton(FText::FromString(TEXT("PROGRAMI AÇ")), EBNButtonVariant::PrimaryAmber,
								FOnClicked::CreateLambda([this]()
								{
									if (OwnerHUD.IsValid())
									{
										OwnerHUD->OpenMenu(EBNMenuTab::BlackNumbers);
									}
									return FReply::Handled();
								}), true, 12, FMargin(18.f, 11.f))
						]
					]
				]
			];
		}
	}

	RaceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("KARAMARA YARIŞ ETKİNLİKLERİ"), TEXT("ANINDA BAŞLAT"))
	];

	if (World)
	{
		for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
		{
			ABNRaceEvent* Race = *It;
			const UBNRaceDefinition* Def = Race ? Race->Definition : nullptr;
			if (!Def)
			{
				continue;
			}
			const FString Tag = Def->Discipline == EBNRaceDiscipline::Drift ? TEXT("DRİFT")
				: Def->Discipline == EBNRaceDiscipline::Drag ? TEXT("DRAG")
				: Def->Discipline == EBNRaceDiscipline::Circuit ? TEXT("PİST TURU") : TEXT("SPRINT");
			const FLinearColor TagCol = Def->Discipline == EBNRaceDiscipline::Drift ? BNUITheme::Nos
				: Def->Discipline == EBNRaceDiscipline::Drag ? BNUITheme::Danger
				: Def->Discipline == EBNRaceDiscipline::Circuit ? BNUITheme::Accent : BNUITheme::Ok;
			const FString Meta = Def->Discipline == EBNRaceDiscipline::Drift
				? FString::Printf(TEXT("Süre: %.0f sn  •  Serbest Drift Puanı"), Def->TimeLimitSeconds)
				: Def->Discipline == EBNRaceDiscipline::Drag
					? TEXT("Manuel Vites ([E]/[Q])  •  1/2 Mil Düzlük  •  Motor Patlama Riski")
				: Def->Discipline == EBNRaceDiscipline::Circuit
					? FString::Printf(TEXT("%d Tur  •  %d Rakip  •  GPS Rota Aktif"), Def->Laps, Def->RivalSkills.Num())
					: FString::Printf(TEXT("%d Rakip  •  Noktadan Noktaya GPS Rota"), Def->RivalSkills.Num());
			const TWeakObjectPtr<ABNRaceEvent> WeakRace = Race;

			RaceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 6.f, 8.f))
			[
				SNew(SBorder)
				.BorderImage(BNUITheme::WhiteBrush())
				.BorderBackgroundColor(BNUITheme::Panel)
				.Padding(FMargin(0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(5.f)[SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(TagCol)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(14.f, 11.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
								[
									SNew(STextBlock).Text(FText::FromString(Tag)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(TagCol)
								]
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(Def->DisplayName.ToString())).Font(BNUITheme::Font(15, true)).ColorAndOpacity(BNUITheme::Bone)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
							[
								SNew(STextBlock).Text(FText::FromString(Meta)).Font(BNUITheme::Font(11, false)).ColorAndOpacity(BNUITheme::Muted)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							BNUITheme::MakeMWButton(
								FText::FromString(TEXT("YARIŞA BAŞLA")),
								EBNButtonVariant::CyanIntel,
								FOnClicked::CreateLambda([this, WeakRace]()
								{
									ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD);
									if (WeakRace.IsValid() && Car && OwnerHUD.IsValid())
									{
										OwnerHUD->CloseMenu();
										WeakRace->StartRace(Car);
									}
									return FReply::Handled();
								}))
						]
					]
				]
			];
		}
	}

	const bool bPursuitActive = Pursuit && Pursuit->GetState().IsPursuitRunning();
	TSharedRef<SVerticalBox> WorldTools = SNew(SVerticalBox);
	WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("ŞEHİR & ATMOSFER"), TEXT("SERBEST SÜRÜŞ"), BNUITheme::Nos)
	];

	auto TimeBtn = [this](const TCHAR* Label, float Hour)
	{
		return BNUITheme::MakeMWButton(FText::FromString(Label), EBNButtonVariant::SecondarySteel,
			FOnClicked::CreateLambda([this, Hour]()
			{
				if (UWorld* W = GetHUDWorld(OwnerHUD))
				{
					for (TActorIterator<ABNDayNightCycle> It(W); It; ++It)
					{
						It->SetTimeOfDay(Hour);
					}
					ActionFeedback = FString::Printf(TEXT("Karamara saati %.1f olarak ayarlandı."), Hour);
					Refresh();
				}
				return FReply::Handled();
			}), true, 10, FMargin(8.f, 8.f));
	};

	WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 5.f, 0.f))[TimeBtn(TEXT("GÜNDÜZ 13:00"), 13.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 5.f, 0.f))[TimeBtn(TEXT("ALTIN SAAT 17:30"), 17.5f)]
		+ SHorizontalBox::Slot().FillWidth(1.f)[TimeBtn(TEXT("GECE 22:30"), 22.5f)]
	];

	auto WeatherBtn = [this](const TCHAR* Label, EBNWeatherMode Mode)
	{
		return BNUITheme::MakeMWButton(FText::FromString(Label), EBNButtonVariant::SecondarySteel,
			FOnClicked::CreateLambda([this, Mode]()
			{
				if (UWorld* W = GetHUDWorld(OwnerHUD))
				{
					for (TActorIterator<ABNDayNightCycle> It(W); It; ++It)
					{
						It->SetWeatherMode(Mode);
					}
					ActionFeedback = FString::Printf(TEXT("Hava & zemin modu: %s"), ABNDayNightCycle::WeatherModeLabel(Mode));
					Refresh();
				}
				return FReply::Handled();
			}), true, 9, FMargin(6.f, 7.f));
	};

	WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 5.f, 0.f))[WeatherBtn(TEXT("BÖLGESEL OTO"), EBNWeatherMode::AutoDistrict)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 5.f, 0.f))[WeatherBtn(TEXT("ISLAK ASFALT"), EBNWeatherMode::WetDamp)]
		+ SHorizontalBox::Slot().FillWidth(1.f)[WeatherBtn(TEXT("HAFİF YAĞMUR"), EBNWeatherMode::LightRain)]
	];

	WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 8.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
		[
			BNUITheme::MakeMWButton(
				FText::FromString(bPursuitActive ? TEXT("TAKİBİ BİTİR") : TEXT("POLİS TAKİBİ [P]")),
				EBNButtonVariant::DangerCrimson,
				FOnClicked::CreateLambda([this]()
				{
					if (UWorld* W = GetHUDWorld(OwnerHUD))
					{
						if (UBNPursuitSubsystem* P = W->GetSubsystem<UBNPursuitSubsystem>())
						{
							if (P->GetState().IsPursuitRunning())
							{
								P->StopPursuit();
							}
							else
							{
								P->EscalateOrStartPursuit();
							}
						}
						if (OwnerHUD.IsValid())
						{
							OwnerHUD->CloseMenu();
						}
					}
					return FReply::Handled();
				}))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("ARACI DÜZELT [R]")), EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this]()
				{
					if (ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD))
					{
						Car->ResetVehicle();
						if (OwnerHUD.IsValid())
						{
							OwnerHUD->CloseMenu();
						}
					}
					return FReply::Handled();
				}))
		]
	];

	WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 8.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("KARAMARA ŞEHRİ")), EBNButtonVariant::ConfirmGreen,
				FOnClicked::CreateLambda([this]()
				{
					if (UWorld* W = GetHUDWorld(OwnerHUD))
					{
						if (OwnerHUD.IsValid()) OwnerHUD->CloseMenu();
						UGameplayStatics::OpenLevel(W, FName(TEXT("/Game/Maps/KaramaraSlice")));
					}
					return FReply::Handled();
				}))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("TEST PİSTİ")), EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this]()
				{
					if (UWorld* W = GetHUDWorld(OwnerHUD))
					{
						if (OwnerHUD.IsValid()) OwnerHUD->CloseMenu();
						UGameplayStatics::OpenLevel(W, FName(TEXT("/Game/Maps/TestTrack")));
					}
					return FReply::Handled();
				}))
		]
	];

	if (!ActionFeedback.IsEmpty())
	{
		WorldTools->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(ActionFeedback)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Ok)
		];
	}

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().FillWidth(0.62f).Padding(FMargin(0.f, 0.f, 18.f, 0.f))
	[
		SNew(SScrollBox) + SScrollBox::Slot()[RaceList]
	]
	+ SHorizontalBox::Slot().FillWidth(0.38f)
	[
		SNew(SBorder)
		.BorderImage(BNUITheme::WhiteBrush())
		.BorderBackgroundColor(BNUITheme::Panel)
		.Padding(FMargin(16.f))
		[
			WorldTools
		]
	];
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildControlsTab()
{
	struct FBind { const TCHAR* Key; const TCHAR* Desc; };
	static const FBind Binds[] = {
		{ TEXT("W / A / S / D  (Ok Tuşları)"), TEXT("Gaz, yönlendirme, fren ve geri vites") },
		{ TEXT("SPACE  (Alt Gamepad)"), TEXT("El freni — viraj girişinde A/D ile basılı tutarak drift başlat") },
		{ TEXT("SOL SHIFT / SOL ALT / N"), TEXT("NOS Nitro Takviyesi (kendiliğinden dolar; drift ve yakın geçişlerle hızlanır)") },
		{ TEXT("E / Q  (Drag Yarışı)"), TEXT("Drag yarışında manuel vites yükselt [E] / düşür [Q] (kırmızı bölgede motor patlar!)") },
		{ TEXT("M / TAB"), TEXT("Karamara Şehir Haritası & A* GPS Navigasyon (tıklayıp rota çiz)") },
		{ TEXT("ENTER"), TEXT("Yarış halkasındayken Yarış Özeti, garaj avlusundayken Selin'in Garajı'nı aç") },
		{ TEXT("X / LEFT CTRL"), TEXT("Speedbreaker — zaman yavaşlatma ve yüksek hızlı manevra kontrolü") },
		{ TEXT("C"), TEXT("Kamera modunu değiştir (Yakın / Uzak / Kaput / Tampon Kamera)") },
		{ TEXT("V / ORTA FARE (L3)"), TEXT("Arka Bakış — basılı tutarak arkadaki polis ve rakipleri kontrol et") },
		{ TEXT("R"), TEXT("Aracı yolda düzelt") },
		{ TEXT("P"), TEXT("Serbest sürüşte Polis Takibi başlat / Heat yükselt (yarışta kilitli)") },
		{ TEXT("G"), TEXT("Selin'in Garajı (360° canlı araç vitrini, parça Tier 1-6 ve 6 araç)") },
		{ TEXT("B"), TEXT("Black Numbers Listesi ve yarış programı") },
		{ TEXT("ESC"), TEXT("Menüyü aç / kapat veya hikaye kartını geç") },
		{ TEXT("BACKSPACE"), TEXT("Aktif yarışı iptal et") },
	};

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("SÜRÜŞ & KISAYOL TUŞLARI"), TEXT("KLAVYE + GAMEPAD"))
	];
	for (const FBind& Row : Binds)
	{
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Panel).Padding(FMargin(14.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.36f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Row.Key)).Font(BNUITheme::Font(11)).ColorAndOpacity(BNUITheme::Accent)]
				+ SHorizontalBox::Slot().FillWidth(0.64f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Row.Desc)).Font(BNUITheme::Font(11, false)).ColorAndOpacity(BNUITheme::Bone)]
			]
		];
	}
	return SNew(SScrollBox) + SScrollBox::Slot()[List];
}
