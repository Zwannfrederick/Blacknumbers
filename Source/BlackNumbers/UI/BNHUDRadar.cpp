#include "UI/BNHUD.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/SplineComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageZone.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitBreaker.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNUITheme.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"
#include "World/BNTrafficSubsystem.h"

namespace BNRadar
{
	constexpr float RadarMargin = 32.f;
	constexpr float RadarHalfSize = 96.f;
	constexpr float RadarInnerClip = 84.f;
	constexpr float FreeRoamRangeCm = 24000.f;
	constexpr float RaceRouteRangeCm = 30000.f;
	constexpr int32 RouteSplineSamples = 72;
	constexpr float LabelScale = 0.85f;
	constexpr float BannerScale = 1.5f;

	const FLinearColor RadarBg(0.0030f, 0.0033f, 0.0040f, 0.78f);
	const FLinearColor RadarRing(0.8550f, 0.8228f, 0.7454f, 0.35f);
	const FLinearColor RadarRoad(0.32f, 0.34f, 0.38f, 0.72f);
	const FLinearColor RadarHighway(0.46f, 0.48f, 0.54f, 0.85f);
	const FLinearColor RadarDirtRoad(0.48f, 0.36f, 0.22f, 0.72f);
	const FLinearColor BlipCooldown(0.18f, 0.62f, 1.00f, 0.96f);
	const FLinearColor RouteShadow(0.02f, 0.02f, 0.03f, 0.92f);
	const FLinearColor RouteAmber = BNUITheme::Accent;
	const FLinearColor RouteNextLeg(0.28f, 0.92f, 1.00f, 1.00f);
	const FLinearColor DriftZoneRing(0.20f, 0.84f, 1.00f, 0.85f);
	const FLinearColor BlipTraffic(0.70f, 0.72f, 0.76f, 0.55f);
	const FLinearColor BlipRace = BNUITheme::Accent;
	const FLinearColor BlipRival(0.96f, 0.28f, 0.14f, 1.f);
	const FLinearColor BlipFinish(0.98f, 0.96f, 0.92f, 1.f);
	const FLinearColor BlipGarage(0.16f, 0.86f, 0.44f, 1.f);
	const FLinearColor BlipBreaker(1.00f, 0.45f, 0.08f, 1.f);
	const FLinearColor BlipEscape(0.22f, 0.88f, 1.0f, 1.f);
	const FLinearColor BlipPoliceRed(1.f, 0.20f, 0.14f, 1.f);
	const FLinearColor BlipPoliceBlue(0.18f, 0.60f, 1.f, 1.f);
	const FLinearColor DashColour = BNUITheme::Accent;
	const FLinearColor SweetSpotColour = BNUITheme::Ok;
	const FLinearColor NosActiveColour(0.55f, 0.94f, 1.f, 1.f);
	const FLinearColor SpeedbreakerActiveColour(1.f, 0.92f, 0.55f, 1.f);
	const FLinearColor LegendColour(0.8550f, 0.8228f, 0.7454f, 0.80f);

	FVector2D ProjectUnclamped(const FVector& WorldPos, const FVector& PlayerPos, float CosY, float SinY,
		float CenterX, float CenterY, float RangeCm)
	{
		const float Dx = WorldPos.X - PlayerPos.X;
		const float Dy = WorldPos.Y - PlayerPos.Y;
		const float LocalFwd = Dx * CosY + Dy * SinY;
		const float LocalRight = -Dx * SinY + Dy * CosY;
		const float Scale = RadarInnerClip / RangeCm;
		return FVector2D(CenterX + LocalRight * Scale, CenterY - LocalFwd * Scale);
	}

	FVector2D ClampToRadarEdge(const FVector2D& Pt, float CenterX, float CenterY, float MaxR)
	{
		FVector2D Offset(Pt.X - CenterX, Pt.Y - CenterY);
		if (Offset.SizeSquared() > MaxR * MaxR)
		{
			Offset = Offset.GetSafeNormal() * MaxR;
		}
		return FVector2D(CenterX + Offset.X, CenterY + Offset.Y);
	}

	/** Analytical segment-vs-circle clip so roads and race routes never poke past the circular radar bezel. */
	bool ClipSegmentToCircle(FVector2D A, FVector2D B, float Cx, float Cy, float Radius, FVector2D& OutA, FVector2D& OutB)
	{
		const FVector2D D = B - A;
		const FVector2D F(A.X - Cx, A.Y - Cy);
		const float a = FVector2D::DotProduct(D, D);
		const float rSq = Radius * Radius;
		if (a < 1e-4f)
		{
			if (F.SizeSquared() <= rSq)
			{
				OutA = A;
				OutB = B;
				return true;
			}
			return false;
		}

		const float b = 2.f * FVector2D::DotProduct(F, D);
		const float c = F.SizeSquared() - rSq;
		const float Disc = b * b - 4.f * a * c;
		if (Disc < 0.f)
		{
			return false;
		}
		const float SqrtD = FMath::Sqrt(Disc);
		const float T0 = FMath::Max(0.f, (-b - SqrtD) / (2.f * a));
		const float T1 = FMath::Min(1.f, (-b + SqrtD) / (2.f * a));
		if (T1 <= T0)
		{
			return false;
		}
		OutA = A + D * T0;
		OutB = A + D * T1;
		return true;
	}
}

void ABNHUD::DrawRadar(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, const UBNPursuitSubsystem* Pursuit, UBNStorySubsystem* Story)
{
	const float Cx = BNRadar::RadarMargin + BNRadar::RadarHalfSize;
	const float Cy = Canvas->ClipY - BNRadar::RadarMargin - BNRadar::RadarHalfSize;
	const bool bRaceActive = ActiveRace && (ActiveRace->GetState() == EBNRaceState::Countdown || ActiveRace->GetState() == EBNRaceState::Racing);
	const float ActiveRangeCm = bRaceActive ? BNRadar::RaceRouteRangeCm : BNRadar::FreeRoamRangeCm;

	// Radar housing: ink disc with a thin bone ring (polyline), no ornament.
	{
		constexpr int32 RingSegs = 64;
		const float R = BNRadar::RadarHalfSize;
		for (float Dy = -R; Dy < R; Dy += 2.f)
		{
			const float HalfChord = FMath::Sqrt(FMath::Max(0.f, R * R - (Dy + 1.f) * (Dy + 1.f)));
			DrawRect(BNRadar::RadarBg, Cx - HalfChord, Cy + Dy, HalfChord * 2.f, 2.f);
		}
		for (int32 i = 0; i < RingSegs; ++i)
		{
			const float A0 = 2.f * PI * i / RingSegs;
			const float A1 = 2.f * PI * (i + 1) / RingSegs;
			DrawLine(Cx + FMath::Cos(A0) * R, Cy + FMath::Sin(A0) * R, Cx + FMath::Cos(A1) * R, Cy + FMath::Sin(A1) * R, BNRadar::RadarRing, 1.5f);
		}
	}

	const FVector PlayerLoc = Car.GetActorLocation();
	const float YawRad = FMath::DegreesToRadians(Car.GetActorRotation().Yaw);
	const float CosY = FMath::Cos(YawRad);
	const float SinY = FMath::Sin(YawRad);

	auto DrawClippedLine = [&](const FVector& W0, const FVector& W1, const FLinearColor& Col, float Thickness)
	{
		const FVector2D P0 = BNRadar::ProjectUnclamped(W0, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
		const FVector2D P1 = BNRadar::ProjectUnclamped(W1, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
		FVector2D C0, C1;
		if (BNRadar::ClipSegmentToCircle(P0, P1, Cx, Cy, BNRadar::RadarInnerClip, C0, C1))
		{
			DrawLine(C0.X, C0.Y, C1.X, C1.Y, Col, Thickness);
		}
	};

	auto PlotSquare = [this](const FVector2D& Pt, float HalfW, const FLinearColor& Col)
	{
		DrawRect(Col, Pt.X - HalfW, Pt.Y - HalfW, HalfW * 2.f, HalfW * 2.f);
	};

	auto PlotDiamond = [this](const FVector2D& Pt, float R, const FLinearColor& Col, float Thick = 2.2f)
	{
		DrawLine(Pt.X, Pt.Y - R, Pt.X + R, Pt.Y, Col, Thick);
		DrawLine(Pt.X + R, Pt.Y, Pt.X, Pt.Y + R, Col, Thick);
		DrawLine(Pt.X, Pt.Y + R, Pt.X - R, Pt.Y, Col, Thick);
		DrawLine(Pt.X - R, Pt.Y, Pt.X, Pt.Y - R, Col, Thick);
	};

	// 1. Real 2 km Karamara curved road network from UBNRoadGraphSubsystem clipped inside the circular radar
	UBNRoadGraphSubsystem* RoadGraph = GetWorld() ? GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (RoadGraph && RoadGraph->HasGraph())
	{
		if (!bRaceActive && RoadGraph->HasGpsDestination())
		{
			if (RoadGraph->UpdateGpsForPlayer(PlayerLoc))
			{
				GpsArrivalBannerUntil = GetWorld()->GetRealTimeSeconds() + 3.5;
				if (UBNAudioSubsystem* Audio = GetWorld()->GetSubsystem<UBNAudioSubsystem>())
				{
					Audio->PlaySfx2D(EBNSfxId::GpsArrive, 0.90f);
				}
			}
		}

		for (const FBNRoadEdge& Edge : RoadGraph->GetEdges())
		{
			const TArray<FVector>& Pts = Edge.SampledPoints;
			if (Pts.Num() < 2)
			{
				continue;
			}
			const FLinearColor Col = Edge.bDirt ? BNRadar::RadarDirtRoad
				: (Edge.bHighway ? BNRadar::RadarHighway : BNRadar::RadarRoad);
			const float Thick = Edge.bHighway ? 3.2f : (Edge.WidthCm >= 1400.f ? 2.2f : 1.5f);
			for (int32 i = 0; i + 1 < Pts.Num(); ++i)
			{
				DrawClippedLine(Pts[i], Pts[i + 1], Col, Thick);
			}
		}

		// Active Free-Roam A* GPS Route (plotted from [M] World Map) drawn in luminous cyan!
		if (!bRaceActive && RoadGraph->HasGpsDestination())
		{
			const TArray<FVector>& GpsPts = RoadGraph->GetGpsRoutePoints();
			for (int32 i = 0; i + 1 < GpsPts.Num(); ++i)
			{
				DrawClippedLine(GpsPts[i], GpsPts[i + 1], BNRadar::RouteShadow, 5.8f);
			}
			for (int32 i = 0; i + 1 < GpsPts.Num(); ++i)
			{
				DrawClippedLine(GpsPts[i], GpsPts[i + 1], BNRadar::RouteNextLeg, 3.2f);
			}
			const float GpsPulse = 0.5f + 0.5f * FMath::Sin((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) * 6.5f);
			const FVector2D GpsDest = BNRadar::ClampToRadarEdge(
				BNRadar::ProjectUnclamped(RoadGraph->GetGpsTargetLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
				Cx, Cy, BNRadar::RadarInnerClip - 3.f);
			PlotDiamond(GpsDest, 5.5f + GpsPulse * 2.f, BNRadar::RouteNextLeg, 2.4f);
			PlotSquare(GpsDest, 2.4f, FLinearColor::White);
		}

		// Cooldown hiding spots (blue circles/diamonds — especially prominent during police cooldown!)
		const bool bShowCooldown = !Pursuit || Pursuit->GetState().Phase == EBNPursuitPhase::Cooldown
			|| Pursuit->GetState().IsPursuitRunning();
		if (bShowCooldown)
		{
			const bool bInCooldown = Pursuit && Pursuit->GetState().Phase == EBNPursuitPhase::Cooldown;
			const float Pulse = 0.5f + 0.5f * FMath::Sin((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) * 6.f);
			for (const FBNCooldownSpot& Spot : RoadGraph->GetCooldownSpots())
			{
				const FVector2D RawPt = BNRadar::ProjectUnclamped(Spot.Location, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
				const bool bInCircle = FVector2D::DistSquared(RawPt, FVector2D(Cx, Cy)) <= BNRadar::RadarInnerClip * BNRadar::RadarInnerClip;
				if (bInCircle || bInCooldown)
				{
					const FVector2D Pt = BNRadar::ClampToRadarEdge(RawPt, Cx, Cy, BNRadar::RadarInnerClip - 3.f);
					PlotDiamond(Pt, bInCooldown ? (5.5f + Pulse * 2.0f) : 4.0f, BNRadar::BlipCooldown, bInCooldown ? 2.4f : 1.6f);
					if (bInCooldown)
					{
						PlotSquare(Pt, 2.2f, FLinearColor::White);
					}
				}
			}
		}
	}
	else if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Karamara")))
	{
		struct FRoadSeg { float X0, Y0, X1, Y1; };
		static const FRoadSeg Roads[] = {
			{ -26000.f, -14000.f, 26000.f, -14000.f },
			{ -26000.f, 14000.f, 26000.f, 14000.f },
			{ -20000.f, -26000.f, -20000.f, 20000.f },
			{ 20000.f, -26000.f, 20000.f, 20000.f },
			{ 0.f, -14000.f, 0.f, 14000.f },
			{ -27000.f, -24000.f, 27000.f, -24000.f },
		};
		for (const FRoadSeg& R : Roads)
		{
			DrawClippedLine(FVector(R.X0, R.Y0, 0.f), FVector(R.X1, R.Y1, 0.f), BNRadar::RadarRoad, 2.2f);
		}
	}

	// 2. Full glowing Most Wanted Race Route on the minimap during active races!
	if (bRaceActive && ActiveRace->Definition)
	{
		const UBNRaceDefinition* Def = ActiveRace->Definition;
		if (Def->Discipline == EBNRaceDiscipline::Drift)
		{
			// Drift arena perimeter ring around beacon
			const FVector Center = ActiveRace->GetActorLocation();
			constexpr int32 Segs = 24;
			constexpr float DriftRadiusCm = 7500.f;
			for (int32 i = 0; i < Segs; ++i)
			{
				const float A0 = UE_TWO_PI * i / Segs;
				const float A1 = UE_TWO_PI * (i + 1) / Segs;
				const FVector W0 = Center + FVector(FMath::Cos(A0) * DriftRadiusCm, FMath::Sin(A0) * DriftRadiusCm, 0.f);
				const FVector W1 = Center + FVector(FMath::Cos(A1) * DriftRadiusCm, FMath::Sin(A1) * DriftRadiusCm, 0.f);
				DrawClippedLine(W0, W1, BNRadar::DriftZoneRing, 2.8f);
			}
		}
		else
		{
			TArray<FVector> RoutePts;
			const USplineComponent* Spline = ActiveRace->GetRacingLine();
			const bool bClosed = Def->Discipline == EBNRaceDiscipline::Circuit;
			if (Spline && Spline->GetSplineLength() > 100.f)
			{
				const float TotalLen = Spline->GetSplineLength();
				RoutePts.Reserve(BNRadar::RouteSplineSamples + 1);
				for (int32 i = 0; i <= BNRadar::RouteSplineSamples; ++i)
				{
					const float Dist = (TotalLen * i) / BNRadar::RouteSplineSamples;
					RoutePts.Add(Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World));
				}
			}
			else
			{
				if (!bClosed)
				{
					RoutePts.Add(Def->Grid.GetLocation());
				}
				for (const FTransform& Gate : Def->Checkpoints)
				{
					RoutePts.Add(Gate.GetLocation());
				}
				if (bClosed && RoutePts.Num() >= 2)
				{
					const FVector FirstPt = RoutePts[0];
					RoutePts.Add(FirstPt);
				}
			}

			// Contrast shadow pass then luminous Most Wanted amber GPS route pass
			for (int32 i = 0; i + 1 < RoutePts.Num(); ++i)
			{
				DrawClippedLine(RoutePts[i], RoutePts[i + 1], BNRadar::RouteShadow, 6.2f);
			}
			for (int32 i = 0; i + 1 < RoutePts.Num(); ++i)
			{
				DrawClippedLine(RoutePts[i], RoutePts[i + 1], BNRadar::RouteAmber, 3.2f);
			}

			// Highlight immediate upcoming curved route along the spline for instant corner reads
			const int32 NextIdx = ActiveRace->GetProgress().NextCheckpoint();
			if (Spline && Spline->GetSplineLength() > 100.f)
			{
				const float TotalLen = Spline->GetSplineLength();
				const float Key = Spline->FindInputKeyClosestToWorldLocation(PlayerLoc);
				const float PlayerDist = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
				const float LookAheadLen = FMath::Min(TotalLen * 0.45f, 34000.f);
				constexpr int32 AheadSteps = 22;
				FVector PrevPt = Spline->GetLocationAtDistanceAlongSpline(PlayerDist, ESplineCoordinateSpace::World);
				for (int32 s = 1; s <= AheadSteps; ++s)
				{
					float SampleDist = PlayerDist + (LookAheadLen * s) / AheadSteps;
					if (bClosed)
					{
						SampleDist = FMath::Fmod(SampleDist, TotalLen);
					}
					else if (SampleDist > TotalLen)
					{
						SampleDist = TotalLen;
					}
					const FVector CurPt = Spline->GetLocationAtDistanceAlongSpline(SampleDist, ESplineCoordinateSpace::World);
					DrawClippedLine(PrevPt, CurPt, BNRadar::RouteNextLeg, 4.0f);
					PrevPt = CurPt;
					if (!bClosed && SampleDist >= TotalLen)
					{
						break;
					}
				}
			}
			else if (Def->Checkpoints.IsValidIndex(NextIdx))
			{
				DrawClippedLine(PlayerLoc, Def->Checkpoints[NextIdx].GetLocation(), BNRadar::RouteNextLeg, 3.6f);
			}

			// Checkpoint nodes, finish flag diamond, and pulsing active target gate
			const float Pulse = 0.5f + 0.5f * FMath::Sin((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) * 7.f);
			for (int32 i = 0; i < Def->Checkpoints.Num(); ++i)
			{
				const FVector GateLoc = Def->Checkpoints[i].GetLocation();
				const FVector2D RawPt = BNRadar::ProjectUnclamped(GateLoc, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
				const bool bInCircle = FVector2D::DistSquared(RawPt, FVector2D(Cx, Cy)) <= BNRadar::RadarInnerClip * BNRadar::RadarInnerClip;
				if (i == NextIdx)
				{
					const FVector2D TargetPt = BNRadar::ClampToRadarEdge(RawPt, Cx, Cy, BNRadar::RadarInnerClip - 2.f);
					PlotSquare(TargetPt, 3.5f, BNRadar::RouteNextLeg);
					PlotDiamond(TargetPt, 6.f + Pulse * 2.5f, BNRadar::BlipRace, 2.2f);
				}
				else if (!bClosed && i == Def->Checkpoints.Num() - 1 && bInCircle)
				{
					PlotSquare(RawPt, 4.5f, BNRadar::BlipFinish);
					PlotDiamond(RawPt, 6.5f, BNRadar::BlipRival, 2.f);
				}
				else if (bInCircle)
				{
					PlotSquare(RawPt, 2.5f, BNRadar::BlipRace);
				}
			}

			// Rival racer blips during the race
			for (int32 r = 0; r < ActiveRace->GetNumRivals(); ++r)
			{
				if (const ABNVehiclePawn* RivalCar = ActiveRace->GetRival(r).Car.Get())
				{
					const FVector2D RawRival = BNRadar::ProjectUnclamped(RivalCar->GetActorLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
					const FVector2D ClampedRival = BNRadar::ClampToRadarEdge(RawRival, Cx, Cy, BNRadar::RadarInnerClip - 3.f);
					PlotSquare(ClampedRival, 4.2f, BNRadar::RouteShadow);
					PlotSquare(ClampedRival, 3.0f, BNRadar::BlipRival);
				}
			}
		}
	}

	// 3. World entities (traffic, free-roam beacons, garages, pursuit breakers & police)
	if (GetWorld())
	{
		if (const UBNTrafficSubsystem* Traffic = GetWorld()->GetSubsystem<UBNTrafficSubsystem>())
		{
			TArray<FVector> TrafficLocs;
			Traffic->GetTrafficLocations(TrafficLocs);
			const float RangeSq = ActiveRangeCm * ActiveRangeCm;
			for (const FVector& Loc : TrafficLocs)
			{
				if (FVector::DistSquared2D(PlayerLoc, Loc) <= RangeSq)
				{
					const FVector2D Pt = BNRadar::ProjectUnclamped(Loc, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
					if (FVector2D::DistSquared(Pt, FVector2D(Cx, Cy)) <= BNRadar::RadarInnerClip * BNRadar::RadarInnerClip)
					{
						PlotSquare(Pt, 1.8f, BNRadar::BlipTraffic);
					}
				}
			}
		}

		if (!bRaceActive)
		{
			for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
			{
				if (It->GetState() == EBNRaceState::Idle)
				{
					const FVector2D Pt = BNRadar::ClampToRadarEdge(
						BNRadar::ProjectUnclamped(It->GetActorLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
						Cx, Cy, BNRadar::RadarInnerClip - 3.f);
					PlotSquare(Pt, 3.5f, BNRadar::BlipRace);
				}
			}
		}

		for (TActorIterator<ABNGarageZone> It(GetWorld()); It; ++It)
		{
			const FVector2D Pt = BNRadar::ClampToRadarEdge(
				BNRadar::ProjectUnclamped(It->GetActorLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
				Cx, Cy, BNRadar::RadarInnerClip - 3.f);
			PlotSquare(Pt, 4.f, BNRadar::BlipGarage);
		}

		if (Pursuit && Pursuit->GetState().IsPursuitRunning())
		{
			// Armed Pursuit Breaker triangles on radar during active pursuit (Most Wanted signature!)
			for (TActorIterator<ABNPursuitBreaker> It(GetWorld()); It; ++It)
			{
				if (!It->IsCollapsed())
				{
					const FVector2D Pt = BNRadar::ProjectUnclamped(It->GetActorLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm);
					if (FVector2D::DistSquared(Pt, FVector2D(Cx, Cy)) <= BNRadar::RadarInnerClip * BNRadar::RadarInnerClip)
					{
						DrawLine(Pt.X, Pt.Y - 5.f, Pt.X - 4.5f, Pt.Y + 4.f, BNRadar::BlipBreaker, 2.f);
						DrawLine(Pt.X, Pt.Y - 5.f, Pt.X + 4.5f, Pt.Y + 4.f, BNRadar::BlipBreaker, 2.f);
						DrawLine(Pt.X - 4.5f, Pt.Y + 4.f, Pt.X + 4.5f, Pt.Y + 4.f, BNRadar::BlipBreaker, 2.f);
					}
				}
			}

			const bool bFlash = FMath::Fmod(GetWorld()->GetTimeSeconds() * 6.0, 1.0) < 0.5;
			// While evading/cooling down, show each unit's real re-spot radius so the player can read where to hide.
			const float SpotRadiusCm = Pursuit ? FBNPursuitState::GetSpotRadiusCm(Pursuit->GetState().Phase) : 0.f;
			for (TActorIterator<ABNPoliceController> It(GetWorld()); It; ++It)
			{
				if (const APawn* Cop = It->GetPawn(); Cop && !It->IsUnitDisabled())
				{
					if (SpotRadiusCm > 0.f)
					{
						constexpr int32 RingSegs = 28;
						const FVector C = Cop->GetActorLocation();
						const FLinearColor RingCol(BNRadar::BlipPoliceRed.R, BNRadar::BlipPoliceRed.G, BNRadar::BlipPoliceRed.B, 0.55f);
						for (int32 i = 0; i < RingSegs; ++i)
						{
							const float A0 = 2.f * PI * i / RingSegs;
							const float A1 = 2.f * PI * (i + 1) / RingSegs;
							DrawClippedLine(C + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * SpotRadiusCm,
								C + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * SpotRadiusCm, RingCol, 1.6f);
						}
						DrawClippedLine(C, C + Cop->GetActorForwardVector().GetSafeNormal2D() * SpotRadiusCm, RingCol, 1.6f);
					}
					const FVector2D Pt = BNRadar::ClampToRadarEdge(
						BNRadar::ProjectUnclamped(Cop->GetActorLocation(), PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
						Cx, Cy, BNRadar::RadarInnerClip - 3.f);
					PlotSquare(Pt, 4.2f, BNRadar::RouteShadow);
					PlotSquare(Pt, 3.2f, bFlash ? BNRadar::BlipPoliceRed : BNRadar::BlipPoliceBlue);
				}
			}

			// Roadblock tactical X warning icon on circular radar
			float RbDistM = 0.f;
			bool bRbSpikes = false;
			FVector RbCenter = FVector::ZeroVector;
			if (Pursuit->GetNearestRoadblockAhead(Car, RbDistM, bRbSpikes, &RbCenter))
			{
				const FVector2D RbPt = BNRadar::ClampToRadarEdge(
					BNRadar::ProjectUnclamped(RbCenter, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
					Cx, Cy, BNRadar::RadarInnerClip - 3.f);
				const FLinearColor RbCol = bFlash ? BNRadar::BlipPoliceRed : BNRadar::BlipBreaker;
				DrawLine(RbPt.X - 5.5f, RbPt.Y - 5.5f, RbPt.X + 5.5f, RbPt.Y + 5.5f, RbCol, 2.8f);
				DrawLine(RbPt.X + 5.5f, RbPt.Y - 5.5f, RbPt.X - 5.5f, RbPt.Y + 5.5f, RbCol, 2.8f);
			}
		}
	}

	if (Story && Story->GetState().bFinalChaseActive)
	{
		for (const int32 RouteIdx : { 0, 1 })
		{
			const FVector ExitLoc = UBNStorySubsystem::GetEscapeRouteLocation(GetWorld(), RouteIdx);
			const FVector2D Pt = BNRadar::ClampToRadarEdge(
				BNRadar::ProjectUnclamped(ExitLoc, PlayerLoc, CosY, SinY, Cx, Cy, ActiveRangeCm),
				Cx, Cy, BNRadar::RadarInnerClip - 3.f);
			PlotDiamond(Pt, 6.f, BNRadar::BlipEscape, 2.5f);
		}
	}

	// Player directional chevron at center
	DrawLine(Cx, Cy - 8.f, Cx - 5.5f, Cy + 6.f, BNRadar::RouteShadow, 4.f);
	DrawLine(Cx, Cy - 8.f, Cx + 5.5f, Cy + 6.f, BNRadar::RouteShadow, 4.f);
	DrawLine(Cx, Cy - 8.f, Cx - 5.5f, Cy + 6.f, FLinearColor::White, 2.2f);
	DrawLine(Cx, Cy - 8.f, Cx + 5.5f, Cy + 6.f, FLinearColor::White, 2.2f);
	DrawLine(Cx - 5.5f, Cy + 6.f, Cx + 5.5f, Cy + 6.f, BNRadar::RadarRing, 2.f);

	// Current district pill right above radar + brief top-center District Entry Banner on boundary crossing
	if (RoadGraph && RoadGraph->HasGraph() && GetWorld())
	{
		FString DistName, DistSub;
		const EBNDistrict CurDist = RoadGraph->GetDistrictAt(PlayerLoc, &DistName, &DistSub);
		const int32 DistInt = static_cast<int32>(CurDist);
		const double Now = GetWorld()->GetRealTimeSeconds();
		if (LastDistrictId != DistInt)
		{
			if (LastDistrictId >= 0)
			{
				DistrictBannerName = DistName;
				DistrictBannerSubtitle = DistSub;
				DistrictBannerRuler.Reset();
				bDistrictBannerLiberated = false;
				if (Story)
				{
					const FBNDistrictTerritoryInfo Terr = Story->GetState().GetDistrictTerritory(CurDist);
					bDistrictBannerLiberated = Terr.bDistrictLiberated;
					DistrictBannerRuler = Terr.bDistrictLiberated
						? FString::Printf(TEXT("BÖLGE ELE GEÇİRİLDİ  •  %s KONTROLÜNDE"), *Terr.BoroughTitle)
						: FString::Printf(TEXT("BÖLGE HÜKÜMDARI: #%02d %s  (%s)  •  EKİP: %s"),
							Terr.RulerRank, *BNUITheme::UpperTr(Terr.RulerName), *Terr.RulerVehicle, *Terr.CrewName);
				}
				DistrictBannerUntil = Now + 3.8;
			}
			LastDistrictId = DistInt;
			if (Story && !bRaceActive)
			{
				Story->NotifyDistrictEntered(DistInt);
			}
		}

		UFont* Font = BNUITheme::HudFont();
		float W = 0.f, H = 0.f;
		GetTextSize(DistName, W, H, Font, 0.74f);
		const float PillX = BNRadar::RadarMargin;
		const float PillY = Cy - BNRadar::RadarHalfSize - H - 10.f;
		DrawTextShadowed(BNUITheme::UpperTr(DistName), BNRadar::LegendColour, PillX, PillY, Font, 0.74f);

		if (Now < DistrictBannerUntil && !DistrictBannerName.IsEmpty() && !bRaceActive)
		{
			const float Remaining = static_cast<float>(DistrictBannerUntil - Now);
			const float Alpha = FMath::Clamp(FMath::Min(Remaining, 3.8f - Remaining) * 2.5f, 0.f, 1.f);
			FLinearColor TitleCol = BNRadar::DashColour;
			TitleCol.A = Alpha;
			FLinearColor SubCol = BNRadar::LegendColour;
			SubCol.A = Alpha * 0.9f;
			DrawCentred(DistrictBannerName, 92.f, 0.95f, FLinearColor(BNUITheme::Bone.R, BNUITheme::Bone.G, BNUITheme::Bone.B, Alpha), BNUITheme::HudFont(true));
			DrawCentred(DistrictBannerSubtitle, 168.f, 0.78f, SubCol);
			if (!DistrictBannerRuler.IsEmpty())
			{
				FLinearColor RulerCol = bDistrictBannerLiberated ? BNRadar::SweetSpotColour : BNRadar::DashColour;
				RulerCol.A = Alpha * 0.95f;
				DrawCentred(DistrictBannerRuler, 194.f, 0.74f, RulerCol);
			}
		}
	}
}

void ABNHUD::DrawWorldMarkers(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, const UBNStorySubsystem* Story)
{
	UFont* Font = BNUITheme::HudFont();
	const FVector PlayerLoc = Car.GetActorLocation();

	auto DrawProjectedTag = [&](const FVector& WorldPos, const FString& Title, const FLinearColor& Col)
	{
		FVector Projected = Canvas->Project(WorldPos + FVector(0.f, 0.f, 260.f));
		if (Projected.Z <= 0.f)
		{
			Projected.X = Canvas->ClipX - Projected.X;
			Projected.Y = Canvas->ClipY - 80.f;
		}
		const float ClampedX = FMath::Clamp(Projected.X, 90.f, Canvas->ClipX - 90.f);
		const float ClampedY = FMath::Clamp(Projected.Y, 90.f, Canvas->ClipY - 120.f);
		const int32 DistMeters = FMath::RoundToInt(FVector::Dist2D(PlayerLoc, WorldPos) / 100.f);
		const FString Label = FString::Printf(TEXT("%s  %dm"), *Title, DistMeters);
		float W = 0.f, H = 0.f;
		GetTextSize(Label, W, H, Font, 0.80f);
		const float X = ClampedX - W * 0.5f - 10.f;
		const float Y = ClampedY - H * 0.5f - 5.f;
		DrawRect(BNRadar::RadarBg, X, Y, W + 20.f, H + 10.f);
		DrawRect(Col, X, Y, 3.f, H + 10.f);
		DrawText(Label, Col, X + 10.f, Y + 5.f, Font, 0.80f);
	};

	if (ActiveRace && ActiveRace->GetState() == EBNRaceState::Racing && ActiveRace->Definition)
	{
		const int32 Next = ActiveRace->GetProgress().NextCheckpoint();
		if (ActiveRace->Definition->Checkpoints.IsValidIndex(Next))
		{
			const FString GateTag = FString::Printf(TEXT("KAPI %d/%d"), Next + 1, ActiveRace->Definition->Checkpoints.Num());
			DrawProjectedTag(ActiveRace->Definition->Checkpoints[Next].GetLocation(), GateTag, BNRadar::BlipRace);
		}
	}

	if (!ActiveRace && GetWorld())
	{
		if (const UBNRoadGraphSubsystem* RoadGraph = GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>();
			RoadGraph && RoadGraph->HasGpsDestination())
		{
			DrawProjectedTag(RoadGraph->GetGpsTargetLocation(),
				FString::Printf(TEXT("GPS: %s"), *BNUITheme::UpperTr(RoadGraph->GetGpsTargetLabel().Left(24))),
				BNRadar::RouteNextLeg);
		}
	}

	if (Story && Story->GetState().bFinalChaseActive)
	{
		DrawProjectedTag(UBNStorySubsystem::GetEscapeRouteLocation(GetWorld(), 0), TEXT("ROTA A: LIMAN"), BNRadar::BlipEscape);
		DrawProjectedTag(UBNStorySubsystem::GetEscapeRouteLocation(GetWorld(), 1), TEXT("ROTA B: KUZEY"), BNRadar::BlipEscape);
	}
}

void ABNHUD::DrawEconomyPill(const UBNProgressionSubsystem& Progression, const UBNStorySubsystem* Story, bool bRaceActive, bool bPursuitVisible)
{
	if (bPursuitVisible || bRaceActive)
	{
		return;
	}
	UFont* Font = BNUITheme::HudFont();
	const FBNPlayerProfile& P = Progression.GetProfile();
	const int32 Bounty = Story ? Story->GetState().TotalBounty : 0;
	UFont* Digits = BNUITheme::HudFont(true);
	const FString CashText = FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(P.Cash));
	const FString BountyText = FString::Printf(TEXT("ÖDÜL  %s"), *FString::FormatAsNumber(Bounty));
	float W = 0.f, H = 0.f;
	GetTextSize(CashText, W, H, Digits, 0.62f);
	const float X = BNRadar::RadarMargin;
	const float Y = BNRadar::RadarMargin - 8.f;
	DrawTextShadowed(CashText, BNUITheme::Bone, X, Y, Digits, 0.62f);
	float BW = 0.f, BH = 0.f;
	GetTextSize(BountyText, BW, BH, Font, 0.78f);
	DrawTextShadowed(BountyText, BNRadar::DashColour, X + W + 16.f, Y + H - BH - 4.f, Font, 0.78f);

	float NextY = Y + H + 2.f;
	if (Story)
	{
		const FString ObjText = Story->GetState().GetCurrentObjectiveLine();
		if (!ObjText.IsEmpty())
		{
			const bool bDuelReady = Story->GetState().IsDuelUnlocked(Story->GetState().NextBossRank);
			float OW = 0.f, OH = 0.f;
			GetTextSize(ObjText, OW, OH, Font, 0.74f);
			DrawTextShadowed(ObjText, bDuelReady ? BNRadar::SweetSpotColour : BNRadar::LegendColour, X, NextY, Font, 0.72f);
			NextY += OH + 6.f;
		}
	}

	if (Progression.GetRewardBannerSeconds() > 0.f && !Progression.GetRewardBanner().IsEmpty())
	{
		DrawTextShadowed(Progression.GetRewardBanner(), BNRadar::SweetSpotColour, X, NextY, Font, 0.90f);
	}
}

void ABNHUD::DrawLiveCommsAndPrompts(const ABNVehiclePawn& Car, const ABNRaceEvent* ActiveRace, const UBNStorySubsystem* Story)
{
	UFont* Font = BNUITheme::HudFont();
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;

	// 1. GPS Arrival Banner (shifts cleanly below the 3-line District Entry Banner if both fire together)
	if (Now < GpsArrivalBannerUntil)
	{
		const float GpsY = (Now < DistrictBannerUntil && !DistrictBannerName.IsEmpty()) ? 228.f : 168.f;
		DrawCentred(TEXT("HEDEFE ULAŞILDI"), GpsY, 0.7f, BNRadar::RouteNextLeg, BNUITheme::HudFont(true));
	}

	// 2. Interactive Free-Roam Pre-Race Briefing Prompt Pill (when parked inside an idle race ring)
	if (!ActiveRace && GetWorld())
	{
		if (const ABNRaceEvent* Nearby = ABNRaceEvent::FindNearbyIdleRace(GetWorld(), &Car); Nearby && Nearby->Definition)
		{
			const UBNRaceDefinition* Def = Nearby->Definition;
			const TCHAR* Disc = Def->Discipline == EBNRaceDiscipline::Drag ? TEXT("DRAG")
				: Def->Discipline == EBNRaceDiscipline::Drift ? TEXT("DRİFT")
				: Def->Discipline == EBNRaceDiscipline::Circuit ? TEXT("PİST") : TEXT("SPRINT");
			const FString Prompt = FString::Printf(TEXT("[ENTER]  YARIŞ ÖZETİ  —  %s  (%s)"),
				*BNUITheme::UpperTr(Def->DisplayName.ToString()), Disc);
			float PW = 0.f, PH = 0.f;
			GetTextSize(Prompt, PW, PH, Font, 1.02f);
			const float BoxW = PW + 36.f;
			const float BoxH = PH + 16.f;
			const float BoxX = (Canvas->ClipX - BoxW) * 0.5f;
			const float BoxY = Canvas->ClipY - 126.f;
			DrawRect(BNRadar::RadarBg, BoxX, BoxY, BoxW, BoxH);
			DrawRect(BNRadar::DashColour, BoxX, BoxY + BoxH - 3.f, BoxW, 3.f);
			DrawText(Prompt, BNUITheme::Bone, BoxX + 18.f, BoxY + 8.f, Font, 1.02f);
		}
	}

	// 3. Non-blocking Live Crew / Rival Comms Banner on an ink slab above the radar
	if (Story)
	{
		if (const FBNLiveComms* Comms = Story->GetActiveComms())
		{
			const FString Header = FString::Printf(TEXT("TELSİZ // %s"), *Comms->Speaker);
			float HW = 0.f, HH = 0.f, MW = 0.f, MH = 0.f;
			GetTextSize(Header, HW, HH, Font, 0.74f);
			GetTextSize(Comms->Message, MW, MH, Font, 0.76f);
			const float BoxW = FMath::Clamp(FMath::Max(HW, MW) + 56.f, 300.f, 660.f);
			const float BoxH = HH + MH + 20.f;
			const float BoxX = BNRadar::RadarMargin;
			const float BoxY = Canvas->ClipY - BNRadar::RadarMargin - BNRadar::RadarHalfSize * 2.f - BoxH - 44.f;
			DrawRect(BNRadar::RadarBg, BoxX, BoxY, BoxW, BoxH);
			DrawRect(Comms->AccentColor, BoxX, BoxY, 3.f, BoxH);
			DrawText(Header, Comms->AccentColor, BoxX + 10.f, BoxY + 5.f, Font, 0.74f);
			DrawText(Comms->Message, BNUITheme::Bone, BoxX + 10.f, BoxY + HH + 8.f, Font, 0.76f);
		}
	}
}

void ABNHUD::DrawActionBanners(const ABNVehiclePawn& Car, const UBNPursuitSubsystem* Pursuit, float BaseY)
{
	float RbDistM = 0.f;
	bool bRbSpikes = false;
	if (Pursuit && Pursuit->GetNearestRoadblockAhead(Car, RbDistM, bRbSpikes))
	{
		const bool bFlash = GetWorld() && FMath::Fmod(GetWorld()->GetTimeSeconds() * 7.0, 1.0) < 0.55;
		const int32 DistM = FMath::Max(10, FMath::RoundToInt(RbDistM / 5.f) * 5);
		const FString Alert = bRbSpikes
			? FString::Printf(TEXT("DİKENLİ BARİKAT ÖNÜNDE — %dm  [X] SPEEDBREAKER"), DistM)
			: FString::Printf(TEXT("POLİS BARİKATI ÖNÜNDE — %dm"), DistM);
		DrawCentred(Alert, BaseY, BNRadar::BannerScale * 0.85f, bFlash ? BNRadar::BlipPoliceRed : BNRadar::DashColour);
		BaseY += 32.f;
	}

	if (Pursuit && Pursuit->GetTakedownBannerSeconds() > 0.f)
	{
		DrawCentred(TEXT("POLİS DEVRE DIŞI!  +$5,000"), BaseY, BNRadar::BannerScale * 1.05f, BNRadar::BlipBreaker);
	}
	else if (Car.GetLaunchBannerSeconds() > 0.f)
	{
		DrawCentred(TEXT("MÜKEMMEL KALKIŞ!"), BaseY, BNRadar::BannerScale * 1.1f, BNRadar::DashColour);
	}
	else if (Car.GetDriftExitBannerSeconds() > 0.f)
	{
		DrawCentred(TEXT("DRİFT ÇIKIŞI  +NOS"), BaseY, BNRadar::BannerScale, BNRadar::NosActiveColour);
	}
	else if (Car.GetNearMissBannerSeconds() > 0.f)
	{
		const int32 Streak = Car.GetNearMissStreak();
		const FString Label = Streak > 1
			? FString::Printf(TEXT("SIFIR GEÇİŞ x%d  +NOS"), Streak)
			: FString(TEXT("SIFIR GEÇİŞ  +NOS"));
		DrawCentred(Label, BaseY, BNRadar::BannerScale, BNRadar::NosActiveColour);
	}
	else if (Car.IsSpeedbreakerActive())
	{
		DrawCentred(TEXT("SPEEDBREAKER"), BaseY, BNRadar::BannerScale * 0.9f, BNRadar::SpeedbreakerActiveColour);
	}
}

void ABNHUD::DrawCameraModePill(const ABNVehiclePawn& Car)
{
	const FString CamLabel = Car.GetCameraModeLabel();
	if (CamLabel.IsEmpty())
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (CamLabel != LastCameraLabel)
	{
		const bool bFirst = LastCameraLabel.IsEmpty();
		LastCameraLabel = CamLabel;
		CameraLabelUntil = bFirst ? 0.0 : Now + 2.0; // only announce real changes, not the spawn default
	}
	if (Now >= CameraLabelUntil)
	{
		return;
	}
	UFont* Font = BNUITheme::HudFont();
	const FString Text = FString::Printf(TEXT("[C] %s"), *CamLabel);
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, 0.72f);
	const float RightEdge = Canvas->ClipX - BNRadar::RadarMargin;
	const float Y = Canvas->ClipY - BNRadar::RadarMargin - 252.f;
	DrawText(Text, FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), RightEdge - W - 5.f, Y + 1.f, Font, 0.72f);
	DrawText(Text, BNRadar::LegendColour, RightEdge - W - 6.f, Y, Font, 0.72f);
}
