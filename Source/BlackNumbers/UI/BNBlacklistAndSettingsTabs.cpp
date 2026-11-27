#include "UI/BNMenuWidget.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "GenericPlatform/GenericWindow.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Story/BNStoryTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/BNHUD.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNEngineAudioComponent.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNTrafficSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	int32 ReadCurrentGraphicsPreset(const UGameUserSettings* GUS)
	{
		if (!GUS)
		{
			return 2;
		}
		const int32 Overall = GUS->GetOverallScalabilityLevel();
		return Overall >= 0 ? Overall : GUS->GetShadowQuality();
	}

	const TCHAR* ArtForBlacklistRank(int32 Rank)
	{
		if (Rank <= 4) return TEXT("T_UI_Art_Boss01");
		if (Rank <= 8) return TEXT("T_UI_Art_Pursuit");
		if (Rank <= 12) return TEXT("T_UI_Art_Garage");
		return TEXT("T_UI_Art_HeroCar");
	}
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildBlackNumbersTab()
{
	UGameInstance* GI = GetGI(OwnerHUD);
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	if (!Story)
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Hikâye alt sistemi bulunamadı.")));
	}

	const FBNStoryState& S = Story->GetState();
	UWorld* World = GetHUDWorld(OwnerHUD);
	if (SelectedBlacklistRank < 1 || SelectedBlacklistRank > 16)
	{
		SelectedBlacklistRank = FMath::Clamp(S.NextBossRank, 1, 16);
	}
	const FBNBossSpec* ActiveBoss = FBNStoryState::FindBossByRank(SelectedBlacklistRank);
	if (!ActiveBoss)
	{
		ActiveBoss = FBNStoryState::FindBossByRank(16);
	}

	const bool bDef = S.IsBossDefeated(ActiveBoss->Rank);
	const bool bCanFight = S.CanChallengeBoss(ActiveBoss->Rank);
	const bool bRacesOk = S.AreRacesComplete(ActiveBoss->Rank);
	const bool bMilestonesOk = S.AreMilestonesComplete(ActiveBoss->Rank);
	const bool bBountyOk = S.IsBountyComplete(ActiveBoss->Rank);
	const bool bDuelReady = S.IsDuelUnlocked(ActiveBoss->Rank);
	const bool bFinalChase = S.bFinalChaseActive && ActiveBoss->Rank == 1;

	const int32 QualifierCount = ActiveBoss->Qualifiers.Num();
	const int32 Wins = bDef ? QualifierCount : (bCanFight ? S.QualifiersWon : 0);
	const int32 DoneMilestones = bDef ? ActiveBoss->RequiredMilestones : (bCanFight ? S.GetCompletedMilestoneCount() : 0);
	const int32 Cash = Prog ? Prog->GetProfile().Cash : 0;
	const FBNDistrictTerritoryInfo BossTerr = S.GetDistrictTerritory(ActiveBoss->HomeDistrict);

	// 1. Masthead: the rank number IS the identity of this game — giant sodium numeral, name beside it.
	TSharedRef<SWidget> TopMasthead = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, -18.f, 18.f, -14.f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("%02d"), ActiveBoss->Rank)))
		.Font(BNUITheme::DisplayFont(112))
		.ColorAndOpacity(bDef ? BNUITheme::Muted : BNUITheme::Accent)
	]
	+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("KARA LİSTE  •  %s"), *ActiveBoss->TagWord.ToUpper())))
			.Font(BNUITheme::Font(13, true))
			.ColorAndOpacity(BNUITheme::Accent)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -2.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(ActiveBoss->Name))
			.Font(BNUITheme::DisplayFont(44))
			.ColorAndOpacity(BNUITheme::Bone)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%s   /   %s"), *ActiveBoss->VehicleName, *BossTerr.BoroughTitle)))
			.Font(BNUITheme::Font(14, false))
			.ColorAndOpacity(BNUITheme::Muted)
		]
	]
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f, 0.f, 0.f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(Cash))))
			.Font(BNUITheme::DisplayFont(28))
			.ColorAndOpacity(BNUITheme::Bone)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("ÖDÜL  %s"), *FString::FormatAsNumber(S.TotalBounty))))
			.Font(BNUITheme::Font(13, true))
			.ColorAndOpacity(BNUITheme::Muted)
		]
	];

	// 2. Left Column: High-Contrast Rival Plate + Tilted Crimson DEFEATED Rubber Stamp + Compact #16..#01 Switcher
	const FSlateBrush* ArtBrush = BNUITheme::GetArtBrush(ArtForBlacklistRank(ActiveBoss->Rank));
	const bool bHasArt = ArtBrush != BNUITheme::WhiteBrush();

	TSharedRef<SOverlay> RivalPlate = SNew(SOverlay)
	+ SOverlay::Slot()
	[
		SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFill).VAlign(VAlign_Center)
			[
				SNew(SImage).Image(ArtBrush)
				.ColorAndOpacity(bHasArt ? (bDef ? FLinearColor(0.35f, 0.35f, 0.35f, 1.f) : FLinearColor::White)
					: BNUITheme::Ink)
			]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
	[
		SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Ink).Padding(FMargin(14.f, 8.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  •  %s"), *ActiveBoss->OrgRole, *ActiveBoss->DrivingStyle)))
				.Font(BNUITheme::Font(11, true))
				.ColorAndOpacity(BNUITheme::AccentHi)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Ödül: $%s + %d İtibar%s"),
					*FString::FormatAsNumber(ActiveBoss->RewardCash), ActiveBoss->RewardRep,
					FBNStoryState::IsDistrictRulerRank(ActiveBoss->Rank) ? TEXT("  •  PEMBE KAĞIT HÜKÜMDARI") : TEXT(""))))
				.Font(BNUITheme::Font(10, false))
				.ColorAndOpacity(BNUITheme::Bone)
			]
		]
	];

	if (bDef)
	{
		RivalPlate->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 24.f, 0.f, 0.f))
		[
			SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Danger).Padding(FMargin(22.f, 4.f))
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-8.f))))
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("YENİLDİ"))).Font(BNUITheme::DisplayFont(40)).ColorAndOpacity(BNUITheme::Bone)
			]
		];
	}
	else if (bDuelReady || bFinalChase)
	{
		RivalPlate->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 14.f, 14.f, 0.f))
		[
			BNUITheme::MakePanel(
				SNew(STextBlock).Text(FText::FromString(bFinalChase ? TEXT("SON KOVALAMACA") : TEXT("DÜELLO AÇIK")))
				.Font(BNUITheme::Font(13)).ColorAndOpacity(BNUITheme::InkSolid),
				FMargin(12.f, 5.f), BNUITheme::Accent)
		];
	}

	// Compact 2-row 16-member rank selector strip (#16..#09, #08..#01) right beneath the portrait
	TSharedRef<SHorizontalBox> RankRowTop = SNew(SHorizontalBox);
	TSharedRef<SHorizontalBox> RankRowBot = SNew(SHorizontalBox);
	for (int32 r = 16; r >= 1; --r)
	{
		const bool bRDef = S.IsBossDefeated(r);
		const bool bRSel = (r == SelectedBlacklistRank);
		const bool bRNext = (!bRDef && S.NextBossRank == r);
		const FLinearColor PillBg = bRSel ? BNUITheme::Accent
			: bRDef ? BNUITheme::DangerDark
			: bRNext ? BNUITheme::AccentDark
			: BNUITheme::Card;
		const FLinearColor PillTxt = bRSel ? BNUITheme::InkSolid
			: bRDef ? BNUITheme::Danger
			: bRNext ? BNUITheme::AccentHi : BNUITheme::Muted;

		TSharedRef<SHorizontalBox>& TargetRow = (r >= 9) ? RankRowTop : RankRowBot;
		TargetRow->AddSlot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateLambda([this, r]()
			{
				UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiClick, 0.72f);
				SelectedBlacklistRank = r;
				Refresh();
				return FReply::Handled();
			}))
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PillBg).Padding(FMargin(0.f, 5.f)).HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("#%02d"), r)))
					.Font(BNUITheme::Font(bRSel ? 12 : 10))
					.ColorAndOpacity(PillTxt)
				]
			]
		];
	}

	TSharedRef<SWidget> LeftColumn = SNew(SBox).WidthOverride(360.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			BNUITheme::MakePanel(RivalPlate, FMargin(0.f), BNUITheme::Panel, bDef ? BNUITheme::Danger : BNUITheme::Accent)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 4.f))
		[
			RankRowTop
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			RankRowBot
		]
	];

	// 3. Right Column: requirements, races and sub-tabs
	auto MakeMWStatRow = [](const TCHAR* Label, const FString& CurVal, const FString& NeedVal, bool bDone) -> TSharedRef<SWidget>
	{
		const FLinearColor ValCol = bDone ? BNUITheme::AccentHi : BNUITheme::Bone;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(BNUITheme::Font(16)).ColorAndOpacity(BNUITheme::Bone)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f))
			[
				SNew(SBox).WidthOverride(122.f).HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text(FText::FromString(CurVal)).Font(BNUITheme::Font(17)).ColorAndOpacity(ValCol)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(112.f).HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text(FText::FromString(NeedVal)).Font(BNUITheme::Font(16)).ColorAndOpacity(BNUITheme::Muted)
				]
			];
	};

	// The iconic MW 2005 White 4-Corner Bracketed [ ] 3-Gate Block (exact match to Izzy photo!)
	TSharedRef<SVerticalBox> StatsInner = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(bDef ? TEXT("DOSYA KAPANDI") : bDuelReady ? TEXT("TÜM ŞARTLAR TAMAM — DÜELLO AÇIK") : TEXT("MEYDAN OKUMA ŞARTLARI")))
			.Font(BNUITheme::Font(10, true))
			.ColorAndOpacity(bDuelReady ? BNUITheme::Ok : BNUITheme::Accent)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(12.f, 0.f))
		[
			SNew(SBox).WidthOverride(122.f).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("MEVCUT"))).Font(BNUITheme::Font(12)).ColorAndOpacity(BNUITheme::Muted)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(112.f).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("GEREKLİ"))).Font(BNUITheme::Font(12)).ColorAndOpacity(BNUITheme::Muted)
			]
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
	[
		MakeMWStatRow(TEXT("Yarış Galibiyeti"), FString::FromInt(Wins), FString::FromInt(QualifierCount), bRacesOk)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
	[
		MakeMWStatRow(TEXT("Dönüm Noktaları"), FString::FromInt(DoneMilestones), FString::FromInt(ActiveBoss->RequiredMilestones), bMilestonesOk)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
	[
		MakeMWStatRow(TEXT("Toplam Ödül"),
			FString::FormatAsNumber(S.TotalBounty),
			FString::FormatAsNumber(ActiveBoss->RequiredBounty),
			bBountyOk)
	];

	// Active Sub-View Content driven by the Bottom MW Stencil Icon Carousel!
	TSharedRef<SVerticalBox> SubContent = SNew(SVerticalBox);
	if (BlacklistSubMode == 0)
	{
		// Mode 0: Race Events (Checkered Flag)
		for (int32 Index = 0; Index <= QualifierCount; ++Index)
		{
			const bool bDuel = Index == QualifierCount;
			const bool bDone = bDef || Index < Wins;
			const bool bNext = bCanFight && (bDuel ? bDuelReady : (Index == Wins));
			const FName Asset = bDuel ? FName(*ActiveBoss->RaceAssetName) : ActiveBoss->Qualifiers[Index];
			const ABNRaceEvent* Event = UBNStorySubsystem::FindProgramRace(World, Asset);
			const FString RaceName = Event && Event->Definition ? Event->Definition->DisplayName.ToString()
				: (bDuel ? ActiveBoss->DisciplineLabel : TEXT("Karamara Sokak Yarışı"));
			const FString StatusStr = bDone ? TEXT("TAMAM")
				: (bDuel && !bDuelReady) ? TEXT("ŞARTLAR EKSİK")
				: bNext ? TEXT("HAZIR") : TEXT("KİLİTLİ");
			const FLinearColor RowTint = bNext ? BNUITheme::AccentDark : BNUITheme::Card;

			SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(RowTint).Padding(FMargin(12.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(86.f)
						[
							SNew(STextBlock).Text(FText::FromString(bDuel ? TEXT("DÜELLO") : FString::Printf(TEXT("YARIŞ %d"), Index + 1)))
							.Font(BNUITheme::Font(11)).ColorAndOpacity(bDuel ? BNUITheme::AccentHi : BNUITheme::Accent)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(RaceName)).Font(BNUITheme::Font(13, true)).ColorAndOpacity(BNUITheme::Bone)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(StatusStr)).Font(BNUITheme::Font(11))
						.ColorAndOpacity(bDone ? BNUITheme::Ok : bNext ? BNUITheme::AccentHi : BNUITheme::Muted)
					]
				]
			];
		}
	}
	else if (BlacklistSubMode == 1)
	{
		// Mode 1: Pursuit & Open-World Milestones (Triangle-Check) + 1-click "Polis Takibi Başlat"
		for (int32 m = 0; m < ActiveBoss->Milestones.Num(); ++m)
		{
			const FBNBossMilestone& M = ActiveBoss->Milestones[m];
			const bool bMDone = bDef || (bCanFight && S.IsMilestoneCompleted(m));
			SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 5.f))
			[
				SNew(SBorder).BorderImage(WhiteBrush())
				.BorderBackgroundColor(bMDone ? FLinearColor(0.008f, 0.04f, 0.015f, 0.94f) : BNUITheme::Card)
				.Padding(FMargin(12.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(M.Title)).Font(BNUITheme::Font(12))
							.ColorAndOpacity(bMDone ? BNUITheme::Ok : BNUITheme::Bone)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(M.Description)).Font(BNUITheme::Font(10, false))
							.ColorAndOpacity(BNUITheme::Muted).AutoWrapText(true)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(bMDone ? TEXT("TAMAM") : FString::Printf(TEXT("+%s ÖDÜL"), *FString::FormatAsNumber(M.BonusBounty))))
						.Font(BNUITheme::Font(11))
						.ColorAndOpacity(bMDone ? BNUITheme::Ok : BNUITheme::AccentHi)
					]
				]
			];
		}
		SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f)).HAlign(HAlign_Left)
		[
			BNUITheme::MakeMWButton(FText::FromString(TEXT("POLİS TAKİBİ BAŞLAT (ÖDÜL & DÖNÜM NOKTASI KAS)")),
				EBNButtonVariant::DangerCrimson,
				FOnClicked::CreateLambda([this]()
				{
					UWorld* W = GetHUDWorld(OwnerHUD);
					if (UBNPursuitSubsystem* Pursuit = W ? W->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
					{
						if (OwnerHUD.IsValid()) { OwnerHUD->CloseMenu(); }
						Pursuit->StartPursuit(1);
					}
					return FReply::Handled();
				}), true, 11, FMargin(16.f, 8.f))
		];
	}
	else if (BlacklistSubMode == 2)
	{
		// Mode 2: Bounty, Rap Sheet & Borough Control (Police Shield)
		const UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		const int32 CityDmg = Pursuit ? Pursuit->GetCostToStateDollars() : 0;
		auto AddStatLine = [&](const FString& Left, const FString& Right, const FLinearColor& Col)
		{
			SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Card).Padding(FMargin(12.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Left)).Font(BNUITheme::Font(12, true)).ColorAndOpacity(BNUITheme::Bone)]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(Right)).Font(BNUITheme::Font(12)).ColorAndOpacity(Col)]
				]
			];
		};
		AddStatLine(TEXT("Bankalanmış Toplam Ödül"), FString::FormatAsNumber(S.TotalBounty), BNUITheme::AccentHi);
		AddStatLine(TEXT("Polisten Kaçış / Yakalanma"), FString::Printf(TEXT("%d Kaçış  •  %d Yakalanma  •  İhtar %d/3"), S.TotalPursuitsEscaped, S.TotalBustedCount, S.ImpoundStrikes), BNUITheme::Nos);
		AddStatLine(TEXT("Şehir Faturası (Toplam Hasar)"), FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(CityDmg)), BNUITheme::Danger);
		AddStatLine(TEXT("Polis Heat Limiti & Bölge"), FString::Printf(TEXT("Maks Heat %d  •  %d/6 Bölge Özgür  •  %d/16 Yenildi"), S.GetMaxAllowedHeat(), S.GetLiberatedDistrictCount(), S.GetDefeatedCount()), BNUITheme::Ok);
	}
	else
	{
		// Mode 3: Conspiracy Evidence Notes (Dossier)
		SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Tarık Kaya: %s   •   Kanıt Dosyaları (%d)"),
				FBNStoryState::TarikAttitudeLabel(S.TarikAttitude), S.UnlockedEvidenceNotes.Num())))
			.Font(BNUITheme::Font(11)).ColorAndOpacity(BNUITheme::Nos)
		];
		for (const FString& Note : S.UnlockedEvidenceNotes)
		{
			SubContent->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::Card).Padding(FMargin(10.f, 5.f))
				[
					SNew(STextBlock).Text(FText::FromString(Note)).Font(BNUITheme::Font(10, false)).ColorAndOpacity(BNUITheme::Bone).AutoWrapText(true)
				]
			];
		}
	}

	// Primary action trigger: Launch Next Race or Boss Duel!
	const FString ActionCaption = bFinalChase ? TEXT("SON KOVALAMACA")
		: bDef ? TEXT("YENİLDİ")
		: bDuelReady ? TEXT("DÜELLOYA ÇIK")
		: !bRacesOk ? TEXT("YARIŞA ÇIK")
		: TEXT("ÖDÜL / DÖNÜM EKSİK");
	const bool bActionEnabled = bFinalChase || (bCanFight && (!bRacesOk || bDuelReady));
	FOnClicked LaunchAction = FOnClicked::CreateLambda([this, BossRank = ActiveBoss->Rank]()
	{
		ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD);
		UBNStorySubsystem* St = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNStorySubsystem>() : nullptr;
		if (Car && St && OwnerHUD.IsValid())
		{
			if (St->ChallengeSelectedBoss(Car, BossRank))
			{
				OwnerHUD->CloseMenu();
			}
			else
			{
				ActionFeedback = St->GetStatusFeedback();
				Refresh();
			}
		}
		return FReply::Handled();
	});

	// Bottom Authentic MW 2005 Stencil Icon Carousel (all 5 fit cleanly with generous margin!)
	TSharedRef<SHorizontalBox> IconCarousel = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
	[
		BNUITheme::MakeIconTab(TEXT("T_UI_Icon_Race"), TEXT("Yarışlar"), BlacklistSubMode == 0,
			FOnClicked::CreateLambda([this]() { BlacklistSubMode = 0; Refresh(); return FReply::Handled(); }))
	]
	+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
	[
		BNUITheme::MakeIconTab(TEXT("T_UI_Icon_Milestone"), TEXT("Dönüm Nokt."), BlacklistSubMode == 1,
			FOnClicked::CreateLambda([this]() { BlacklistSubMode = 1; Refresh(); return FReply::Handled(); }))
	]
	+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
	[
		BNUITheme::MakeIconTab(TEXT("T_UI_Icon_Bounty"), TEXT("Ödül & Sicil"), BlacklistSubMode == 2,
			FOnClicked::CreateLambda([this]() { BlacklistSubMode = 2; Refresh(); return FReply::Handled(); }))
	]
	+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
	[
		BNUITheme::MakeIconTab(TEXT("T_UI_Icon_Evidence"), TEXT("Soruşturma"), BlacklistSubMode == 3,
			FOnClicked::CreateLambda([this]() { BlacklistSubMode = 3; Refresh(); return FReply::Handled(); }))
	]
	+ SHorizontalBox::Slot().AutoWidth()
	[
		BNUITheme::MakeIconTab(TEXT("T_UI_Icon_Duel"), ActionCaption, bDuelReady || bFinalChase,
			LaunchAction, bActionEnabled, bDuelReady ? BNUITheme::Ok : BNUITheme::AccentHi)
	];

	TSharedRef<SVerticalBox> RightColumn = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, 2.f, 6.f, 14.f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(ActiveBoss->TauntQuote))
		.Font(BNUITheme::Font(15))
		.ColorAndOpacity(BNUITheme::Bone)
		.AutoWrapText(true)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		BNUITheme::MakePanel(StatsInner, FMargin(22.f, 14.f), BNUITheme::Panel)
	]
	+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		SNew(SScrollBox) + SScrollBox::Slot()[SubContent]
	]
	+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
	[
		IconCarousel
	];

	if (!ActionFeedback.IsEmpty())
	{
		RightColumn->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f)).HAlign(HAlign_Right)
		[
			SNew(STextBlock).Text(FText::FromString(ActionFeedback)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(BNUITheme::AccentHi)
		];
	}

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		TopMasthead
	]
	+ SVerticalBox::Slot().FillHeight(1.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 28.f, 0.f))
		[
			LeftColumn
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			RightColumn
		]
	];
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildSettingsTab()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	const int32 ActiveQuality = ReadCurrentGraphicsPreset(GUS);
	const bool bVSyncOn = GUS ? GUS->IsVSyncEnabled() : false;
	const bool bWindowed = GUS && GUS->GetFullscreenMode() == EWindowMode::Windowed;

	UWorld* World = GetHUDWorld(OwnerHUD);
	UBNTrafficSubsystem* Traffic = World ? World->GetSubsystem<UBNTrafficSubsystem>() : nullptr;
	const bool bTrafficOn = Traffic ? Traffic->IsTrafficEnabled() : true;
	ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD);
	const float CurrentEngineGain = (Car && Car->EngineAudio) ? Car->EngineAudio->MasterGain : ABNHUD::GetPreferredEngineGain();
	const float CurrentMusicVol = UBNAudioSubsystem::GetMusicVolume();
	const float CurrentSfxVol = UBNAudioSubsystem::GetSfxVolume();

	auto MakePresetCard = [&](int32 Level, const TCHAR* Title, const TCHAR* Desc) -> TSharedRef<SWidget>
	{
		const bool bSelected = ActiveQuality == Level;
		return SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(bSelected ? BNUITheme::AccentDark : BNUITheme::Panel)
			.Padding(FMargin(14.f, 10.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(bSelected ? BNUITheme::AccentHi : BNUITheme::Bone)]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))[SNew(STextBlock).Text(FText::FromString(Desc)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::Muted)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					BNUITheme::MakeMWButton(FText::FromString(bSelected ? TEXT("SEÇİLİ") : TEXT("UYGULA")),
						bSelected ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
						FOnClicked::CreateLambda([this, Level, Title]()
						{
							if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
							{
								Settings->SetOverallScalabilityLevel(Level);
								Settings->SetResolutionScaleValueEx(100.f);
								Settings->ApplySettings(false);
								ActionFeedback = FString::Printf(TEXT("Grafik ön ayarı uygulandı: %s (sg.* = %d)"), Title, Level);
								Refresh();
							}
							return FReply::Handled();
						}))
				]
			];
	};

	auto MakeToggleRow = [&](const TCHAR* Title, const FString& Sub, const TCHAR* OptA, bool bAOn, FOnClicked OnA,
		const TCHAR* OptB, bool bBOn, FOnClicked OnB, bool bEnabled = true) -> TSharedRef<SWidget>
	{
		TSharedRef<SVerticalBox> Labels = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(BNUITheme::Bone)];
		if (!Sub.IsEmpty())
		{
			Labels->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Sub)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::Muted)];
		}
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Labels]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(OptA), bAOn ? EBNButtonVariant::ConfirmGreen : EBNButtonVariant::SecondarySteel, OnA, bEnabled)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BNUITheme::MakeMWButton(FText::FromString(OptB), bBOn ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel, OnB, bEnabled)
		];
	};

	auto MakeVolumeRow = [&](const TCHAR* Label, float CurrentVal, TFunction<void(float)> ApplyFn) -> TSharedRef<SWidget>
	{
		const float Steps[] = { 0.f, 0.25f, 0.50f, 0.75f, 1.00f };
		const TCHAR* Names[] = { TEXT("%0"), TEXT("%25"), TEXT("%50"), TEXT("%75"), TEXT("%100") };
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < UE_ARRAY_COUNT(Steps); ++i)
		{
			const float Val = Steps[i];
			const TCHAR* Txt = Names[i];
			const bool bActive = FMath::Abs(CurrentVal - Val) < 0.12f;
			Row->AddSlot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, i < 4 ? 5.f : 0.f, 0.f))
			[
				BNUITheme::MakeMWButton(FText::FromString(Txt),
					bActive ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
					FOnClicked::CreateLambda([this, ApplyFn, Val, Label, Txt]()
					{
						ApplyFn(Val);
						ActionFeedback = FString::Printf(TEXT("%s seviyesi %s olarak ayarlandı."), Label, Txt);
						Refresh();
						return FReply::Handled();
					}), true, 9, FMargin(6.f, 6.f))
			];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Bone)]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%%%d"), FMath::RoundToInt(CurrentVal * 100.f)))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Nos)]
			]
			+ SVerticalBox::Slot().AutoHeight()[Row];
	};

	auto SetVSyncAction = [this](bool bEnable)
	{
		return FOnClicked::CreateLambda([this, bEnable]()
		{
			if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
			{
				Settings->SetVSyncEnabled(bEnable);
				Settings->ApplySettings(false);
				ActionFeedback = bEnable ? TEXT("VSync açıldı.") : TEXT("VSync kapatıldı.");
				Refresh();
			}
			return FReply::Handled();
		});
	};

	auto SetWindowModeAction = [this](EWindowMode::Type Mode, const TCHAR* Msg)
	{
		return FOnClicked::CreateLambda([this, Mode, Msg]()
		{
			if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
			{
				Settings->SetFullscreenMode(Mode);
				Settings->ApplySettings(false);
				ActionFeedback = Msg;
				Refresh();
			}
			return FReply::Handled();
		});
	};

	auto SetTrafficAction = [this](bool bEnable)
	{
		return FOnClicked::CreateLambda([this, bEnable]()
		{
			if (UWorld* W = GetHUDWorld(OwnerHUD))
			{
				if (UBNTrafficSubsystem* Tr = W->GetSubsystem<UBNTrafficSubsystem>())
				{
					Tr->SetTrafficEnabled(bEnable);
					ActionFeedback = bEnable ? TEXT("Karamara sivil trafiği açıldı.") : TEXT("Karamara sivil trafiği kapatıldı.");
					Refresh();
				}
			}
			return FReply::Handled();
		});
	};

	TSharedRef<SVerticalBox> LeftCol = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("GRAFİK KALİTE ÖN AYARLARI"), TEXT("RTX 3050 Ti — 4 GB VRAM"), BNUITheme::Accent)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[MakePresetCard(1, TEXT("PERFORMANS (Düşük — ~180 FPS)"), TEXT("sg.* = 1  •  Maksimum akıcılık ve en düşük GPU yükü"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[MakePresetCard(2, TEXT("DENGELİ (RTX 3050 Ti Önerilen — ~90 FPS)"), TEXT("sg.* = 2  •  Net gölgeler, yansımalar ve kararlı VRAM dengesi"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[MakePresetCard(3, TEXT("YÜKSEK (~60+ FPS)"), TEXT("sg.* = 3  •  Lumen/RT/VSM kapalı kalarak 4 GB VRAM güvenliğini korur"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[BNUITheme::MakeSectionHeader(TEXT("GÖRÜNTÜ MODU & VSYNC"), FString(), BNUITheme::Nos)]
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::Panel).Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				MakeToggleRow(TEXT("VSync (Dikey Eşitleme)"), FString(), TEXT("AÇIK"), bVSyncOn, SetVSyncAction(true), TEXT("KAPALI"), !bVSyncOn, SetVSyncAction(false))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeToggleRow(TEXT("Ekran Modu"), FString(),
					TEXT("TAM EKRAN"), !bWindowed, SetWindowModeAction(EWindowMode::WindowedFullscreen, TEXT("Çerçevesiz Tam Ekran modu uygulandı.")),
					TEXT("PENCERE"), bWindowed, SetWindowModeAction(EWindowMode::Windowed, TEXT("Pencere modu uygulandı.")))
			]
		]
	];

	const FString TrafficSub = Traffic
		? FString::Printf(TEXT("Durum: %s  (%d aktif sivil araç)"), bTrafficOn ? TEXT("Açık") : TEXT("Kapalı"), Traffic->GetActiveTrafficCount())
		: TEXT("Sivil trafik yalnızca Karamara Şehrinde aktiftir.");

	UBNProgressionSubsystem* Prog = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const EBNDifficulty CurDiff = Prog ? Prog->GetProfile().Difficulty : EBNDifficulty::StreetAutoTier;
	TSharedRef<SHorizontalBox> DiffRow = SNew(SHorizontalBox);
	for (int32 d = 0; d < static_cast<int32>(EBNDifficulty::Count); ++d)
	{
		const EBNDifficulty DiffVal = static_cast<EBNDifficulty>(d);
		DiffRow->AddSlot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, d < 2 ? 5.f : 0.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(FBNVehicleSpec::DifficultyLabel(DiffVal)),
				CurDiff == DiffVal ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this, DiffVal]()
				{
					if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
					{
						Pr->SetDifficulty(DiffVal);
						ActionFeedback = FString::Printf(TEXT("Rakip zorluğu: %s"), FBNVehicleSpec::DifficultyLabel(DiffVal));
						Refresh();
					}
					return FReply::Handled();
				}), true, 9, FMargin(6.f, 7.f))
		];
	}

	TSharedRef<SVerticalBox> RightCol = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("ŞEHİR, TRAFİK & ZORLUK"), FString(), BNUITheme::Accent)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::Panel).Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				MakeToggleRow(TEXT("Sivil Trafik"), TrafficSub, TEXT("AÇIK"), bTrafficOn, SetTrafficAction(true), TEXT("KAPALI"), !bTrafficOn, SetTrafficAction(false), Traffic != nullptr)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Serbest Yarış Rakip Zorluğu (Oto-Tier Ölçekleme)"))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Bone)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[DiffRow]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("YARIŞ İÇİ POLİS MÜDAHALESİ: HER ZAMAN AÇIK (Karamara Kuralı — Kapatılamaz)")))
				.Font(BNUITheme::Font(9, true))
				.ColorAndOpacity(BNUITheme::Danger)
				.AutoWrapText(true)
			]
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("SES MİKSERİ"), TEXT("MOTOR • MÜZİK • EFEKT"), BNUITheme::Nos)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::Panel).Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				MakeVolumeRow(TEXT("Motor, Lastik & Turbo Sesi"), CurrentEngineGain, [this](float Gain)
				{
					ABNHUD::SetPreferredEngineGain(Gain);
					if (ABNVehiclePawn* CurCar = GetPlayerCar(OwnerHUD); CurCar && CurCar->EngineAudio)
					{
						CurCar->EngineAudio->MasterGain = Gain;
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				MakeVolumeRow(TEXT("Dinamik Müzik"), CurrentMusicVol, [](float Vol)
				{
					UBNAudioSubsystem::SetMusicVolume(Vol);
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeVolumeRow(TEXT("Çarpışma, Telsiz & Arayüz Efektleri (SFX)"), CurrentSfxVol, [](float Vol)
				{
					UBNAudioSubsystem::SetSfxVolume(Vol);
				})
			]
		]
	];

	if (!ActionFeedback.IsEmpty())
	{
		RightCol->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(ActionFeedback)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(BNUITheme::Ok).AutoWrapText(true)
		];
	}

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().FillWidth(0.54f).Padding(FMargin(0.f, 0.f, 16.f, 0.f))[SNew(SScrollBox) + SScrollBox::Slot()[LeftCol]]
	+ SHorizontalBox::Slot().FillWidth(0.46f)[SNew(SScrollBox) + SScrollBox::Slot()[RightCol]];
}

void SBNStoryCardWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(40.f, 0.f, 40.f, 42.f))
		[
			SNew(SBox).WidthOverride(780.f)[SAssignNew(CardSlot, SVerticalBox)]
		]
	];

	Refresh();
}

void SBNStoryCardWidget::Refresh()
{
	if (!CardSlot.IsValid())
	{
		return;
	}
	CardSlot->ClearChildren();

	UGameInstance* GI = SBNGameMenuWidget::GetGI(OwnerHUD);
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const FBNStoryCard* Card = Story ? Story->GetActiveCard() : nullptr;
	if (!Card)
	{
		return;
	}

	const FString BtnLabel = Story->GetQueuedCardCount() > 1
		? FString::Printf(TEXT("DEVAM ET [ENTER / ESC]  (%d mesaj)"), Story->GetQueuedCardCount())
		: FString(TEXT("DEVAM ET [ENTER / ESC]"));

	CardSlot->AddSlot().AutoHeight()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeAccentRule(6.f)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(BNUITheme::Panel)
			.Padding(FMargin(24.f, 18.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
					[
						SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::Accent).Padding(FMargin(9.f, 3.f))
						[
							SNew(STextBlock).Text(FText::FromString(Card->Speaker.ToUpper())).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::InkSolid)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Card->Title.ToUpper())).Font(BNUITheme::Font(13, true)).ColorAndOpacity(BNUITheme::AccentHi)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 14.f))
				[
					SNew(STextBlock).Text(FText::FromString(Card->Body)).Font(BNUITheme::Font(12, false)).ColorAndOpacity(BNUITheme::Bone).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					BNUITheme::MakeMWButton(FText::FromString(BtnLabel), EBNButtonVariant::PrimaryAmber,
						FOnClicked::CreateLambda([this]()
						{
							if (OwnerHUD.IsValid())
							{
								OwnerHUD->HandleMenuCloseOrAdvanceCard();
							}
							return FReply::Handled();
						}))
				]
			]
		]
	];
}
