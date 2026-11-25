#include "UI/BNWorldMapWidget.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageZone.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Rendering/DrawElements.h"
#include "Story/BNStorySubsystem.h"
#include "Story/BNStoryTypes.h"
#include "UI/BNHUD.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"
#include "World/BNWorldActivities.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Karamara 2 km x 2 km bounding box in UE centimeters (+X East, +Y South)
	constexpr float MapMinX = -98000.f;
	constexpr float MapMaxX = 104000.f;
	constexpr float MapMinY = -98000.f;
	constexpr float MapMaxY =  90000.f;

	struct FBNMapPin
	{
		FVector WorldPos = FVector::ZeroVector;
		FString Label;
		FString Category;
		FString Detail;
		FLinearColor Color = FLinearColor::White;
		float SizePx = 7.f;
		bool bIsNextBoss = false;
		bool bRace = false;
	};

	bool IsPinVisible(const FBNMapPin& Pin, EBNMapFilter Filter)
	{
		switch (Filter)
		{
		case EBNMapFilter::Program: return Pin.bIsNextBoss || Pin.Category == TEXT("GARAJ");
		case EBNMapFilter::Races: return Pin.bRace;
		default: return true;
		}
	}

	const FLinearColor SpeedTrapPinCol(0.95f, 0.88f, 0.22f, 1.f);
	const FLinearColor JumpRampPinCol(1.00f, 0.44f, 0.14f, 1.f);
	const FLinearColor CooldownPinCol(0.22f, 0.62f, 1.0f, 1.f);

	FLinearColor ColorForDiscipline(EBNRaceDiscipline Disc)
	{
		switch (Disc)
		{
		case EBNRaceDiscipline::Circuit: return BNUITheme::AmberBright;
		case EBNRaceDiscipline::Drift:   return BNUITheme::NosCyan;
		case EBNRaceDiscipline::Drag:    return FLinearColor(0.96f, 0.30f, 0.42f, 1.f);
		default:                         return FLinearColor(0.98f, 0.56f, 0.12f, 1.f);
		}
	}

	const TCHAR* ShortDiscTag(EBNRaceDiscipline Disc)
	{
		switch (Disc)
		{
		case EBNRaceDiscipline::Circuit: return TEXT("PİST");
		case EBNRaceDiscipline::Drift:   return TEXT("DRİFT");
		case EBNRaceDiscipline::Drag:    return TEXT("DRAG");
		default:                         return TEXT("SPRINT");
		}
	}

	void GatherWorldPins(UWorld* World, TArray<FBNMapPin>& OutPins)
	{
		OutPins.Reset();
		if (!World)
		{
			return;
		}
		const UBNProgressionSubsystem* Prog = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
		const UBNStorySubsystem* Story = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
		const FBNBossSpec* NextBoss = (Story && !Story->GetState().bFinalChaseActive && !Story->GetState().bCampaignCompleted)
			? FBNStoryState::FindBossByRank(Story->GetState().NextBossRank) : nullptr;
		const FName Qualifier = NextBoss ? Story->GetState().GetNextQualifier() : NAME_None;
		const FName NextRace = NextBoss && Qualifier.IsNone() ? FName(*NextBoss->RaceAssetName) : Qualifier;

		for (TActorIterator<ABNGarageZone> It(World); It; ++It)
		{
			OutPins.Add({ It->GetActorLocation(), It->GarageName.ToUpper(), TEXT("GARAJ"),
				TEXT("Ücretsiz Tamir • 6 Araç • Tier 1-6 Modifiye"), BNUITheme::EmeraldGreen, 8.5f, false });
		}
		for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
		{
			if (const UBNRaceDefinition* Def = It->Definition.Get())
			{
				const bool bNextBossRace = NextBoss && Def->GetFName() == NextRace;
				FString Detail = ShortDiscTag(Def->Discipline);
				if (Prog)
				{
					if (Def->Discipline == EBNRaceDiscipline::Drift)
					{
						if (const int32* Pb = Prog->GetProfile().BestDriftScores.Find(Def->GetName()))
						{
							Detail += FString::Printf(TEXT("  •  PB: %s puan"), *FString::FormatAsNumber(*Pb));
						}
					}
					else if (const float* Pb = Prog->GetProfile().BestRaceTimes.Find(Def->GetName()))
					{
						Detail += FString::Printf(TEXT("  •  PB: %s"), *ABNHUD::FormatRaceTime(*Pb));
					}
				}
				if (bNextBossRace)
				{
					Detail = FString::Printf(TEXT("#%02d %s • %s"), NextBoss->Rank, *NextBoss->Name,
						Qualifier.IsNone() ? TEXT("DÜELLO") : TEXT("SIRADAKİ ELEME"));
				}
				OutPins.Add({ It->GetActorLocation(), Def->DisplayName.ToString(), ShortDiscTag(Def->Discipline),
					Detail, ColorForDiscipline(Def->Discipline), bNextBossRace ? 9.f : 7.f, bNextBossRace, true });
			}
		}
		for (TActorIterator<ABNSpeedTrap> It(World); It; ++It)
		{
			FString Detail = TEXT("100+ km/h hız kamerası");
			if (Prog)
			{
				if (const float* Pb = Prog->GetProfile().BestSpeedTrapKmh.Find(It->TrapId))
				{
					Detail = FString::Printf(TEXT("REKOR: %.0f KM/H"), *Pb);
				}
			}
			OutPins.Add({ It->GetActorLocation(), It->DisplayName, TEXT("HIZ TUZAĞI"), Detail, SpeedTrapPinCol, 6.f, false });
		}
		for (TActorIterator<ABNStuntJump> It(World); It; ++It)
		{
			FString Detail = TEXT("Sıçrama rampası");
			if (Prog)
			{
				if (const float* Pb = Prog->GetProfile().BestJumpMeters.Find(It->JumpId))
				{
					Detail = FString::Printf(TEXT("REKOR: %.0f METRE"), *Pb);
				}
			}
			OutPins.Add({ It->GetActorLocation(), It->DisplayName, TEXT("RAMPA"), Detail, JumpRampPinCol, 6.f, false });
		}
		if (const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>();
			RoadGraph && RoadGraph->HasGraph())
		{
			for (const FBNCooldownSpot& Spot : RoadGraph->GetCooldownSpots())
			{
				OutPins.Add({ Spot.Location, Spot.DisplayName, TEXT("SAKLANMA"), TEXT("Polis Soğuma 3.5x Hız"), CooldownPinCol, 5.5f, false });
			}
		}
	}

	/** Interactive vector map surface drawing the complete Karamara road graph, Mara Sea, POIs, and A* GPS path. */
	class SBNKaramaraMapCanvas : public SLeafWidget
	{
	public:
		DECLARE_DELEGATE_TwoParams(FOnMapPointPicked, const FVector&, const FString&);

		SLATE_BEGIN_ARGS(SBNKaramaraMapCanvas) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
			SLATE_ATTRIBUTE(EBNMapFilter, MapFilter)
			SLATE_EVENT(FOnMapPointPicked, OnPointPicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OwnerHUD = InArgs._OwnerHUD;
			MapFilter = InArgs._MapFilter;
			OnPointPicked = InArgs._OnPointPicked;
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 700.f); }

		void ComputeMapTransform(const FVector2D& Size, FVector2D& OutOrigin, float& OutScale) const
		{
			const float Pad = 36.f;
			const float W = FMath::Max(100.f, Size.X - Pad * 2.f);
			const float H = FMath::Max(100.f, Size.Y - Pad * 2.f);
			const float WorldW = MapMaxX - MapMinX;
			const float WorldH = MapMaxY - MapMinY;
			OutScale = FMath::Min(W / WorldW, H / WorldH);
			OutOrigin.X = (Size.X - WorldW * OutScale) * 0.5f;
			OutOrigin.Y = (Size.Y - WorldH * OutScale) * 0.5f;
		}

		FVector2D WorldToScreen(const FVector& Pos, const FVector2D& Origin, float Scale) const
		{
			return FVector2D(
				Origin.X + (Pos.X - MapMinX) * Scale,
				Origin.Y + (Pos.Y - MapMinY) * Scale);
		}

		FVector ScreenToWorld(const FVector2D& LocalPt, const FVector2D& Origin, float Scale) const
		{
			return FVector(
				MapMinX + (LocalPt.X - Origin.X) / FMath::Max(Scale, 1e-6f),
				MapMinY + (LocalPt.Y - Origin.Y) / FMath::Max(Scale, 1e-6f),
				0.f);
		}

		virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			HoveredLocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			bHasHoverPos = true;
			return FReply::Unhandled();
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			bHasHoverPos = false;
			SLeafWidget::OnMouseLeave(MouseEvent);
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			ABNHUD* HUD = OwnerHUD.Get();
			UWorld* World = HUD ? HUD->GetWorld() : nullptr;
			if (!World)
			{
				return FReply::Unhandled();
			}

			const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			FVector2D Origin;
			float Scale = 1.f;
			ComputeMapTransform(MyGeometry.GetLocalSize(), Origin, Scale);

			// 1. Check clickable POI markers first (22 px generous hit radius)
			TArray<FBNMapPin> Pins;
			GatherWorldPins(World, Pins);
			const FBNMapPin* BestPin = nullptr;
			float BestDistSq = 22.f * 22.f;
			for (const FBNMapPin& Pin : Pins)
			{
				if (!IsPinVisible(Pin, MapFilter.Get()))
				{
					continue;
				}
				const FVector2D Pt = WorldToScreen(Pin.WorldPos, Origin, Scale);
				const float D2 = FVector2D::DistSquared(LocalPos, Pt);
				if (D2 <= BestDistSq)
				{
					BestDistSq = D2;
					BestPin = &Pin;
				}
			}
			if (BestPin)
			{
				OnPointPicked.ExecuteIfBound(BestPin->WorldPos, BestPin->Label);
				return FReply::Handled();
			}

			// 2. Otherwise snap click to nearest Karamara road segment cleanly!
			FVector WorldClick = ScreenToWorld(LocalPos, Origin, Scale);
			FString Label(TEXT("HARİTA İŞARETİ"));
			if (const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>();
				RoadGraph && RoadGraph->HasGraph())
			{
				FVector RoadPt, RoadTan;
				float Width = 0.f, EdgeDist = 0.f;
				int32 EdgeIdx = INDEX_NONE;
				if (RoadGraph->FindNearestRoadPoint(WorldClick, RoadPt, RoadTan, Width, EdgeIdx, EdgeDist))
				{
					WorldClick = RoadPt;
				}
				FString DistName;
				RoadGraph->GetDistrictAt(WorldClick, &DistName, nullptr);
				if (!DistName.IsEmpty())
				{
					Label = FString::Printf(TEXT("ROTA: %s"), *DistName);
				}
			}
			OnPointPicked.ExecuteIfBound(WorldClick, Label);
			return FReply::Handled();
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			FVector2D Origin;
			float Scale = 1.f;
			ComputeMapTransform(Size, Origin, Scale);

			const FSlateBrush* Brush = BNUITheme::WhiteBrush();
			// Deep tactical asphalt map background
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Brush,
				ESlateDrawEffect::None, FLinearColor(0.036f, 0.042f, 0.052f, 0.98f));
			++LayerId;

			// Mara Sea shoreline tint on the east side (X > 62000)
			const float SeaX = Origin.X + (62000.f - MapMinX) * Scale;
			if (SeaX < Size.X)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(Size.X - SeaX, Size.Y), FSlateLayoutTransform(FVector2D(SeaX, 0.f))),
					Brush, ESlateDrawEffect::None, FLinearColor(0.05f, 0.14f, 0.22f, 0.72f));
			}
			++LayerId;

			// Subtle 200 m tactical grid
			const FLinearColor GridCol(0.14f, 0.16f, 0.20f, 0.35f);
			for (float gx = -80000.f; gx <= 100000.f; gx += 20000.f)
			{
				const float Sx = Origin.X + (gx - MapMinX) * Scale;
				TArray<FVector2D> Line = { FVector2D(Sx, 0.f), FVector2D(Sx, Size.Y) };
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridCol, true, 1.f);
			}
			for (float gy = -80000.f; gy <= 80000.f; gy += 20000.f)
			{
				const float Sy = Origin.Y + (gy - MapMinY) * Scale;
				TArray<FVector2D> Line = { FVector2D(0.f, Sy), FVector2D(Size.X, Sy) };
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridCol, true, 1.f);
			}
			++LayerId;

			ABNHUD* HUD = OwnerHUD.Get();
			UWorld* World = HUD ? HUD->GetWorld() : nullptr;
			const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
			const UBNStorySubsystem* Story = World && World->GetGameInstance()
				? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;

			// 6 Most Wanted / Carbon-style District Territory Control Zones (translucent colored overlay + border)
			if (RoadGraph && RoadGraph->HasGraph())
			{
				for (const FBNDistrictBounds& D : RoadGraph->GetDistricts())
				{
					const FBNDistrictTerritoryInfo Terr = Story
						? Story->GetState().GetDistrictTerritory(D.District)
						: FBNStoryState().GetDistrictTerritory(D.District);
					const FVector2D MinPt = WorldToScreen(FVector(D.CenterCm.X - D.HalfExtentCm.X, D.CenterCm.Y - D.HalfExtentCm.Y, 0.f), Origin, Scale);
					const FVector2D MaxPt = WorldToScreen(FVector(D.CenterCm.X + D.HalfExtentCm.X, D.CenterCm.Y + D.HalfExtentCm.Y, 0.f), Origin, Scale);
					const FVector2D BoxSize = MaxPt - MinPt;
					if (BoxSize.X > 8.f && BoxSize.Y > 8.f)
					{
						FLinearColor ZoneFill = Terr.bDistrictLiberated ? BNUITheme::EmeraldGreen : Terr.TerritoryColor;
						ZoneFill.A = Terr.bDistrictLiberated ? 0.11f : 0.085f;
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
							AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(MinPt)),
							Brush, ESlateDrawEffect::None, ZoneFill);
						FLinearColor BorderCol = Terr.bDistrictLiberated ? BNUITheme::EmeraldGreen : Terr.TerritoryColor;
						BorderCol.A = 0.42f;
						TArray<FVector2D> RectLines = {
							MinPt, FVector2D(MaxPt.X, MinPt.Y), MaxPt, FVector2D(MinPt.X, MaxPt.Y), MinPt
						};
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), RectLines,
							ESlateDrawEffect::None, BorderCol, true, 1.4f);
					}
				}
				LayerId += 2;
			}

			// Road network polylines
			if (RoadGraph && RoadGraph->HasGraph())
			{
				for (const FBNRoadEdge& Edge : RoadGraph->GetEdges())
				{
					if (Edge.SampledPoints.Num() < 2)
					{
						continue;
					}
					TArray<FVector2D> Poly;
					Poly.Reserve(Edge.SampledPoints.Num());
					for (const FVector& P : Edge.SampledPoints)
					{
						Poly.Add(WorldToScreen(P, Origin, Scale));
					}
					const FLinearColor Col = Edge.bDirt ? FLinearColor(0.54f, 0.40f, 0.24f, 0.90f)
						: (Edge.bHighway ? FLinearColor(0.60f, 0.56f, 0.48f, 0.95f) : FLinearColor(0.38f, 0.42f, 0.48f, 0.88f));
					const float Thick = Edge.bHighway ? 4.2f : (Edge.WidthCm >= 1600.f ? 3.0f : 2.2f);
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Poly, ESlateDrawEffect::None, Col, true, Thick);
				}
				++LayerId;

				// Active A* GPS Route glowing in bright cyan!
				if (RoadGraph->HasGpsDestination())
				{
					const TArray<FVector>& Route = RoadGraph->GetGpsRoutePoints();
					if (Route.Num() >= 2)
					{
						TArray<FVector2D> Poly;
						Poly.Reserve(Route.Num());
						for (const FVector& P : Route)
						{
							Poly.Add(WorldToScreen(P, Origin, Scale));
						}
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Poly,
							ESlateDrawEffect::None, FLinearColor(0.04f, 0.22f, 0.30f, 0.95f), true, 8.5f);
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Poly,
							ESlateDrawEffect::None, BNUITheme::NosCyan, true, 4.5f);
					}
					const FVector2D TargetPt = WorldToScreen(RoadGraph->GetGpsTargetLocation(), Origin, Scale);
					TArray<FVector2D> Diamond = {
						TargetPt + FVector2D(0.f, -12.f), TargetPt + FVector2D(12.f, 0.f),
						TargetPt + FVector2D(0.f, 12.f), TargetPt + FVector2D(-12.f, 0.f),
						TargetPt + FVector2D(0.f, -12.f)
					};
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Diamond,
						ESlateDrawEffect::None, BNUITheme::NosCyan, true, 3.0f);
				}
				LayerId += 3;

				// District Borough Titles & District Ruler status neatly placed near the top of each territory box
				const FSlateFontInfo DistFont = BNUITheme::Font(10, true);
				const FSlateFontInfo RulerFont = BNUITheme::Font(8, true);
				for (const FBNDistrictBounds& D : RoadGraph->GetDistricts())
				{
					const FBNDistrictTerritoryInfo Terr = Story
						? Story->GetState().GetDistrictTerritory(D.District)
						: FBNStoryState().GetDistrictTerritory(D.District);
					const FVector2D TopAnchor = WorldToScreen(
						FVector(D.CenterCm.X - D.HalfExtentCm.X * 0.85f, D.CenterCm.Y - D.HalfExtentCm.Y * 0.82f, 0.f), Origin, Scale);
					const FVector2D LabelPos(FMath::Clamp(TopAnchor.X, 14.f, Size.X - 230.f), FMath::Clamp(TopAnchor.Y, 10.f, Size.Y - 40.f));
					const FLinearColor TitleCol = Terr.bDistrictLiberated ? BNUITheme::EmeraldGreen : Terr.TerritoryColor;
					FSlateDrawElement::MakeText(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(FVector2D(240.f, 18.f), FSlateLayoutTransform(LabelPos)),
						Terr.BoroughTitle, DistFont, ESlateDrawEffect::None, TitleCol);
					const FString RulerLine = Terr.bDistrictLiberated
						? TEXT("✓ BÖLGE ELE GEÇİRİLDİ")
						: FString::Printf(TEXT("HÜKÜMDAR #%02d %s"), Terr.RulerRank, *Terr.RulerName);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(FVector2D(320.f, 14.f), FSlateLayoutTransform(LabelPos + FVector2D(0.f, 14.f))),
						RulerLine, RulerFont, ESlateDrawEffect::None,
						Terr.bDistrictLiberated ? FLinearColor(0.55f, 0.96f, 0.72f, 0.85f) : FLinearColor(0.92f, 0.90f, 0.84f, 0.75f));
				}
				FSlateDrawElement::MakeText(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(180.f, 22.f), FSlateLayoutTransform(WorldToScreen(FVector(86000.f, 36000.f, 0.f), Origin, Scale))),
					TEXT("MARA DENİZİ"), DistFont, ESlateDrawEffect::None, FLinearColor(0.32f, 0.72f, 0.96f, 0.55f));
				++LayerId;
			}

			// Clickable POI markers (Races, Selin's Garage, Speed Traps, Stunt Jumps, Cooldown Spots) + Hover detection
			TArray<FBNMapPin> Pins;
			GatherWorldPins(World, Pins);
			const FBNMapPin* HoveredPin = nullptr;
			FVector2D HoveredPinPt = FVector2D::ZeroVector;
			float BestHoverDistSq = 22.f * 22.f;

			for (const FBNMapPin& Pin : Pins)
			{
				if (!IsPinVisible(Pin, MapFilter.Get()))
				{
					continue;
				}
				const FVector2D Pt = WorldToScreen(Pin.WorldPos, Origin, Scale);
				if (bHasHoverPos)
				{
					const float D2 = FVector2D::DistSquared(HoveredLocalPos, Pt);
					if (D2 < BestHoverDistSq)
					{
						BestHoverDistSq = D2;
						HoveredPin = &Pin;
						HoveredPinPt = Pt;
					}
				}
				const float R = Pin.SizePx;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D((R + 2.f) * 2.f, (R + 2.f) * 2.f),
						FSlateLayoutTransform(Pt - FVector2D(R + 2.f, R + 2.f))),
					Brush, ESlateDrawEffect::None, FLinearColor(0.02f, 0.02f, 0.03f, 0.96f));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f),
						FSlateLayoutTransform(Pt - FVector2D(R, R))),
					Brush, ESlateDrawEffect::None, Pin.Color);

				if (Pin.bIsNextBoss)
				{
					const float Pulse = World ? (13.f + 3.f * FMath::Sin(World->GetRealTimeSeconds() * 5.f)) : 14.f;
					TArray<FVector2D> BossRing = {
						Pt + FVector2D(0.f, -Pulse), Pt + FVector2D(Pulse, 0.f),
						Pt + FVector2D(0.f, Pulse), Pt + FVector2D(-Pulse, 0.f),
						Pt + FVector2D(0.f, -Pulse)
					};
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), BossRing,
						ESlateDrawEffect::None, BNUITheme::CrimsonRed, true, 3.0f);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
						AllottedGeometry.ToPaintGeometry(FVector2D(110.f, 16.f), FSlateLayoutTransform(Pt + FVector2D(12.f, -18.f))),
						TEXT("★ SIRADAKİ"), BNUITheme::Font(9, true), ESlateDrawEffect::None, BNUITheme::AmberBright);
				}
			}
			LayerId += 3;

			// Player vehicle chevron
			if (const ABNVehiclePawn* Car = HUD ? Cast<ABNVehiclePawn>(HUD->GetOwningPawn()) : nullptr)
			{
				const FVector2D P = WorldToScreen(Car->GetActorLocation(), Origin, Scale);
				const float YawRad = FMath::DegreesToRadians(Car->GetActorRotation().Yaw);
				const FVector2D Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
				const FVector2D Right(-Fwd.Y, Fwd.X);
				TArray<FVector2D> Arrow = {
					P + Fwd * 12.f,
					P - Fwd * 8.f + Right * 7.5f,
					P - Fwd * 4.f,
					P - Fwd * 8.f - Right * 7.5f,
					P + Fwd * 12.f
				};
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Arrow,
					ESlateDrawEffect::None, FLinearColor::Black, true, 5.f);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Arrow,
					ESlateDrawEffect::None, FLinearColor::White, true, 2.8f);
				LayerId += 2;
			}

			// Floating Hover Tooltip Card over hovered POI pin
			if (HoveredPin)
			{
				const float CardW = 260.f;
				const float CardH = 54.f;
				const FVector2D TipPos(
					FMath::Clamp(HoveredPinPt.X + 16.f, 12.f, Size.X - CardW - 12.f),
					FMath::Clamp(HoveredPinPt.Y - CardH - 10.f, 12.f, Size.Y - CardH - 12.f));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(CardW, CardH), FSlateLayoutTransform(TipPos)),
					Brush, ESlateDrawEffect::None, FLinearColor(0.03f, 0.038f, 0.050f, 0.96f));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2D(4.f, CardH), FSlateLayoutTransform(TipPos)),
					Brush, ESlateDrawEffect::None, HoveredPin->Color);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2D(CardW - 16.f, 16.f), FSlateLayoutTransform(TipPos + FVector2D(10.f, 6.f))),
					FString::Printf(TEXT("[%s]  %s"), *HoveredPin->Category, *HoveredPin->Label.ToUpper()),
					BNUITheme::Font(9, true), ESlateDrawEffect::None, HoveredPin->Color);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2D(CardW - 16.f, 16.f), FSlateLayoutTransform(TipPos + FVector2D(10.f, 26.f))),
					HoveredPin->Detail, BNUITheme::Font(8, false), ESlateDrawEffect::None, BNUITheme::ChalkWhite);
				LayerId += 2;
			}

			return LayerId;
		}

	private:
		TWeakObjectPtr<ABNHUD> OwnerHUD;
		TAttribute<EBNMapFilter> MapFilter;
		FOnMapPointPicked OnPointPicked;
		FVector2D HoveredLocalPos = FVector2D::ZeroVector;
		bool bHasHoverPos = false;
	};
}

void SBNWorldMapWidget::Construct(const FArguments& InArgs)
{
	OwnerHUD = InArgs._OwnerHUD;

	TSharedRef<SHorizontalBox> Filters = SNew(SHorizontalBox);
	auto AddFilter = [this, &Filters](const TCHAR* Label, EBNMapFilter Filter)
	{
		Filters->AddSlot().FillWidth(1.f).Padding(2.f, 0.f)
		[
			SNew(SButton)
			.IsEnabled_Lambda([this, Filter]() { return MapFilter != Filter; })
			.Text(FText::FromString(Label))
			.OnClicked_Lambda([this, Filter]()
			{
				MapFilter = Filter;
				Invalidate(EInvalidateWidgetReason::Paint);
				return FReply::Handled();
			})
		];
	};
	AddFilter(TEXT("PROGRAM"), EBNMapFilter::Program);
	AddFilter(TEXT("YARIŞLAR"), EBNMapFilter::Races);
	AddFilter(TEXT("TÜMÜ"), EBNMapFilter::All);

	TSharedRef<SVerticalBox> QuickList = SNew(SVerticalBox);
	if (UWorld* World = OwnerHUD.IsValid() ? OwnerHUD->GetWorld() : nullptr)
	{
		TArray<FBNMapPin> Pins;
		GatherWorldPins(World, Pins);
		if (Pins.ContainsByPredicate([](const FBNMapPin& Pin) { return Pin.bIsNextBoss; }))
		{
			MapFilter = EBNMapFilter::Program;
		}
		for (const FBNMapPin& Pin : Pins)
		{
			if (Pin.Category == TEXT("SAKLANMA"))
			{
				continue;
			}
			const FVector PinPos = Pin.WorldPos;
			const FString PinLabel = Pin.Label;
			QuickList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
			[
				SNew(SBox)
				.Visibility_Lambda([this, Pin]() { return IsPinVisible(Pin, MapFilter) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					BNUITheme::MakeMWButton(
						FText::FromString(FString::Printf(TEXT("[%s]  %s"), *Pin.Category, *Pin.Label)),
						Pin.Category == TEXT("GARAJ") ? EBNButtonVariant::ConfirmGreen : EBNButtonVariant::SecondarySteel,
						FOnClicked::CreateLambda([this, PinPos, PinLabel]()
						{
							SetDestination(PinPos, PinLabel);
							return FReply::Handled();
						}),
						true, 10, FMargin(10.f, 7.f))
				]
			];
		}
	}

	auto MakeLegendDot = [](const TCHAR* Name, const FLinearColor& Col) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
		[
			SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
			[
				SNew(SBorder).BorderImage(BNUITheme::WhiteBrush()).BorderBackgroundColor(Col)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(Name)).Font(BNUITheme::Font(9, true)).ColorAndOpacity(BNUITheme::SteelMuted)
		];
	};

	const UBNStorySubsystem* StorySub = (OwnerHUD.IsValid() && OwnerHUD->GetGameInstance())
		? OwnerHUD->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const int32 LiberatedCount = StorySub ? StorySub->GetState().GetLiberatedDistrictCount() : 0;
	const int32 NextBossRank = StorySub && !StorySub->GetState().bFinalChaseActive && !StorySub->GetState().bCampaignCompleted
		? StorySub->GetState().NextBossRank : 0;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(BNUITheme::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.028f, 0.94f))
		.Padding(FMargin(24.f, 20.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BNUITheme::MakeHazardBar(6.f)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 10.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("KARAMARA BÖLGE HARİTASI & GPS NAVİGASYON")))
						.Font(BNUITheme::Font(18, true))
						.ColorAndOpacity(BNUITheme::AmberBright)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Bir hedefe tıklayarak rota çiz.")))
						.Font(BNUITheme::Font(10, false))
						.ColorAndOpacity(BNUITheme::NosCyan)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(BNUITheme::WhiteBrush())
					.BorderBackgroundColor(LiberatedCount >= 6 ? BNUITheme::EmeraldGreen : BNUITheme::CrimsonDarkCard)
					.Padding(FMargin(14.f, 7.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("BÖLGE HAKİMİYETİ:  %d / 6"), LiberatedCount)))
						.Font(BNUITheme::Font(11, true))
						.ColorAndOpacity(LiberatedCount >= 6 ? BNUITheme::ObsidianInk : BNUITheme::AmberBright)
					]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				// Main Interactive Vector Map
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
				[
					SNew(SBorder)
					.BorderImage(BNUITheme::WhiteBrush())
					.BorderBackgroundColor(BNUITheme::HairlineSteel)
					.Padding(FMargin(2.f))
					[
						SNew(SBNKaramaraMapCanvas)
						.OwnerHUD(OwnerHUD)
						.MapFilter_Lambda([this]() { return MapFilter; })
						.OnPointPicked_Lambda([this](const FVector& Pos, const FString& Label)
						{
							SetDestination(Pos, Label);
						})
					]
				]
				// Right GPS Status + Quick Destination Sidebar
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(340.f)
					[
						SNew(SBorder)
						.BorderImage(BNUITheme::GetPanelBevelBrush())
						.BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.96f))
						.Padding(FMargin(16.f, 14.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								BNUITheme::MakeSectionHeader(TEXT("GPS ROTASI"), TEXT(""), BNUITheme::NosCyan)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 12.f))
							[
								SAssignNew(GpsStatusBox, SVerticalBox)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
							[
								Filters
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 0.f, 0.f, 12.f))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									QuickList
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
							[
								BNUITheme::MakeMWButton(
									FText::FromString(NextBossRank > 0
										? TEXT("SIRADAKİ YARIŞA ROTA")
										: TEXT("YARIŞ PROGRAMI TAMAMLANDI")),
									EBNButtonVariant::PrimaryAmber,
									FOnClicked::CreateSP(this, &SBNWorldMapWidget::HandlePlotNextBossGps),
									NextBossRank > 0, 10, FMargin(10.f, 8.f))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("PİST"), BNUITheme::AmberBright) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("SPRINT"), FLinearColor(0.98f, 0.56f, 0.12f, 1.f)) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("DRİFT"), BNUITheme::NosCyan) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("DRAG"), FLinearColor(0.96f, 0.30f, 0.42f, 1.f)) ]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("GARAJ"), BNUITheme::EmeraldGreen) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("SAKLANMA"), CooldownPinCol) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("TUZAK"), SpeedTrapPinCol) ]
								+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendDot(TEXT("RAMPA"), JumpRampPinCol) ]
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								BNUITheme::MakeMWButton(
									FText::FromString(TEXT("HARİTAYI KAPAT  [M / ESC]")),
									EBNButtonVariant::PrimaryAmber,
									FOnClicked::CreateSP(this, &SBNWorldMapWidget::HandleClose),
									true, 11, FMargin(14.f, 11.f))
							]
						]
					]
				]
			]
		]
	];

	RefreshGpsSidebar();
}

void SBNWorldMapWidget::RefreshGpsSidebar()
{
	if (!GpsStatusBox.IsValid())
	{
		return;
	}
	GpsStatusBox->ClearChildren();

	ABNHUD* HUD = OwnerHUD.Get();
	UWorld* World = HUD ? HUD->GetWorld() : nullptr;
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	const ABNVehiclePawn* Car = HUD ? Cast<ABNVehiclePawn>(HUD->GetOwningPawn()) : nullptr;

	if (RoadGraph && RoadGraph->HasGpsDestination())
	{
		const float DistM = Car ? (RoadGraph->GetGpsRemainingDistanceCm(Car->GetActorLocation()) / 100.f) : 0.f;
		const FString DistStr = DistM >= 1000.f
			? FString::Printf(TEXT("%.2f km"), DistM / 1000.f)
			: FString::Printf(TEXT("%d m"), FMath::RoundToInt(DistM));

		GpsStatusBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.06f, 0.16f, 0.22f, 0.96f))
			.Padding(FMargin(12.f, 10.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(RoadGraph->GetGpsTargetLabel().ToUpper()))
					.Font(BNUITheme::Font(12, true))
					.ColorAndOpacity(BNUITheme::NosCyan)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("KALAN MESAFE: %s  (Radarda parlıyor)"), *DistStr)))
					.Font(BNUITheme::Font(10, false))
					.ColorAndOpacity(BNUITheme::ChalkWhite)
				]
			]
		];
		const UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
		const bool bCanFastTravel = !Pursuit || !Pursuit->GetState().IsPursuitRunning();

		GpsStatusBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			BNUITheme::MakeMWButton(
				FText::FromString(bCanFastTravel ? TEXT("HIZLI SEYAHAT (IŞINLAN)") : TEXT("TAKİPTE IŞINLANMA KİLİTLİ")),
				EBNButtonVariant::ConfirmGreen,
				FOnClicked::CreateSP(this, &SBNWorldMapWidget::HandleFastTravel),
				bCanFastTravel, 10, FMargin(12.f, 8.f))
		];
		GpsStatusBox->AddSlot().AutoHeight()
		[
			BNUITheme::MakeMWButton(
				FText::FromString(TEXT("GPS ROTASINI İPTAL ET")),
				EBNButtonVariant::DangerCrimson,
				FOnClicked::CreateSP(this, &SBNWorldMapWidget::HandleClearGps),
				true, 10, FMargin(12.f, 8.f))
		];
	}
	else
	{
		GpsStatusBox->AddSlot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(BNUITheme::WhiteBrush())
			.BorderBackgroundColor(BNUITheme::GunmetalCard)
			.Padding(FMargin(12.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Aktif GPS hedefi yok. Haritada bir yarışa, garaja veya yola tıkla.")))
				.Font(BNUITheme::Font(10, false))
				.ColorAndOpacity(BNUITheme::SteelMuted)
				.AutoWrapText(true)
			]
		];
	}
}

void SBNWorldMapWidget::SetDestination(const FVector& WorldPos, const FString& Label)
{
	UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiConfirm);
	if (ABNHUD* HUD = OwnerHUD.Get())
	{
		if (UWorld* World = HUD->GetWorld())
		{
			if (UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>())
			{
				const FVector PlayerPos = HUD->GetOwningPawn() ? HUD->GetOwningPawn()->GetActorLocation() : FVector::ZeroVector;
				RoadGraph->SetGpsDestination(WorldPos, Label, PlayerPos);
			}
		}
	}
	RefreshGpsSidebar();
}

FReply SBNWorldMapWidget::HandlePlotNextBossGps()
{
	ABNHUD* HUD = OwnerHUD.Get();
	UWorld* World = HUD ? HUD->GetWorld() : nullptr;
	const UBNStorySubsystem* Story = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
	if (!World || !Story)
	{
		return FReply::Handled();
	}
	const FBNBossSpec* NextBoss = FBNStoryState::FindBossByRank(Story->GetState().NextBossRank);
	if (!NextBoss || Story->GetState().bFinalChaseActive || Story->GetState().bCampaignCompleted)
	{
		return FReply::Handled();
	}
	const FName Qualifier = Story->GetState().GetNextQualifier();
	const FName NextRace = Qualifier.IsNone() ? FName(*NextBoss->RaceAssetName) : Qualifier;
	if (const ABNRaceEvent* Event = UBNStorySubsystem::FindProgramRace(World, NextRace))
	{
		SetDestination(Event->GetActorLocation(), Event->Definition->DisplayName.ToString());
	}
	return FReply::Handled();
}

FReply SBNWorldMapWidget::HandleFastTravel()
{
	ABNHUD* HUD = OwnerHUD.Get();
	UWorld* World = HUD ? HUD->GetWorld() : nullptr;
	UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	const UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
	ABNVehiclePawn* Car = HUD ? Cast<ABNVehiclePawn>(HUD->GetOwningPawn()) : nullptr;
	if (!HUD || !World || !RoadGraph || !RoadGraph->HasGpsDestination() || !Car)
	{
		return FReply::Handled();
	}
	if (Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiError);
		return FReply::Handled();
	}

	FVector Dest = RoadGraph->GetGpsTargetLocation();
	FRotator Heading = Car->GetActorRotation();
	Heading.Pitch = 0.f;
	Heading.Roll = 0.f;
	if (RoadGraph->HasGraph())
	{
		FVector RoadPt, RoadTan;
		float Width = 0.f, EdgeDist = 0.f;
		int32 EdgeIdx = INDEX_NONE;
		if (RoadGraph->FindNearestRoadPoint(Dest, RoadPt, RoadTan, Width, EdgeIdx, EdgeDist))
		{
			Dest = RoadPt;
			if (!RoadTan.IsNearlyZero())
			{
				Heading = FRotator(0.f, RoadTan.Rotation().Yaw, 0.f);
			}
		}
	}
	Car->SetActorLocationAndRotation(Dest + FVector(0.f, 0.f, 95.f), Heading, false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* Body = Car->GetMesh())
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	RoadGraph->ClearGpsDestination();
	if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
	{
		Audio->PlaySfx2D(EBNSfxId::GpsArrive, 0.9f);
	}
	HUD->CloseWorldMap();
	return FReply::Handled();
}

FReply SBNWorldMapWidget::HandleClearGps()
{
	UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiClick);
	if (ABNHUD* HUD = OwnerHUD.Get())
	{
		if (UWorld* World = HUD->GetWorld())
		{
			if (UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>())
			{
				RoadGraph->ClearGpsDestination();
			}
		}
	}
	RefreshGpsSidebar();
	return FReply::Handled();
}

FReply SBNWorldMapWidget::HandleClose()
{
	UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiClick);
	if (ABNHUD* HUD = OwnerHUD.Get())
	{
		HUD->CloseWorldMap();
	}
	return FReply::Handled();
}

FReply SBNWorldMapWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::M || Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		return HandleClose();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
