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
		// Subtle radial cinema vignette keeping the live 3D Karamara car bright and open
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(VigFull)
			.ColorAndOpacity(bHasVigFull ? FLinearColor(1.f, 1.f, 1.f, 0.72f) : FLinearColor(0.f, 0.f, 0.f, 0.f))
		]
		// Narrow smooth left-side fade leaving ~68% of the frame crystal clear for the 3D showcase car
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SImage)
				.Image(VigLeft)
				.ColorAndOpacity(bHasVigLeft ? FLinearColor::White : FLinearColor(0.020f, 0.024f, 0.032f, 0.88f))
			]
		]
		// Minimal bottom-left shortcut legend (clean, zero clutter)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(56.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("ESC  MENÜ    •    G  GARAJ    •    B  LİSTE    •    M  HARİTA")))
			.Font(BNUITheme::Font(11, true))
			.ColorAndOpacity(FLinearColor(0.76f, 0.74f, 0.70f, 0.78f))
		]
		// Left-docked car-forward Most Wanted title stack (wide enough at 720p and 1080p so title never clips)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(52.f, 52.f, 0.f, 64.f))
		[
			SNew(SBox).WidthOverride(470.f)
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

	FString TargetPill = TEXT("#16  SIRADAKİ HEDEF");
	int32 DefeatedCount = 0;
	if (Story)
	{
		const FBNStoryState& S = Story->GetState();
		DefeatedCount = S.GetDefeatedCount();
		if (S.bCampaignCompleted)
		{
			TargetPill = TEXT("KARAMARA HÜKÜMDARI");
		}
		else if (S.bFinalChaseActive)
		{
			TargetPill = TEXT("ÖZEL KADEME  •  KAÇIŞ");
		}
		else if (const FBNBossSpec* NextBoss = FBNStoryState::FindBossByRank(S.NextBossRank))
		{
			TargetPill = FString::Printf(TEXT("#%02d %s"), NextBoss->Rank, *BNUITheme::UpperTr(NextBoss->Name));
		}
	}

	const int32 Cash = Prog ? Prog->GetProfile().Cash : 4000;
	const int32 TotalBounty = Story ? Story->GetState().TotalBounty : 0;
	const FBNVehicleSpec ActiveCarSpec = Prog
		? FBNVehicleSpec::ForCar(Prog->GetProfile().ActiveCar)
		: FBNVehicleSpec::ForCar(EBNCarId::KuroganeMSpec);

	// MW 2005 transparent bracketed [ ] status readout (ZERO grey rectangle card!)
	return BNUITheme::MakePanel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TargetPill))
				.Font(BNUITheme::Font(13))
				.ColorAndOpacity(BNUITheme::Bone)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%d / 16"), DefeatedCount)))
				.Font(BNUITheme::DisplayFont(18))
				.ColorAndOpacity(BNUITheme::AccentHi)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("$%s   •   ÖDÜL %s   •   %s"),
				*FString::FormatAsNumber(Cash), *FString::FormatAsNumber(TotalBounty), *BNUITheme::UpperTr(ActiveCarSpec.DisplayName))))
			.Font(BNUITheme::Font(11, true))
			.ColorAndOpacity(BNUITheme::Muted)
		],
		FMargin(16.f, 10.f), BNUITheme::Ink, BNUITheme::Accent);
}

TSharedRef<SWidget> SBNTitleScreenWidget::BuildSidebarContent()
{
	UGameInstance* GI = SBNGameMenuWidget::GetGI(OwnerHUD);
	const UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const bool bHasProgress = (Story && (Story->GetState().QualifiersWon > 0 || Story->GetState().GetDefeatedCount() > 0))
		|| (Prog && Prog->GetProfile().RacesCompleted > 0);
	const FText StartLabel = FText::FromString(bHasProgress ? TEXT("KARİYERE DEVAM ET") : TEXT("KARİYERE BAŞLA"));

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

	// Wordmark: stacked display type, the second word in sodium.
	Col->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("KARAMARA  /  PORT MARA")))
		.Font(BNUITheme::Font(12))
		.ColorAndOpacity(BNUITheme::Muted)
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, -4.f, 0.f, 0.f))
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("BLACK"))).Font(BNUITheme::DisplayFont(78)).ColorAndOpacity(BNUITheme::Bone)
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, -34.f, 0.f, 26.f))
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("NUMBERS"))).Font(BNUITheme::DisplayFont(78)).ColorAndOpacity(BNUITheme::Accent)
	];

	// Slim career pill
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 22.f))
	[
		BuildCampaignStatusCard()
	];

	// Clean, bold, minimal-copy Most Wanted navigation blades (zero verbose subtitle clutter)
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		BNUITheme::MakeNavBladeButton(StartLabel, FText::GetEmpty(), true, OpenTabAction(EBNMenuTab::BlackNumbers))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("HIZLI YARIŞ")), FText::GetEmpty(), false, OpenTabAction(EBNMenuTab::Races))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("SELİN'İN GARAJI")), FText::GetEmpty(), false, OpenTabAction(EBNMenuTab::Garage))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("SERBEST SÜRÜŞ")), FText::GetEmpty(), false, FOnClicked::CreateLambda([this]()
			{
				if (OwnerHUD.IsValid())
				{
					OwnerHUD->DismissTitleScreen(true);
				}
				return FReply::Handled();
			}))
	];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 22.f))
	[
		BNUITheme::MakeNavBladeButton(FText::FromString(TEXT("AYARLAR")), FText::GetEmpty(), false, OpenTabAction(EBNMenuTab::Settings))
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
					}), true, 12, FMargin(12.f, 9.f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				BNUITheme::MakeMWButton(FText::FromString(TEXT("ÇIKIŞ")), EBNButtonVariant::SecondarySteel,
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
					}), true, 12, FMargin(12.f, 9.f))
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
			SNew(STextBlock).Text(FText::FromString(StatusNotice)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Ok).AutoWrapText(true)
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
