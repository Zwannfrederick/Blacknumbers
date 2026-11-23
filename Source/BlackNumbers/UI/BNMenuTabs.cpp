#include "UI/BNMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "GenericPlatform/GenericWindow.h"
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

TSharedRef<SWidget> SBNGameMenuWidget::BuildGarageTab()
{
	UGameInstance* GI = GetGI(OwnerHUD);
	UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	if (!Prog)
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Garaj alt sistemi bulunamadı.")));
	}

	const FBNPlayerProfile& P = Prog->GetProfile();
	const EBNCarId ShownCar = PreviewedCar.Get(P.ActiveCar);
	const int32 ShownIdx = FMath::Clamp(static_cast<int32>(ShownCar), 0, FBNUpgradeSpec::NumCars - 1);
	const FBNVehicleSpec ActiveSpec = FBNVehicleSpec::ForCar(P.ActiveCar);
	const FBNVehicleSpec ShownSpec = FBNVehicleSpec::ForCar(ShownCar);
	const FBNComputedVehicleStats ShownStats = FBNComputedVehicleStats::Compute(ShownCar, P.Upgrades[ShownIdx]);
	const bool bPreviewingOther = ShownCar != P.ActiveCar;

	// Left Rail: Performance Bars + 5-Slot Tier 1-6 Upgrades
	TSharedRef<SVerticalBox> UpgradeRows = SNew(SVerticalBox);
	for (int32 s = 0; s < FBNUpgradeSpec::NumSlots; ++s)
	{
		const EBNUpgradeSlot Slot = static_cast<EBNUpgradeSlot>(s);
		const int32 Tier = P.GetUpgradeTier(Slot);
		const int32 MaxTier = P.GetMaxAllowedUpgradeTier();
		const int32 Cost = Tier < 6 ? FBNUpgradeSpec::CostForTier(Tier + 1) : 0;

		TSharedRef<SHorizontalBox> TierPips = SNew(SHorizontalBox);
		for (int32 k = 1; k <= 6; ++k)
		{
			const bool bOwnedPip = k <= Tier;
			const bool bLockedT6 = k == 6 && !P.bTier6Unlocked;
			const FLinearColor PipCol = bOwnedPip ? BNUITheme::AmberGold
				: bLockedT6 ? FLinearColor(0.45f, 0.16f, 0.14f, 0.80f)
				: FLinearColor(0.20f, 0.21f, 0.24f, 0.90f);
			TierPips->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				SNew(SBox).WidthOverride(16.f).HeightOverride(6.f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PipCol)
				]
			];
		}

		const bool bCanUpgrade = Tier < MaxTier && P.Cash >= Cost;
		const FString BtnText = Tier >= 6 ? TEXT("MAKS T6")
			: (Tier == 5 && !P.bTier6Unlocked) ? TEXT("T6 KİLİTLİ")
			: FString::Printf(TEXT("+$%s"), *FString::FormatAsNumber(Cost));
		const EBNButtonVariant BtnVar = (Tier == 5 && !P.bTier6Unlocked) ? EBNButtonVariant::DangerCrimson
			: bCanUpgrade ? EBNButtonVariant::ConfirmGreen
			: EBNButtonVariant::SecondarySteel;

		UpgradeRows->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 4.f, 7.f))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(BNUITheme::GunmetalCard)
			.Padding(FMargin(11.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s   [T%d/6]"), FBNUpgradeSpec::SlotLabel(Slot), Tier)))
						.Font(BNUITheme::Font(10, true))
						.ColorAndOpacity(BNUITheme::ChalkWhite)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 0.f))
					[
						TierPips
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					BNUITheme::MakeMWButton(FText::FromString(BtnText), BtnVar,
						FOnClicked::CreateLambda([this, Slot]()
						{
							if (UGameInstance* CurGI = GetGI(OwnerHUD))
							{
								if (UBNProgressionSubsystem* Pr = CurGI->GetSubsystem<UBNProgressionSubsystem>())
								{
									Pr->TryBuyUpgrade(Slot, GetPlayerCar(OwnerHUD));
									PreviewedCar.Reset();
									ActionFeedback = Pr->GetFeedbackText();
									Refresh();
								}
							}
							return FReply::Handled();
						}), bCanUpgrade, 10, FMargin(11.f, 7.f))
				]
			]
		];
	}

	TSharedRef<SWidget> LeftRail = SNew(SBox)
		.WidthOverride(368.f)
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::GetPanelBevelBrush())
			.BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.88f))
			.Padding(FMargin(16.f, 14.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					BNUITheme::MakeSectionHeader(
						bPreviewingOther ? TEXT("ÖNİZLEME VERİLERİ") : TEXT("ARAÇ PERFORMANSI"),
						FBNVehicleSpec::BodyStyleLabel(ShownSpec.BodyStyle),
						bPreviewingOther ? BNUITheme::AmberGold : BNUITheme::NosCyan)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(STextBlock).Text(FText::FromString(ShownSpec.Subtitle)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("MOTOR TORKU"), FString::Printf(TEXT("%.0f Nm"), ShownStats.MaxTorqueNm), ShownStats.MaxTorqueNm / 880.f, BNUITheme::AmberGold)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("MAKS DEVİR"), FString::Printf(TEXT("%.0f RPM"), ShownStats.MaxRpm), ShownStats.MaxRpm / 9500.f, BNUITheme::EmeraldGreen)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("LASTİK TUTUŞU"), FString::Printf(TEXT("%.2fx"), ShownStats.TireGripScale), ShownStats.TireGripScale / 1.50f, BNUITheme::NosCyan)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("NOS MAKS HIZ"), FString::Printf(TEXT("%.0f km/h"), ShownStats.NosMaxSpeedKmh), ShownStats.NosMaxSpeedKmh / 360.f, BNUITheme::NosCyan)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 8.f))
				[
					BNUITheme::MakeSectionHeader(TEXT("PARÇA MODİFİYESİ"), ActiveSpec.DisplayName, BNUITheme::AmberGold)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[UpgradeRows]
				]
			]
		];

	// Center open viewport: completely clear for live 360-degree 3D car turntable + bottom nameplate!
	TSharedRef<SVerticalBox> CenterShowcase = SNew(SVerticalBox)
	+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
	+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeHazardBar(4.f)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::GetPanelBevelBrush())
			.BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.88f))
			.Padding(FMargin(22.f, 10.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s   //   %s"), *ShownSpec.DisplayName.ToUpper(), FBNVehicleSpec::BodyStyleLabel(ShownSpec.BodyStyle))))
					.Font(BNUITheme::Font(15, true))
					.ColorAndOpacity(BNUITheme::AmberBright)
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(ActionFeedback.IsEmpty()
						? FString(TEXT("SELİN'İN GARAJI  •  360° CANLI 3D ARAÇ VİTRİNİ"))
						: ActionFeedback))
					.Font(BNUITheme::Font(10, !ActionFeedback.IsEmpty()))
					.ColorAndOpacity(ActionFeedback.IsEmpty() ? BNUITheme::SteelMuted : BNUITheme::EmeraldGreen)
					.Justification(ETextJustify::Center)
				]
			]
		]
	];

	// Right Rail: 6-Vehicle Showroom Selector
	TSharedRef<SVerticalBox> CarList = SNew(SVerticalBox);
	for (int32 c = 0; c < FBNUpgradeSpec::NumCars; ++c)
	{
		const EBNCarId CarId = static_cast<EBNCarId>(c);
		const FBNVehicleSpec Spec = FBNVehicleSpec::ForCar(CarId);
		const bool bOwned = P.IsCarOwned(CarId);
		const bool bActive = P.ActiveCar == CarId;
		const bool bPreviewed = ShownCar == CarId;
		const bool bCanAfford = bOwned || P.Cash >= Spec.PurchaseCost;

		const FString BtnLabel = bActive ? TEXT("AKTİF")
			: bOwned ? TEXT("SEÇ")
			: FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(Spec.PurchaseCost));
		const EBNButtonVariant BtnVar = bActive ? EBNButtonVariant::IdleTab
			: bOwned ? EBNButtonVariant::CyanIntel
			: bCanAfford ? EBNButtonVariant::ConfirmGreen
			: EBNButtonVariant::SecondarySteel;
		const FString SpecLine = FString::Printf(TEXT("%s • %.0f Nm • %.2fx"),
			FBNVehicleSpec::BodyStyleLabel(Spec.BodyStyle), Spec.BaseTorqueNm, Spec.BaseTireGripScale);

		TSharedRef<SHorizontalBox> BtnGroup = SNew(SHorizontalBox);
		if (!bActive)
		{
			BtnGroup->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
			[
				BNUITheme::MakeMWButton(FText::FromString(bPreviewed ? TEXT("VİTRİN") : TEXT("GÖR")),
					bPreviewed ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
					FOnClicked::CreateLambda([this, CarId, c]()
					{
						PreviewedCar = CarId;
						if (ABNVehiclePawn* CurCar = GetPlayerCar(OwnerHUD))
						{
							if (const UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
							{
								CurCar->ApplyVehicleSpec(CarId, FBNComputedVehicleStats::Compute(CarId, Pr->GetProfile().Upgrades[c]));
							}
						}
						ActionFeedback = FString::Printf(TEXT("%s canlı 3D vitrinde."), *FBNVehicleSpec::ForCar(CarId).DisplayName);
						Refresh();
						return FReply::Handled();
					}), true, 9, FMargin(9.f, 7.f))
			];
		}
		BtnGroup->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			BNUITheme::MakeMWButton(FText::FromString(BtnLabel), BtnVar,
				FOnClicked::CreateLambda([this, CarId]()
				{
					if (UGameInstance* CurGI = GetGI(OwnerHUD))
					{
						if (UBNProgressionSubsystem* Pr = CurGI->GetSubsystem<UBNProgressionSubsystem>())
						{
							Pr->TryBuyOrSelectCar(CarId, GetPlayerCar(OwnerHUD));
							PreviewedCar.Reset();
							ActionFeedback = Pr->GetFeedbackText();
							Refresh();
						}
					}
					return FReply::Handled();
				}), !bActive && bCanAfford, 9, FMargin(10.f, 7.f))
		];

		CarList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 4.f, 7.f))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor((bActive || bPreviewed) ? BNUITheme::AmberDarkCard : BNUITheme::GunmetalCard)
			.Padding(FMargin(0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(6.f)[SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Spec.PaintTint)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(10.f, 8.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(Spec.DisplayName)).Font(BNUITheme::Font(11, true))
							.ColorAndOpacity(bActive ? BNUITheme::AmberBright : BNUITheme::ChalkWhite)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
						[
							SNew(STextBlock).Text(FText::FromString(SpecLine)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BtnGroup]
				]
			]
		];
	}

	TSharedRef<SWidget> RightRail = SNew(SBox)
		.WidthOverride(378.f)
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::GetPanelBevelBrush())
			.BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.88f))
			.Padding(FMargin(16.f, 14.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
				[
					BNUITheme::MakeSectionHeader(TEXT("ARAÇ KOLEKSİYONU"), FString::Printf(TEXT("%d ARAÇ"), FBNUpgradeSpec::NumCars), BNUITheme::AmberGold)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[CarList]]
			]
		];

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth()[LeftRail]
	+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(16.f, 0.f))[CenterShowcase]
	+ SHorizontalBox::Slot().AutoWidth()[RightRail];
}

TSharedRef<SWidget> SBNGameMenuWidget::BuildBlackNumbersTab()
{
	UGameInstance* GI = GetGI(OwnerHUD);
	const UBNStorySubsystem* Story = GI ? GI->GetSubsystem<UBNStorySubsystem>() : nullptr;
	if (!Story)
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Hikaye alt sistemi bulunamadı.")));
	}

	const FBNStoryState& S = Story->GetState();
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
		const FString Badge = bDefeated ? TEXT("YENİLDİ") : bNext ? TEXT("SIRADAKİ") : TEXT("KİLİTLİ");
		const FLinearColor BadgeCol = bDefeated ? BNUITheme::CrimsonRed : bNext ? BNUITheme::AmberBright : BNUITheme::SteelMuted;
		const FLinearColor RowBg = bSelected ? BNUITheme::AmberDarkCard
			: bDefeated ? FLinearColor(0.08f, 0.065f, 0.068f, 0.94f)
			: BNUITheme::GunmetalCard;

		RosterRows->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 6.f, 5.f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateLambda([this, Rank = B.Rank]()
			{
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
							SNew(STextBlock).Text(FText::FromString(B.VehicleName)).Font(BNUITheme::Font(9, false)).ColorAndOpacity(BNUITheme::SteelMuted)
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
		const FSlateBrush* ArtBrush = BNUITheme::GetArtBrush(ArtForBlacklistRank(ActiveBoss->Rank));
		const FSlateBrush* VigLeft = BNUITheme::GetVignetteLeftBrush();
		const bool bHasArt = ArtBrush != BNUITheme::WhiteBrush();
		const FString StampText = bDef ? TEXT("YENİLDİ  //  DOSYA KAPANDI")
			: bCanFight ? TEXT("MEYDAN OKUNABİLİR HEDEF")
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
						SNew(SImage)
						.Image(ArtBrush)
						.ColorAndOpacity(bHasArt ? FLinearColor(0.95f, 0.92f, 0.88f, 1.f) : BNUITheme::AsphaltDark)
					]
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(VigLeft)
						.ColorAndOpacity(VigLeft != BNUITheme::WhiteBrush() ? FLinearColor::White : FLinearColor(0.03f, 0.035f, 0.045f, 0.85f))
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
									FText::FromString(bDef ? TEXT("YENİLDİ") : TEXT("MEYDAN OKU")),
									bDef ? EBNButtonVariant::DangerCrimson : EBNButtonVariant::PrimaryAmber,
									FOnClicked::CreateLambda([this, BossRank = ActiveBoss->Rank]()
									{
										ABNVehiclePawn* Car = GetPlayerCar(OwnerHUD);
										UBNStorySubsystem* St = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNStorySubsystem>() : nullptr;
										if (Car && St && OwnerHUD.IsValid())
										{
											St->OpenDossier(Car);
											St->MoveDossierSelection((16 - BossRank) - St->GetSelectedBossIndex());
											OwnerHUD->CloseMenu();
											St->ChallengeSelectedBoss(Car);
										}
										return FReply::Handled();
									}), bCanFight, 12, FMargin(18.f, 10.f))
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
						TEXT("DİSİPLİN: %s   |   ÖDÜL: $%s + %d İTİBAR   |   KANIT: %s"),
						*ActiveBoss->DisciplineLabel, *FString::FormatAsNumber(ActiveBoss->RewardCash), ActiveBoss->RewardRep,
						ActiveBoss->EvidenceCode.IsEmpty() ? TEXT("Sokak İstihbaratı") : *ActiveBoss->EvidenceCode)))
					.Font(BNUITheme::Font(10, true))
					.ColorAndOpacity(BNUITheme::NosCyan)
				]
			]
		];
	}

	// Evidence Ledger + Tarik Kaya & Heat Status
	RightPane->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[
		BNUITheme::MakeSectionHeader(TEXT("SORUŞTURMA & KANIT DOSYASI"),
			FString::Printf(TEXT("TARIK: %s  •  %d/16"), FBNStoryState::TarikAttitudeLabel(S.TarikAttitude), S.GetDefeatedCount()),
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
					: S.GetMaxAllowedHeat() >= 6 ? TEXT("Heat 6-10 (Başkomiser Demir devrede)")
					: TEXT("Heat 1-5 (Komiser Vance)"))))
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
	RightPane->AddSlot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[EvidenceList]];

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
	const float CurrentGain = (Car && Car->EngineAudio) ? Car->EngineAudio->MasterGain : ABNHUD::GetPreferredEngineGain();

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

	auto MakeGainBtn = [&](const TCHAR* Label, float Gain) -> TSharedRef<SWidget>
	{
		const bool bActive = FMath::Abs(CurrentGain - Gain) < 0.12f;
		return BNUITheme::MakeMWButton(FText::FromString(Label), bActive ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
			FOnClicked::CreateLambda([this, Gain, Label]()
			{
				ABNHUD::SetPreferredEngineGain(Gain);
				if (ABNVehiclePawn* CurCar = GetPlayerCar(OwnerHUD); CurCar && CurCar->EngineAudio)
				{
					CurCar->EngineAudio->MasterGain = Gain;
				}
				ActionFeedback = FString::Printf(TEXT("Motor & efekt ses seviyesi %s olarak ayarlandı."), Label);
				Refresh();
				return FReply::Handled();
			}));
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

	TSharedRef<SVerticalBox> RightCol = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("ŞEHİR & SİVİL TRAFİK"), FString(), BNUITheme::AmberGold)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(14.f, 12.f))
		[
			MakeToggleRow(TEXT("Sivil Trafik"), TrafficSub, TEXT("AÇIK"), bTrafficOn, SetTrafficAction(true), TEXT("KAPALI"), !bTrafficOn, SetTrafficAction(false), Traffic != nullptr)
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[BNUITheme::MakeSectionHeader(TEXT("MOTOR & EFEKT SESİ"), FString::Printf(TEXT("%%%d"), FMath::RoundToInt(CurrentGain * 100.f)), BNUITheme::NosCyan)]
	+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		SNew(SBorder).BorderImage(BNUITheme::GetPanelBevelBrush()).BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.95f)).Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Motor, Lastik Kayması, NOS ve Turbo BOV ses çıkışı:"))).Font(BNUITheme::Font(10, false)).ColorAndOpacity(BNUITheme::SteelMuted)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 6.f, 0.f))[MakeGainBtn(TEXT("%25"), 0.25f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 6.f, 0.f))[MakeGainBtn(TEXT("%50"), 0.50f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 6.f, 0.f))[MakeGainBtn(TEXT("%75"), 0.75f)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[MakeGainBtn(TEXT("%100"), 1.00f)]
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
