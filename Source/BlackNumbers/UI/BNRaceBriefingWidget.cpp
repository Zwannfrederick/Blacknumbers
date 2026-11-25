#include "UI/BNRaceBriefingWidget.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNHUD.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* BriefingDisciplineTag(EBNRaceDiscipline Discipline)
	{
		switch (Discipline)
		{
		case EBNRaceDiscipline::Circuit: return TEXT("PİST TURU");
		case EBNRaceDiscipline::Drift:   return TEXT("DRİFT YARIŞI");
		case EBNRaceDiscipline::Drag:    return TEXT("DRAG YARIŞI");
		default:                         return TEXT("SPRINT");
		}
	}

	FLinearColor BriefingDisciplineColor(EBNRaceDiscipline Discipline)
	{
		switch (Discipline)
		{
		case EBNRaceDiscipline::Circuit: return BNUITheme::AmberBright;
		case EBNRaceDiscipline::Drift:   return BNUITheme::NosCyan;
		case EBNRaceDiscipline::Drag:    return FLinearColor(0.96f, 0.34f, 0.24f, 1.f);
		default:                         return BNUITheme::EmeraldGreen;
		}
	}

	FString BriefingFlavorNote(EBNRaceDiscipline Discipline)
	{
		switch (Discipline)
		{
		case EBNRaceDiscipline::Drag:
			return TEXT("MANUEL VİTES ZORUNLU: [E] Vites Yükselt / [Q] Vites Düşür. Yeşil devir aralığında vites atarak fırlama kazan; kırmızı bölgede uzun süre tutarsan MOTOR PATLAR!");
		case EBNRaceDiscipline::Drift:
			return TEXT("El freni ([SPACE]) ve gaz dengesiyle yüksek açılı drift zinciri kur. Duvarlara çarpmadan puanları katla!");
		case EBNRaceDiscipline::Circuit:
			return TEXT("Karamara sokaklarında turluk kapışma. Viraj çıkışında NOS ve geç frenle rakipleri geride bırak.");
		default:
			return TEXT("Noktadan noktaya sokak sprinti. Trafiğe ve yüksek hızlı çarpışmalara (TOTALED) dikkat et!");
		}
	}

	float EstimateRouteMeters(const UBNRaceDefinition& Def)
	{
		if (Def.RacingLine.Num() >= 2)
		{
			float TotalCm = 0.f;
			for (int32 i = 1; i < Def.RacingLine.Num(); ++i)
			{
				TotalCm += FVector::Dist(Def.RacingLine[i - 1], Def.RacingLine[i]);
			}
			return TotalCm / 100.f;
		}
		if (Def.Checkpoints.Num() >= 2)
		{
			float TotalCm = 0.f;
			for (int32 i = 1; i < Def.Checkpoints.Num(); ++i)
			{
				TotalCm += FVector::Dist(Def.Checkpoints[i - 1].GetLocation(), Def.Checkpoints[i].GetLocation());
			}
			return TotalCm / 100.f;
		}
		return 0.f;
	}
}

void SBNRaceBriefingWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;
	RaceEvent = InArgs._RaceEvent;

	const ABNRaceEvent* Event = RaceEvent.Get();
	const UBNRaceDefinition* Def = Event ? Event->Definition.Get() : nullptr;
	const EBNRaceDiscipline Disc = Def ? Def->Discipline : EBNRaceDiscipline::Sprint;
	const FString Title = Def ? Def->DisplayName.ToString().ToUpper() : TEXT("SOKAK YARIŞI");
	const FLinearColor Accent = BriefingDisciplineColor(Disc);

	FString DistrictName(TEXT("KARAMARA"));
	FString RulerTag;
	if (Event && Event->GetWorld())
	{
		if (const UBNRoadGraphSubsystem* RoadGraph = Event->GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>();
			RoadGraph && RoadGraph->HasGraph())
		{
			const EBNDistrict Dist = RoadGraph->GetDistrictAt(Event->GetActorLocation(), &DistrictName, nullptr);
			if (const UBNStorySubsystem* Story = Event->GetWorld()->GetGameInstance()
				? Event->GetWorld()->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
			{
				const FBNDistrictTerritoryInfo Terr = Story->GetState().GetDistrictTerritory(Dist);
				RulerTag = Terr.bDistrictLiberated
					? TEXT("  •  BÖLGE ELE GEÇİRİLDİ")
					: FString::Printf(TEXT("  •  HÜKÜMDAR: #%02d %s"), Terr.RulerRank, *Terr.RulerName);
			}
		}
	}

	const float RouteMeters = Def ? EstimateRouteMeters(*Def) : 0.f;
	const FString FormatVal = (Disc == EBNRaceDiscipline::Drift)
		? FString::Printf(TEXT("%.0f SANİYE"), Def ? Def->TimeLimitSeconds : 75.f)
		: (Disc == EBNRaceDiscipline::Circuit)
			? FString::Printf(TEXT("%d TUR  (%.1f km)"), Def ? Def->Laps : 2, (RouteMeters * (Def ? Def->Laps : 2)) / 1000.f)
			: (RouteMeters >= 1000.f
				? FString::Printf(TEXT("%.2f km"), RouteMeters / 1000.f)
				: FString::Printf(TEXT("%d m"), FMath::RoundToInt(RouteMeters)));

	const int32 Rivals = Def ? Def->RivalSkills.Num() : 0;
	const FString RivalVal = (Disc == EBNRaceDiscipline::Drift)
		? TEXT("SOLO SKOR")
		: FString::Printf(TEXT("%d RAKİP"), FMath::Max(1, Rivals));

	// Real single-source-of-truth payouts synchronized with FBNPlayerProfile::AwardRaceResult!
	int32 FirstCash = 0, FirstRep = 0, SecondCash = 0, SecondRep = 0;
	const int32 TotalRacers = (Disc == EBNRaceDiscipline::Drift) ? 1 : (FMath::Max(1, Rivals) + 1);
	FBNPlayerProfile::EstimateRacePayout(Disc == EBNRaceDiscipline::Drift, 1, TotalRacers, 5000, FirstCash, FirstRep);
	FBNPlayerProfile::EstimateRacePayout(Disc == EBNRaceDiscipline::Drift, 2, TotalRacers, 2500, SecondCash, SecondRep);

	const FString CashReward = (Disc == EBNRaceDiscipline::Drift)
		? FString::Printf(TEXT("$%s+"), *FString::FormatAsNumber(FirstCash))
		: FString::Printf(TEXT("$%s / $%s"), *FString::FormatAsNumber(FirstCash), *FString::FormatAsNumber(SecondCash));

	// Personal Best record for this race track
	FString PbLabel = TEXT("HENÜZ YOK");
	if (Def && Event && Event->GetWorld() && Event->GetWorld()->GetGameInstance())
	{
		if (const UBNProgressionSubsystem* Prog = Event->GetWorld()->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>())
		{
			if (Disc == EBNRaceDiscipline::Drift)
			{
				if (const int32* BestDrift = Prog->GetProfile().BestDriftScores.Find(Def->GetName()))
				{
					PbLabel = FString::Printf(TEXT("%s PUAN"), *FString::FormatAsNumber(*BestDrift));
				}
			}
			else if (const float* BestTime = Prog->GetProfile().BestRaceTimes.Find(Def->GetName()))
			{
				PbLabel = ABNHUD::FormatRaceTime(*BestTime);
			}
		}
	}
	const FString RepReward = FString::Printf(TEXT("+%d  |  PB: %s"), FirstRep, *PbLabel);

	const FSlateBrush* VigFull = BNUITheme::GetVignetteFullBrush();
	const bool bHasVig = VigFull != BNUITheme::WhiteBrush();

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.015f, 0.018f, 0.024f, 0.72f))
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(VigFull)
			.ColorAndOpacity(bHasVig ? FLinearColor(1.f, 1.f, 1.f, 0.90f) : FLinearColor(0.f, 0.f, 0.f, 0.f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.f)
			[
				SNew(SBorder)
				.BorderImage(BNUITheme::GetPanelBevelBrush())
				.BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.98f))
				.Padding(FMargin(0.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						BNUITheme::MakeHazardBar(7.f)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(32.f, 24.f, 32.f, 26.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBorder)
								.BorderImage(BNUITheme::WhiteBrush())
								.BorderBackgroundColor(Accent)
								.Padding(FMargin(10.f, 4.f))
								[
									SNew(STextBlock)
									.Text(FText::FromString(BriefingDisciplineTag(Disc)))
									.Font(BNUITheme::Font(10, true))
									.ColorAndOpacity(BNUITheme::ObsidianInk)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("BÖLGE: %s%s"), *DistrictName, *RulerTag)))
								.Font(BNUITheme::Font(10, true))
								.ColorAndOpacity(BNUITheme::SteelMuted)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 6.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(Title))
							.Font(BNUITheme::Font(28, true))
							.ColorAndOpacity(BNUITheme::ChalkWhite)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 18.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("YARIŞ ÖZETİ  —  BU ETKİNLİĞE GİRMEK İSTER MİSİN?")))
							.Font(BNUITheme::Font(11, true))
							.ColorAndOpacity(BNUITheme::AmberBright)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
							[
								MakeTile(TEXT("PARKUR / SÜRE"), FormatVal, BNUITheme::ChalkWhite)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
							[
								MakeTile(TEXT("KATILIM"), RivalVal, Accent)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
							[
								MakeTile(TEXT("1. / 2. NAKİT"), CashReward, BNUITheme::EmeraldGreen)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								MakeTile(TEXT("İTİBAR"), RepReward, BNUITheme::NosCyan)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 22.f))
						[
							SNew(SBorder)
							.BorderImage(BNUITheme::WhiteBrush())
							.BorderBackgroundColor(Disc == EBNRaceDiscipline::Drag ? BNUITheme::CrimsonDarkCard : BNUITheme::GunmetalCard)
							.Padding(FMargin(16.f, 12.f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(BriefingFlavorNote(Disc)))
								.Font(BNUITheme::Font(11, Disc == EBNRaceDiscipline::Drag))
								.ColorAndOpacity(BNUITheme::ChalkWhite)
								.AutoWrapText(true)
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
							[
								BNUITheme::MakeMWButton(
									FText::FromString(TEXT("YARIŞA GİR  [ENTER]")),
									EBNButtonVariant::ConfirmGreen,
									FOnClicked::CreateSP(this, &SBNRaceBriefingWidget::HandleEnterRace),
									true, 13, FMargin(18.f, 13.f))
							]
							+ SHorizontalBox::Slot().FillWidth(0.75f)
							[
								BNUITheme::MakeMWButton(
									FText::FromString(TEXT("VAZGEÇ  [ESC]")),
									EBNButtonVariant::SecondarySteel,
									FOnClicked::CreateSP(this, &SBNRaceBriefingWidget::HandleCancel),
									true, 12, FMargin(16.f, 13.f))
							]
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SBNRaceBriefingWidget::MakeTile(const TCHAR* Label, const FString& Value, const FLinearColor& Color)
{
	return SNew(SBorder)
		.BorderImage(BNUITheme::WhiteBrush())
		.BorderBackgroundColor(BNUITheme::GunmetalCard)
		.Padding(FMargin(12.f, 10.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(BNUITheme::Font(9, true))
				.ColorAndOpacity(BNUITheme::SteelMuted)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Value))
				.Font(BNUITheme::Font(14, true))
				.ColorAndOpacity(Color)
			]
		];
}

FReply SBNRaceBriefingWidget::HandleEnterRace()
{
	UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiConfirm);
	if (ABNHUD* HUD = OwnerHUD.Get())
	{
		ABNRaceEvent* Event = RaceEvent.Get();
		HUD->CloseRaceBriefing(false);
		if (Event)
		{
			if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(HUD->GetOwningPawn()))
			{
				Event->StartRace(Car);
			}
		}
	}
	return FReply::Handled();
}

FReply SBNRaceBriefingWidget::HandleCancel()
{
	UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiClick);
	if (ABNHUD* HUD = OwnerHUD.Get())
	{
		HUD->CloseRaceBriefing(true);
	}
	return FReply::Handled();
}

FReply SBNRaceBriefingWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		return HandleEnterRace();
	}
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		return HandleCancel();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
