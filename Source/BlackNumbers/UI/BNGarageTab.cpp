#include "UI/BNMenuWidget.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/BNHUD.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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

	// Live 10-color custom paint swatch picker directly under the live 360-degree showcase car!
	const TArray<TPair<FString, FLinearColor>>& Palette = FBNVehicleSpec::GetPaintPalette();
	const int32 ActivePaintIdx = P.GetActivePaintIndex();
	TSharedRef<SHorizontalBox> PaintSwatches = SNew(SHorizontalBox);
	PaintSwatches->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
	[
		BNUITheme::MakeMWButton(FText::FromString(TEXT("FABRİKA")),
			ActivePaintIdx < 0 ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
			FOnClicked::CreateLambda([this]()
			{
				if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
				{
					Pr->TrySetCustomPaint(-1, GetPlayerCar(OwnerHUD));
					PreviewedCar.Reset();
					ActionFeedback = Pr->GetFeedbackText();
					Refresh();
				}
				return FReply::Handled();
			}), true, 9, FMargin(8.f, 5.f))
	];
	for (int32 p = 0; p < Palette.Num(); ++p)
	{
		const bool bPicked = (ActivePaintIdx == p);
		const FLinearColor SwatchCol = Palette[p].Value;
		PaintSwatches->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.ToolTipText(FText::FromString(Palette[p].Key))
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateLambda([this, p]()
			{
				UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiConfirm, 0.85f);
				if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
				{
					Pr->TrySetCustomPaint(p, GetPlayerCar(OwnerHUD));
					PreviewedCar.Reset();
					ActionFeedback = Pr->GetFeedbackText();
					Refresh();
				}
				return FReply::Handled();
			}))
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(bPicked ? BNUITheme::AmberBright : BNUITheme::HairlineSteel)
				.Padding(FMargin(bPicked ? 2.5f : 1.5f))
				[
					SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(SwatchCol)
					]
				]
			]
		];
	}

	const FString PaintName = (ActivePaintIdx >= 0 && Palette.IsValidIndex(ActivePaintIdx))
		? Palette[ActivePaintIdx].Key
		: FString(TEXT("Fabrika Rengi"));

	// Center open viewport: completely clear for live 360-degree 3D car turntable + bottom paint & nameplate dock!
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
			.Padding(FMargin(20.f, 10.f))
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
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 6.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(ActionFeedback.IsEmpty()
						? FString::Printf(TEXT("SELİN'İN GARAJI  •  BOYA: %s  •  360° CANLI VİTRİN"), *PaintName)
						: ActionFeedback))
					.Font(BNUITheme::Font(10, !ActionFeedback.IsEmpty()))
					.ColorAndOpacity(ActionFeedback.IsEmpty() ? BNUITheme::SteelMuted : BNUITheme::EmeraldGreen)
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					PaintSwatches
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
								const int32 PaintIdx = Pr->GetProfile().CustomPaintIndices[c];
								const auto& Pal = FBNVehicleSpec::GetPaintPalette();
								if (PaintIdx >= 0 && Pal.IsValidIndex(PaintIdx))
								{
									CurCar->ApplyPaintTint(Pal[PaintIdx].Value);
								}
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
