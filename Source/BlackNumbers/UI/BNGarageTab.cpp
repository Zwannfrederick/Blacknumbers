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
			const FLinearColor PipCol = bOwnedPip ? BNUITheme::Accent
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
			.BorderBackgroundColor(BNUITheme::Card)
			.Padding(FMargin(11.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s   T%d/6"), FBNUpgradeSpec::SlotLabel(Slot), Tier)))
						.Font(BNUITheme::Font(12, true))
						.ColorAndOpacity(BNUITheme::Bone)
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
		.WidthOverride(324.f)
		[
			BNUITheme::MakePanel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					BNUITheme::MakeSectionHeader(
						bPreviewingOther ? TEXT("ÖNİZLEME") : TEXT("PERFORMANS"),
						FBNVehicleSpec::BodyStyleLabel(ShownSpec.BodyStyle),
						bPreviewingOther ? BNUITheme::Accent : BNUITheme::Nos)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					SNew(STextBlock).Text(FText::FromString(ShownSpec.Subtitle)).Font(BNUITheme::Font(11, false)).ColorAndOpacity(BNUITheme::Muted).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("MOTOR TORKU"), FString::Printf(TEXT("%.0f Nm"), ShownStats.MaxTorqueNm), ShownStats.MaxTorqueNm / 880.f, BNUITheme::Accent)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("MAKS DEVİR"), FString::Printf(TEXT("%.0f RPM"), ShownStats.MaxRpm), ShownStats.MaxRpm / 9500.f, BNUITheme::Ok)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("LASTİK TUTUŞU"), FString::Printf(TEXT("%.2fx"), ShownStats.TireGripScale), ShownStats.TireGripScale / 1.50f, BNUITheme::Nos)]
				+ SVerticalBox::Slot().AutoHeight()[MakeStatBar(TEXT("NOS MAKS HIZ"), FString::Printf(TEXT("%.0f km/h"), ShownStats.NosMaxSpeedKmh), ShownStats.NosMaxSpeedKmh / 360.f, BNUITheme::Nos)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 8.f))
				[
					BNUITheme::MakeSectionHeader(TEXT("MODİFİYE"), ActiveSpec.DisplayName, BNUITheme::Accent)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[UpgradeRows]
				],
				FMargin(16.f, 14.f), BNUITheme::Panel)
		];

	// Compact 2-row paint palette (5+5) + Finish + Rim Color + Underglow Neon controls (zero overflow at 1080p/720p!)
	const TArray<TPair<FString, FLinearColor>>& Palette = FBNVehicleSpec::GetPaintPalette();
	const int32 ActivePaintIdx = P.GetActivePaintIndex();
	const EBNPaintFinish ActiveFinish = P.GetActivePaintFinish();
	const auto& Rims = FBNVehicleSpec::GetRimPaintPalette();
	const int32 ActiveRimIdx = P.GetActiveRimPaintIndex();
	const auto& Neons = FBNVehicleSpec::GetUnderglowPalette();
	const int32 ActiveNeonIdx = P.GetActiveUnderglowIndex();

	auto MakeColorSwatch = [](const FLinearColor& Col, const FString& Tip, bool bPicked, FOnClicked OnClick, float BoxSize = 18.f) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.ToolTipText(FText::FromString(Tip))
			.ContentPadding(FMargin(0.f))
			.OnClicked(OnClick)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(bPicked ? BNUITheme::AccentHi : BNUITheme::Hairline)
				.Padding(FMargin(bPicked ? 2.f : 1.f))
				[
					SNew(SBox).WidthOverride(BoxSize).HeightOverride(BoxSize)
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Col)
					]
				]
			];
	};

	TSharedRef<SHorizontalBox> PaintRow1 = SNew(SHorizontalBox);
	TSharedRef<SHorizontalBox> PaintRow2 = SNew(SHorizontalBox);
	PaintRow1->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
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
			}), true, 10, FMargin(9.f, 5.f))
	];
	for (int32 p = 0; p < Palette.Num(); ++p)
	{
		TSharedRef<SHorizontalBox>& TargetRow = (p < 5) ? PaintRow1 : PaintRow2;
		TargetRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		[
			MakeColorSwatch(Palette[p].Value, Palette[p].Key, ActivePaintIdx == p,
				FOnClicked::CreateLambda([this, p]()
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
				}), 22.f)
		];
	}

	// Paint Finish row (4 presets)
	TSharedRef<SHorizontalBox> FinishRow = SNew(SHorizontalBox);
	FinishRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("YÜZEY"))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Muted)
	];
	for (int32 f = 0; f < static_cast<int32>(EBNPaintFinish::Count); ++f)
	{
		const EBNPaintFinish Fin = static_cast<EBNPaintFinish>(f);
		FinishRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		[
			BNUITheme::MakeMWButton(FText::FromString(FBNVehicleSpec::PaintFinishLabel(Fin)),
				ActiveFinish == Fin ? EBNButtonVariant::PrimaryAmber : EBNButtonVariant::SecondarySteel,
				FOnClicked::CreateLambda([this, Fin]()
				{
					if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
					{
						Pr->TrySetPaintFinish(Fin, GetPlayerCar(OwnerHUD));
						PreviewedCar.Reset();
						ActionFeedback = Pr->GetFeedbackText();
						Refresh();
					}
					return FReply::Handled();
				}), true, 10, FMargin(8.f, 4.f))
		];
	}

	// Rim Paint & Underglow Neon compact strip
	TSharedRef<SHorizontalBox> RimAndNeonRow = SNew(SHorizontalBox);
	RimAndNeonRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("JANT"))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Muted)
	];
	for (int32 r = 0; r < Rims.Num(); ++r)
	{
		RimAndNeonRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		[
			MakeColorSwatch(Rims[r].Value, Rims[r].Key, ActiveRimIdx == r,
				FOnClicked::CreateLambda([this, r]()
				{
					UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiConfirm, 0.85f);
					if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
					{
						Pr->TrySetRimPaint(r, GetPlayerCar(OwnerHUD));
						PreviewedCar.Reset();
						ActionFeedback = Pr->GetFeedbackText();
						Refresh();
					}
					return FReply::Handled();
				}), 18.f)
		];
	}
	RimAndNeonRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 6.f, 0.f))
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("NEON"))).Font(BNUITheme::Font(10, true)).ColorAndOpacity(BNUITheme::Muted)
	];
	for (int32 n = 0; n < Neons.Num(); ++n)
	{
		const FLinearColor PreviewCol = (n == 0) ? FLinearColor(0.14f, 0.15f, 0.18f, 1.f) : Neons[n].Value;
		RimAndNeonRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		[
			MakeColorSwatch(PreviewCol, Neons[n].Key, ActiveNeonIdx == n,
				FOnClicked::CreateLambda([this, n]()
				{
					UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiConfirm, 0.85f);
					if (UBNProgressionSubsystem* Pr = GetGI(OwnerHUD) ? GetGI(OwnerHUD)->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
					{
						Pr->TrySetUnderglow(n, GetPlayerCar(OwnerHUD));
						PreviewedCar.Reset();
						ActionFeedback = Pr->GetFeedbackText();
						Refresh();
					}
					return FReply::Handled();
				}), 18.f)
		];
	}

	const FString PaintName = (ActivePaintIdx >= 0 && Palette.IsValidIndex(ActivePaintIdx))
		? Palette[ActivePaintIdx].Key
		: FString(TEXT("Fabrika Rengi"));

	// Center open viewport: wide open live 360-degree 3D car turntable + clean smoked-glass bottom paint dock
	TSharedRef<SVerticalBox> CenterShowcase = SNew(SVerticalBox)
	+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
	+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
	[
		SNew(SBox).MaxDesiredWidth(540.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BNUITheme::MakeAccentRule(2.f)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(BNUITheme::WhiteBrush())
				.BorderBackgroundColor(BNUITheme::Panel)
				.Padding(FMargin(16.f, 10.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s  //  %s"), *ShownSpec.DisplayName.ToUpper(), FBNVehicleSpec::BodyStyleLabel(ShownSpec.BodyStyle))))
						.Font(BNUITheme::DisplayFont(22))
						.ColorAndOpacity(BNUITheme::AccentHi)
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 8.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(ActionFeedback.IsEmpty()
							? FString::Printf(TEXT("%s  (%s)"), *PaintName, FBNVehicleSpec::PaintFinishLabel(ActiveFinish))
							: ActionFeedback))
						.Font(BNUITheme::Font(11, !ActionFeedback.IsEmpty()))
						.ColorAndOpacity(ActionFeedback.IsEmpty() ? BNUITheme::Muted : BNUITheme::Ok)
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 5.f))[PaintRow1]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 7.f))[PaintRow2]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))[FinishRow]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[RimAndNeonRow]
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
		const FString SpecLine = FString::Printf(TEXT("%.0f Nm • %.2fx Tutuş"),
			Spec.BaseTorqueNm, Spec.BaseTireGripScale);

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
			.BorderBackgroundColor((bActive || bPreviewed) ? BNUITheme::AccentDark : BNUITheme::Card)
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
							SNew(STextBlock).Text(FText::FromString(Spec.DisplayName)).Font(BNUITheme::Font(13, true))
							.ColorAndOpacity(bActive ? BNUITheme::AccentHi : BNUITheme::Bone)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
						[
							SNew(STextBlock).Text(FText::FromString(SpecLine)).Font(BNUITheme::Font(10, false)).ColorAndOpacity(BNUITheme::Muted)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BtnGroup]
				]
			]
		];
	}

	TSharedRef<SWidget> RightRail = SNew(SBox)
		.WidthOverride(332.f)
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(BNUITheme::Panel)
			.Padding(FMargin(14.f, 12.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
				[
					BNUITheme::MakeSectionHeader(TEXT("ARAÇ KOLEKSİYONU"), FString::Printf(TEXT("%d ARAÇ"), FBNUpgradeSpec::NumCars), BNUITheme::Accent)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[CarList]]
			]
		];

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth()[LeftRail]
	+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(16.f, 0.f))[CenterShowcase]
	+ SHorizontalBox::Slot().AutoWidth()[RightRail];
}
