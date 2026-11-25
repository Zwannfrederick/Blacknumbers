#include "UI/BNMenuWidget.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "GenericPlatform/GenericWindow.h"
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
	if (!Story)
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Hikâye alt sistemi bulunamadı.")));
	}

	const FBNStoryState& S = Story->GetState();
	UWorld* World = GetHUDWorld(OwnerHUD);
	TSharedRef<SVerticalBox> ProgramRows = SNew(SVerticalBox);
	if (SelectedBlacklistRank < 1 || SelectedBlacklistRank > 16)
	{
		SelectedBlacklistRank = FMath::Clamp(S.NextBossRank, 1, 16);
	}
	const FBNBossSpec* ActiveBoss = FBNStoryState::FindBossByRank(SelectedBlacklistRank);
	if (!ActiveBoss)
	{
		ActiveBoss = FBNStoryState::FindBossByRank(16);
	}

	// Left Rail: Clickable Most Wanted Blacklist #16 .. #01
	TSharedRef<SVerticalBox> RosterRows = SNew(SVerticalBox);
	for (const FBNBossSpec& B : FBNStoryState::GetAllBosses())
	{
		const bool bDefeated = S.IsBossDefeated(B.Rank);
		const bool bNext = !bDefeated && S.NextBossRank == B.Rank && !S.bFinalChaseActive && !S.bCampaignCompleted;
		const bool bSelected = (B.Rank == SelectedBlacklistRank);
		const bool bRuler = FBNStoryState::IsDistrictRulerRank(B.Rank);
		const FBNDistrictTerritoryInfo BossTerr = S.GetDistrictTerritory(B.HomeDistrict);
		const FString Badge = bDefeated ? TEXT("YENİLDİ") : bNext ? TEXT("SIRADAKİ") : (bRuler ? TEXT("★ HÜKÜMDAR") : TEXT("KİLİTLİ"));
		const FLinearColor BadgeCol = bDefeated ? BNUITheme::CrimsonRed : bNext ? BNUITheme::AmberBright : (bRuler ? BNUITheme::NosCyan : BNUITheme::SteelMuted);
		const FLinearColor RowBg = bSelected ? BNUITheme::AmberDarkCard
			: bDefeated ? FLinearColor(0.08f, 0.065f, 0.068f, 0.94f)
			: BNUITheme::GunmetalCard;
		const FString SubInfo = FString::Printf(TEXT("%s  •  %s"), *B.VehicleName, *BossTerr.BoroughTitle);

		RosterRows->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 6.f, 5.f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateLambda([this, Rank = B.Rank]()
			{
				UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiClick, 0.75f);
				SelectedBlacklistRank = Rank;
				Refresh();
				return FReply::Handled();
			}))
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(RowBg)
				.Padding(FMargin(0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(4.f)[SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(bSelected ? BNUITheme::AmberBright : BadgeCol)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 7.f, 8.f, 7.f))
					[
						SNew(SBox).WidthOverride(42.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("#%02d"), B.Rank)))
							.Font(BNUITheme::Font(15, true))
							.ColorAndOpacity(bSelected || bNext ? BNUITheme::AmberGold : bDefeated ? BNUITheme::CrimsonRed : BNUITheme::SteelMuted)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f, 6.f, 8.f, 6.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(B.Name)).Font(BNUITheme::Font(11, true))
							.ColorAndOpacity(bSelected ? BNUITheme::ChalkWhite : bDefeated ? BNUITheme::SteelMuted : BNUITheme::ChalkWhite)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(SubInfo)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
					[
						SNew(STextBlock).Text(FText::FromString(Badge)).Font(BNUITheme::Font(9, true)).ColorAndOpacity(BadgeCol)
					]
				]
			]
		];
	}

	// Right Hero Dossier Card with real imported concept artwork backdrop!
	TSharedRef<SVerticalBox> RightPane = SNew(SVerticalBox);
	if (ActiveBoss)
	{
		const bool bDef = S.IsBossDefeated(ActiveBoss->Rank);
		const bool bCanFight = S.CanChallengeBoss(ActiveBoss->Rank);
		const bool bDuelReady = S.IsDuelUnlocked(ActiveBoss->Rank);
		const bool bFinalChase = S.bFinalChaseActive && ActiveBoss->Rank == 1;
		const int32 QualifierCount = ActiveBoss->Qualifiers.Num();
		const int32 Wins = bDef ? QualifierCount : bCanFight ? S.QualifiersWon : 0;
		const FSlateBrush* ArtBrush = BNUITheme::GetArtBrush(ArtForBlacklistRank(ActiveBoss->Rank));
		const FSlateBrush* VigLeft = BNUITheme::GetVignetteLeftBrush();
		const bool bHasArt = ArtBrush != BNUITheme::WhiteBrush();
		const FString StampText = bDef ? TEXT("YENİLDİ  //  DOSYA KAPANDI")
			: bDuelReady ? TEXT("DÜELLO AÇIK") : bCanFight ? TEXT("ELEME PROGRAMI")
			: FString::Printf(TEXT("KİLİTLİ  —  ÖNCE #%02d YENİLMELİ"), S.NextBossRank);
		const FLinearColor StampCol = bDef ? BNUITheme::CrimsonRed : bCanFight ? BNUITheme::AmberBright : BNUITheme::SteelMuted;

		RightPane->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(186.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::AsphaltDark)
					]
					+ SOverlay::Slot().HAlign(HAlign_Right)
					[
						SNew(SBox).WidthOverride(640.f).Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SScaleBox).Stretch(EStretch::ScaleToFill).VAlign(VAlign_Bottom)
								[
									SNew(SImage).Image(ArtBrush)
									.ColorAndOpacity(bHasArt ? FLinearColor(0.95f, 0.92f, 0.88f, 1.f) : BNUITheme::AsphaltDark)
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SImage).Image(VigLeft)
								.ColorAndOpacity(VigLeft != BNUITheme::WhiteBrush() ? FLinearColor::White : FLinearColor(0.03f, 0.035f, 0.045f, 0.85f))
							]
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 18.f, 8.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("#%02d"), ActiveBoss->Rank)))
						.Font(BNUITheme::Font(48, true))
						.ColorAndOpacity(FLinearColor(0.95f, 0.66f, 0.08f, 0.35f))
					]
					+ SOverlay::Slot().Padding(FMargin(20.f, 16.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("BLACK NUMBERS #%02d   •   %s"), ActiveBoss->Rank, *StampText)))
							.Font(BNUITheme::Font(10, true))
							.ColorAndOpacity(StampCol)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 2.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(ActiveBoss->Name.ToUpper()))
							.Font(BNUITheme::Font(24, true))
							.ColorAndOpacity(BNUITheme::ChalkWhite)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%s   //   %s"), *ActiveBoss->VehicleName, *ActiveBoss->OrgRole)))
							.Font(BNUITheme::Font(11, true))
							.ColorAndOpacity(BNUITheme::AmberGold)
						]
						+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 14.f, 0.f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("Stil: %s   •   %s"), *ActiveBoss->DrivingStyle, *ActiveBoss->Personality)))
								.Font(BNUITheme::Font(9, false))
								.ColorAndOpacity(BNUITheme::ChalkWhite)
								.AutoWrapText(true)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
							[
								BNUITheme::MakeMWButton(
									FText::FromString(bFinalChase ? TEXT("SON KOVALAMACA")
										: bDef ? TEXT("YENİLDİ") : bDuelReady ? TEXT("DÜELLOYA ÇIK") : TEXT("SIRADAKİ YARIŞ")),
									bDef ? EBNButtonVariant::DangerCrimson : EBNButtonVariant::PrimaryAmber,
									FOnClicked::CreateLambda([this, BossRank = ActiveBoss->Rank]()
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
									}), bCanFight || bFinalChase, 12, FMargin(18.f, 10.f))
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeHazardBar(6.f)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(BNUITheme::GunmetalCard)
				.Padding(FMargin(16.f, 9.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("BÖLGE: %s%s   |   ÖDÜL: $%s + %d İTİBAR   |   KANIT: %s"),
						*S.GetDistrictTerritory(ActiveBoss->HomeDistrict).BoroughTitle,
						FBNStoryState::IsDistrictRulerRank(ActiveBoss->Rank) ? TEXT(" (★ HÜKÜMDAR)") : TEXT(""),
						*FString::FormatAsNumber(ActiveBoss->RewardCash), ActiveBoss->RewardRep,
						ActiveBoss->EvidenceCode.IsEmpty() ? TEXT("Sokak İstihbaratı") : *ActiveBoss->EvidenceCode)))
					.Font(BNUITheme::Font(10, true))
					.ColorAndOpacity(BNUITheme::NosCyan)
				]
			]
		];

		ProgramRows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			BNUITheme::MakeSectionHeader(TEXT("SOKAK YARIŞI PROGRAMI"),
				FString::Printf(TEXT("%d/%d TAMAM"), Wins + (bDef ? 1 : 0), QualifierCount + 1), BNUITheme::AmberGold)
		];
		for (int32 Index = 0; Index <= QualifierCount; ++Index)
		{
			const bool bDuel = Index == QualifierCount;
			const bool bDone = bDef || Index < Wins;
			const bool bNext = bCanFight && Index == Wins;
			const FName Asset = bDuel ? FName(*ActiveBoss->RaceAssetName) : ActiveBoss->Qualifiers[Index];
			const ABNRaceEvent* Event = UBNStorySubsystem::FindProgramRace(World, Asset);
			const FString RaceName = Event && Event->Definition ? Event->Definition->DisplayName.ToString()
				: bDuel ? ActiveBoss->DisciplineLabel : TEXT("Karamara sokak yarışı");
			const bool bDrift = Event && Event->Definition && Event->Definition->Discipline == EBNRaceDiscipline::Drift;
			const FString Objective = bDrift ? FString::Printf(TEXT("%s PUAN"),
				*FString::FormatAsNumber(bDuel ? ActiveBoss->DriftTargetScore : S.GetQualifierDriftTarget()))
				: TEXT("1. OL");
			ProgramRows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SBorder).BorderImage(WhiteBrush())
				.BorderBackgroundColor(bNext ? BNUITheme::AmberDarkCard : BNUITheme::GunmetalCard).Padding(10.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(85.f)
						[
							SNew(STextBlock).Text(FText::FromString(bDuel ? TEXT("DÜELLO") : FString::Printf(TEXT("ELEME %d"), Index + 1)))
							.Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::AmberGold)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(RaceName)).Font(BNUITheme::Font(11, true))
							.ColorAndOpacity(BNUITheme::ChalkWhite).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
					[
						SNew(STextBlock).Text(FText::FromString(bDone ? TEXT("TAMAM") : bNext ? Objective : TEXT("KİLİTLİ")))
							.Font(BNUITheme::Font(9, true)).ColorAndOpacity(bDone ? BNUITheme::EmeraldGreen : BNUITheme::SteelMuted)
					]
				]
			];
		}
	}

	// Evidence Ledger + Tarik Kaya & Heat Status (Kanon-accurate: K. Vance is Kerem's alias, never a police rank!)
	ProgramRows->AddSlot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 8.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("SORUŞTURMA & BÖLGE HAKİMİYETİ"),
			FString::Printf(TEXT("BÖLGE: %d/6  •  TARIK: %s  •  %d/16"), S.GetLiberatedDistrictCount(), FBNStoryState::TarikAttitudeLabel(S.TarikAttitude), S.GetDefeatedCount()),
			BNUITheme::NosCyan)
	];

	TSharedRef<SVerticalBox> EvidenceList = SNew(SVerticalBox);
	EvidenceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 6.f, 6.f))
	[
		SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BNUITheme::GunmetalCard).Padding(FMargin(12.f, 8.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Polis Heat Limiti: %s"),
				S.GetMaxAllowedHeat() >= 11 ? TEXT("Özel Kademe (Başkomiser Demir: Ölü ya da Diri)")
					: S.GetMaxAllowedHeat() >= 6 ? TEXT("Heat 6-10 (Başkomiser Demir devrede — Kerem Yalçın ifşa)")
					: TEXT("Heat 1-5 (K. Vance — Sahte Kimlik)"))))
			.Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::AmberGold)
		]
	];
	for (const FString& Note : S.UnlockedEvidenceNotes)
	{
		EvidenceList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 6.f, 6.f))
		[
			SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(12.f, 8.f))
			[
				SNew(STextBlock).Text(FText::FromString(Note)).Font(BNUITheme::Font(10, false)).ColorAndOpacity(BNUITheme::ChalkWhite).AutoWrapText(true)
			]
		];
	}
	RightPane->AddSlot().FillHeight(1.f)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()[ProgramRows]
		+ SScrollBox::Slot()[EvidenceList]
	];

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 16.f, 0.f))
	[
		SNew(SBox).WidthOverride(348.f)[SNew(SScrollBox) + SScrollBox::Slot()[RosterRows]]
	]
	+ SHorizontalBox::Slot().FillWidth(1.f)[RightPane];
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
			.BorderImage(BNUITheme::GetPanelBevelBrush())
			.BorderBackgroundColor(bSelected ? FLinearColor(1.f, 0.88f, 0.65f, 0.98f) : FLinearColor(0.92f, 0.92f, 0.92f, 0.95f))
			.Padding(FMargin(14.f, 10.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(bSelected ? BNUITheme::AmberBright : BNUITheme::ChalkWhite)]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))[SNew(STextBlock).Text(FText::FromString(Desc)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)]
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
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(BNUITheme::ChalkWhite)];
		if (!Sub.IsEmpty())
		{
			Labels->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Sub)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)];
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
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::ChalkWhite)]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%%%d"), FMath::RoundToInt(CurrentVal * 100.f)))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::NosCyan)]
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
		BNUITheme::MakeSectionHeader(TEXT("GRAFİK KALİTE ÖN AYARLARI"), TEXT("RTX 3050 Ti — 4 GB VRAM"), BNUITheme::AmberGold)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[MakePresetCard(1, TEXT("PERFORMANS (Düşük — ~180 FPS)"), TEXT("sg.* = 1  •  Maksimum akıcılık ve en düşük GPU yükü"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[MakePresetCard(2, TEXT("DENGELİ (RTX 3050 Ti Önerilen — ~90 FPS)"), TEXT("sg.* = 2  •  Net gölgeler, yansımalar ve kararlı VRAM dengesi"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[MakePresetCard(3, TEXT("YÜKSEK (~60+ FPS)"), TEXT("sg.* = 3  •  Lumen/RT/VSM kapalı kalarak 4 GB VRAM güvenliğini korur"))]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[BNUITheme::MakeSectionHeader(TEXT("GÖRÜNTÜ MODU & VSYNC"), FString(), BNUITheme::NosCyan)]
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(14.f, 12.f))
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
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("ŞEHİR, TRAFİK & ZORLUK"), FString(), BNUITheme::AmberGold)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			[
				MakeToggleRow(TEXT("Sivil Trafik"), TrafficSub, TEXT("AÇIK"), bTrafficOn, SetTrafficAction(true), TEXT("KAPALI"), !bTrafficOn, SetTrafficAction(false), Traffic != nullptr)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Serbest Yarış Rakip Zorluğu (Oto-Tier Ölçekleme)"))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::ChalkWhite)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[DiffRow]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("YARIŞ İÇİ POLİS MÜDAHALESİ: HER ZAMAN AÇIK (Karamara Kuralı — Kapatılamaz)")))
				.Font(BNUITheme::Font(9, true))
				.ColorAndOpacity(BNUITheme::CrimsonRed)
				.AutoWrapText(true)
			]
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("SES MİKSERİ"), TEXT("MOTOR • MÜZİK • EFEKT"), BNUITheme::NosCyan)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(14.f, 12.f))
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
			SNew(STextBlock).Text(FText::FromString(ActionFeedback)).Font(BNUITheme::Font(11, true)).ColorAndOpacity(BNUITheme::EmeraldGreen).AutoWrapText(true)
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
		+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeHazardBar(6.f)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::GetPanelBevelBrush())
			.BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.96f))
			.Padding(FMargin(24.f, 18.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
					[
						SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(BNUITheme::AmberGold).Padding(FMargin(9.f, 3.f))
						[
							SNew(STextBlock).Text(FText::FromString(Card->Speaker.ToUpper())).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::ObsidianInk)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Card->Title.ToUpper())).Font(BNUITheme::Font(13, true)).ColorAndOpacity(BNUITheme::AmberBright)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 14.f))
				[
					SNew(STextBlock).Text(FText::FromString(Card->Body)).Font(BNUITheme::Font(12, false)).ColorAndOpacity(BNUITheme::ChalkWhite).AutoWrapText(true)
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
