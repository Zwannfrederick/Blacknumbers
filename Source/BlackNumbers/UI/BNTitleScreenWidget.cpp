#include "UI/BNTitleScreenWidget.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
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
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Text/STextBlock.h"

void SBNTitleScreenWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;
	const FSlateBrush* VigFull = BNUITheme::GetVignetteFullBrush();
	const FSlateBrush* VigLeft = BNUITheme::GetVignetteLeftBrush();
	const bool bHasVigFull = VigFull != BNUITheme::WhiteBrush();
	const bool bHasVigLeft = VigLeft != BNUITheme::WhiteBrush();

	ChildSlot
	[
		SNew(SOverlay)
		// Subtle radial cinema vignette over the live 3D Karamara showcase viewport
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(VigFull)
			.ColorAndOpacity(bHasVigFull ? FLinearColor(1.f, 1.f, 1.f, 0.90f) : FLinearColor(0.f, 0.f, 0.f, 0.f))
		]
		// Smooth exponential dark-left cinema fade (replaces harsh stepped boxes)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SNew(SBox).WidthOverride(780.f)
			[
				SNew(SImage)
				.Image(VigLeft)
				.ColorAndOpacity(bHasVigLeft ? FLinearColor::White : FLinearColor(0.025f, 0.028f, 0.036f, 0.90f))
			]
		]
		// Top & bottom cinema framing anchor bars
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(18.f)
				[
					SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::AsphaltDark)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BNUITheme::MakeHazardBar(4.f)
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(26.f)
			[
				SNew(SBorder)
				.BorderImage(BNUITheme::WhiteBrush())
				.BorderBackgroundColor(BNUITheme::AsphaltDark)
				.Padding(FMargin(44.f, 5.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("ESC: Sürüşe Geç   •   G: Selin'in Garajı   •   B: 16'lık Blacklist   •   P: Polis Takibi   •   C: Kamera")))
						.Font(BNUITheme::Font(9, false))
						.ColorAndOpacity(BNUITheme::SteelMuted)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("KARAMARA EDITION  //  UE 5.8")))
						.Font(BNUITheme::Font(9, true))
						.ColorAndOpacity(BNUITheme::AmberGold)
					]
				]
			]
		]
		// Main left-docked Most Wanted title menu
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(42.f, 44.f, 0.f, 42.f))
		[
			SNew(SBox).WidthOverride(448.f)
			[
				SAssignNew(SidebarSlot, SVerticalBox)
			]
		]
	];

	Refresh();
}

void SBNTitleScreenWidget::Refresh()
{
	if (!SidebarSlot.IsValid())
	{
		return;
	}
	SidebarSlot->ClearChildren();
	SidebarSlot->AddSlot().FillHeight(1.f)[BuildSidebarContent()];
}

TSharedRef<SWidget> SBNTitleScreenWidget::BuildCampaignStatusCard()
{
	UGameInstance* GI = SBNGameMenuWidget::GetGI(OwnerHUD);
	const UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;

	FString ActLine = TEXT("PERDE 1 - DÖNÜŞ");
	FString TargetLine = TEXT("#16 — Sıradaki Hedef");
	int32 DefeatedCount = 0;
	const TCHAR* ArtKey = TEXT("T_UI_Art_HeroCar");
	if (Story)
	{
		const FBNStoryState& S = Story->GetState();
		ActLine = FBNStoryState::ActLabel(S.Act);
		DefeatedCount = S.GetDefeatedCount();
		if (S.bCampaignCompleted)
		{
			TargetLine = TEXT("Kampanya Tamamlandı — Serbest Sürüş");
		}
		else if (S.bFinalChaseActive)
		{
			TargetLine = TEXT("ÖZEL KADEME — Karamara'dan Kaçış");
			ArtKey = TEXT("T_UI_Art_Pursuit");
		}
		else if (const FBNBossSpec* NextBoss = FBNStoryState::FindBossByRank(S.NextBossRank))
		{
			TargetLine = FString::Printf(TEXT("#%02d %s  •  %s"), NextBoss->Rank, *NextBoss->Name, *NextBoss->VehicleName);
			if (NextBoss->Rank <= 4)
			{
				ArtKey = TEXT("T_UI_Art_Boss01");
			}
		}
	}

	const int32 Cash = Prog ? Prog->GetProfile().Cash : 4000;
	const int32 Rep = Prog ? Prog->GetProfile().Reputation : 0;
	const FBNVehicleSpec ActiveCarSpec = Prog
		? FBNVehicleSpec::ForCar(Prog->GetProfile().ActiveCar)
		: FBNVehicleSpec::ForCar(EBNCarId::KuroganeMSpec);
	const FSlateBrush* ArtBrush = BNUITheme::GetArtBrush(ArtKey);

	return SNew(SBorder)
		.BorderImage(BNUITheme::GetPanelBevelBrush())
		.BorderBackgroundColor(FLinearColor(0.96f, 0.96f, 0.96f, 0.94f))
		.Padding(FMargin(12.f, 11.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
			[
				SNew(SBox).WidthOverride(104.f).HeightOverride(66.f)
				[
					SNew(SImage)
					.Image(ArtBrush)
					.ColorAndOpacity(ArtBrush != BNUITheme::WhiteBrush() ? FLinearColor::White : BNUITheme::AmberDarkCard)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Text(FText::FromString(ActLine)).Font(BNUITheme::Font(9, true)).ColorAndOpacity(BNUITheme::NosCyan)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("LİSTE %d/16"), DefeatedCount)))
						.Font(BNUITheme::Font(9, true)).ColorAndOpacity(BNUITheme::AmberGold)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 2.f))
				[
					SNew(STextBlock).Text(FText::FromString(TargetLine)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::ChalkWhite).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("$%s  •  %s İtibar  •  %s"),
						*FString::FormatAsNumber(Cash), *FString::FormatAsNumber(Rep), *ActiveCarSpec.DisplayName)))
					.Font(BNUITheme::Font(9, false))
					.ColorAndOpacity(BNUITheme::EmeraldGreen)
				]
			]
		];
}

TSharedRef<SWidget> SBNTitleScreenWidget::BuildSidebarContent()
{
	UGameInstance* GI = SBNGameMenuWidget::GetGI(OwnerHUD);
	const UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const bool bHasProgress = (Story && (Story->GetState().bIntroShown || Story->GetState().GetDefeatedCount() > 0))
		|| (Prog && Prog->GetProfile().RacesCompleted > 0);
	const FText StartLabel = FText::FromString(bHasProgress ? TEXT("DEVAM ET") : TEXT("OYUNA BAŞLA"));

	auto OpenTabAction = [this](EBNMenuTab Tab)
	{
		return FOnClicked::CreateLambda([this, Tab]()
		{
			if (OwnerHUD.IsValid())
			{
				OwnerHUD->DismissTitleScreen(false);
				OwnerHUD->OpenMenu(Tab);
			}
			return FReply::Handled();
		});
	};

	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);

	// Iconic Most Wanted Masthead
	Col->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("KARAMARA UNDERGROUND  //  MOST WANTED")))
		.Font(BNUITheme::Font(10, true))
		.ColorAndOpacity(BNUITheme::AmberGold)
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 6.f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("BLACK NUMBERS")))
		.Font(BNUITheme::Font(38, true))
		.ColorAndOpacity(BNUITheme::ChalkWhite)
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
	[
		BNUITheme::MakeHazardBar(7.f)
	];

	// Compact visual campaign dossier strip
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
	[
		BuildCampaignStatusCard()
	];

	// Hover-lit Most Wanted navigation blades
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 9.f))
	[
		BNUITheme::MakeNavBladeButton(StartLabel, FText::FromString(TEXT("Karamara sokaklarına çık ve serbest sürüşe başla")), true,
			FOnClicked::CreateLambda([this]()
			{
				if (OwnerHUD.IsValid())
				{
					OwnerHUD->DismissTitleScreen(true);
				}
				return FReply::Handled();
			}))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 7.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("HIZLI YARIŞ")),
			FText::FromString(TEXT("Sprint, Pist Turu ve Drift etkinliklerine doğrudan geç")), false, OpenTabAction(EBNMenuTab::Races))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 7.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("SELİN'İN GARAJI")),
			FText::FromString(TEXT("360° canlı araç vitrini, 6 araç ve Tier 1-6 parça geliştirme")), false, OpenTabAction(EBNMenuTab::Garage))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 7.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("16'LIK BLACKLIST")),
			FText::FromString(TEXT("Sendika sürücü dosyaları, R1-R8 kanıtlar ve meydan okuma")), false, OpenTabAction(EBNMenuTab::BlackNumbers))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("GRAFİK & AYARLAR")),
			FText::FromString(TEXT("RTX 3050 Ti ön ayarları, sivil trafik ve ses seviyesi")), false, OpenTabAction(EBNMenuTab::Settings))
	];

	// Utility row: New Career (2-step safe confirm) + Quit
	if (!bConfirmNewCareer)
	{
		Col->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("YENİ KARİYER")), EBNButtonVariant::SecondarySteel,
					FOnClicked::CreateLambda([this]()
					{
						bConfirmNewCareer = true;
						StatusNotice.Reset();
						Refresh();
						return FReply::Handled();
					}), true, 10, FMargin(12.f, 9.f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("OYUNDAN ÇIK")), EBNButtonVariant::SecondarySteel,
					FOnClicked::CreateLambda([this]()
					{
						UWorld* W = SBNGameMenuWidget::GetHUDWorld(OwnerHUD);
						APlayerController* PC = OwnerHUD.IsValid() ? OwnerHUD->GetOwningPlayerController() : nullptr;
						if (W)
						{
							UKismetSystemLibrary::QuitGame(W, PC, EQuitPreference::Quit, false);
						}
						else if (PC)
						{
							PC->ConsoleCommand(TEXT("quit"));
						}
						return FReply::Handled();
					}), true, 10, FMargin(12.f, 9.f))
			]
		];
	}
	else
	{
		Col->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.65f).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("EMİN MİSİN? SIFIRLA")), EBNButtonVariant::DangerCrimson,
					FOnClicked::CreateLambda([this]()
					{
						if (UGameInstance* CurGI = SBNGameMenuWidget::GetGI(OwnerHUD))
						{
							if (UBNProgressionSubsystem* Pr = CurGI->GetSubsystem<UBNProgressionSubsystem>())
							{
								Pr->ResetProfile();
								if (ABNVehiclePawn* Car = SBNGameMenuWidget::GetPlayerCar(OwnerHUD))
								{
									Pr->ApplyProfileToCar(*Car);
								}
							}
						}
						bConfirmNewCareer = false;
						StatusNotice = TEXT("Kariyer sıfırlandı. Yeni başlangıca hazırsın.");
						Refresh();
						return FReply::Handled();
					}), true, 10, FMargin(12.f, 9.f))
			]
			+ SHorizontalBox::Slot().FillWidth(0.35f)
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("VAZGEÇ")), EBNButtonVariant::SecondarySteel,
					FOnClicked::CreateLambda([this]()
					{
						bConfirmNewCareer = false;
						Refresh();
						return FReply::Handled();
					}), true, 10, FMargin(12.f, 9.f))
			]
		];
	}

	if (!StatusNotice.IsEmpty())
	{
		Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(StatusNotice)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::EmeraldGreen).AutoWrapText(true)
		];
	}

	Col->AddSlot().FillHeight(1.f)[SNew(SSpacer)];
	return Col;
}

void ABNHUD::OpenTitleScreen()
{
	if (!CanUseViewportSlate())
	{
		return;
	}
	if (bPausedForRaceMenu)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
		bPausedForRaceMenu = false;
	}
	if (ABNRaceEvent* ActiveRace = ABNRaceEvent::FindActiveRace(GetWorld()))
	{
		ActiveRace->EndRace();
	}
	const bool bWasGarageTab = MenuWidget.IsValid() && MenuWidget->GetActiveTab() == EBNMenuTab::Garage;
	if (MenuHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuHost.ToSharedRef());
		MenuHost.Reset();
		MenuWidget.Reset();
	}
	if (ResultsHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ResultsHost.ToSharedRef());
		ResultsHost.Reset();
		ResultsOverlayWidget.Reset();
	}

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (bWasGarageTab && Car)
	{
		if (const UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->ApplyProfileToCar(*Car);
		}
	}

	if (TitleScreenWidget.IsValid())
	{
		TitleScreenWidget->Refresh();
		return;
	}

	SAssignNew(TitleScreenWidget, SBNTitleScreenWidget).OwnerHUD(this);
	TitleScreenHost = SNew(SWeakWidget).PossiblyNullContent(TitleScreenWidget.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(TitleScreenHost.ToSharedRef(), 40);

	if (Car)
	{
		Car->SetInputLocked(true);
		Car->SetShowcaseMode(true, -95.f);
	}
	ApplyCursorMode(true);
	SyncStoryCardOverlay();
}

void ABNHUD::DismissTitleScreen(bool bStartDriving)
{
	if (GEngine && GEngine->GameViewport && TitleScreenHost.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TitleScreenHost.ToSharedRef());
	}
	TitleScreenHost.Reset();
	TitleScreenWidget.Reset();

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (!bStartDriving)
	{
		if (Car)
		{
			Car->SetShowcaseMode(false);
		}
		return;
	}

	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		Story->EnsureIntroShown(Car);
	}
	SyncStoryCardOverlay();

	if (!HasStoryCardOpen() && !IsMenuOpen())
	{
		if (Car)
		{
			Car->SetShowcaseMode(false);
			Car->SetInputLocked(false);
		}
		ApplyCursorMode(false);
	}
}
