#include "World/BNRoadGraph.h"

#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace
{
	const TCHAR* const KaramaraGraphPackage = TEXT("/Game/World/DA_KaramaraRoadGraph");
	const TCHAR* const KaramaraGraphAssetPath = TEXT("/Game/World/DA_KaramaraRoadGraph.DA_KaramaraRoadGraph");

	const TArray<FBNRoadNode> EmptyNodes;
	const TArray<FBNRoadEdge> EmptyEdges;
	const TArray<FBNCooldownSpot> EmptySpots;
	const TArray<FBNDistrictBounds> EmptyDistricts;
}

void UBNRoadGraphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TryLoadDefaultGraph();
}

void UBNRoadGraphSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!GraphAsset)
	{
		TryLoadDefaultGraph();
	}
}

void UBNRoadGraphSubsystem::TryLoadDefaultGraph()
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetMapName().Contains(TEXT("Karamara")))
	{
		return;
	}
	if (FPackageName::DoesPackageExist(KaramaraGraphPackage))
	{
		if (UBNRoadGraphAsset* Loaded = LoadObject<UBNRoadGraphAsset>(nullptr, KaramaraGraphAssetPath))
		{
			SetRoadGraphAsset(Loaded);
		}
	}
}

void UBNRoadGraphSubsystem::SetRoadGraphAsset(UBNRoadGraphAsset* InAsset)
{
	GraphAsset = InAsset;
	RebuildIndex();
}

void UBNRoadGraphSubsystem::RebuildIndex()
{
	NodeIdToIndex.Reset();
	Adjacency.Reset();
	EdgeLengthsCm.Reset();
	EdgeCumulativeDist.Reset();

	if (!GraphAsset)
	{
		return;
	}

	const int32 NumNodes = GraphAsset->Nodes.Num();
	Adjacency.SetNum(NumNodes);
	for (int32 i = 0; i < NumNodes; ++i)
	{
		NodeIdToIndex.Add(GraphAsset->Nodes[i].Id, i);
	}

	const int32 NumEdges = GraphAsset->Edges.Num();
	EdgeLengthsCm.SetNumZeroed(NumEdges);
	EdgeCumulativeDist.SetNum(NumEdges);

	for (int32 e = 0; e < NumEdges; ++e)
	{
		const FBNRoadEdge& Edge = GraphAsset->Edges[e];
		TArray<float>& Cum = EdgeCumulativeDist[e];
		float TotalLen = 0.f;
		if (Edge.SampledPoints.Num() >= 2)
		{
			Cum.Reserve(Edge.SampledPoints.Num());
			Cum.Add(0.f);
			for (int32 k = 1; k < Edge.SampledPoints.Num(); ++k)
			{
				TotalLen += FVector::Dist(Edge.SampledPoints[k - 1], Edge.SampledPoints[k]);
				Cum.Add(TotalLen);
			}
		}
		EdgeLengthsCm[e] = TotalLen;

		const int32* IdxA = NodeIdToIndex.Find(Edge.NodeA);
		const int32* IdxB = NodeIdToIndex.Find(Edge.NodeB);
		if (!IdxA || !IdxB || *IdxA == *IdxB)
		{
			continue;
		}
		const float EdgeCost = FMath::Max(TotalLen, FVector::Dist(GraphAsset->Nodes[*IdxA].Position, GraphAsset->Nodes[*IdxB].Position));
		// Slightly prefer paved/faster roads in A* cost
		const float SpeedWeight = Edge.bDirt ? 1.25f : (Edge.bHighway ? 0.88f : 1.0f);
		Adjacency[*IdxA].Add({ *IdxB, e, EdgeCost * SpeedWeight, false });
		if (!Edge.bOneWay)
		{
			Adjacency[*IdxB].Add({ *IdxA, e, EdgeCost * SpeedWeight, true });
		}
	}
}

const TArray<FBNRoadNode>& UBNRoadGraphSubsystem::GetNodes() const
{
	return GraphAsset ? GraphAsset->Nodes : EmptyNodes;
}

const TArray<FBNRoadEdge>& UBNRoadGraphSubsystem::GetEdges() const
{
	return GraphAsset ? GraphAsset->Edges : EmptyEdges;
}

const TArray<FBNCooldownSpot>& UBNRoadGraphSubsystem::GetCooldownSpots() const
{
	return GraphAsset ? GraphAsset->CooldownSpots : EmptySpots;
}

const TArray<FBNDistrictBounds>& UBNRoadGraphSubsystem::GetDistricts() const
{
	return GraphAsset ? GraphAsset->Districts : EmptyDistricts;
}

int32 UBNRoadGraphSubsystem::FindNearestNode(const FVector& Pos) const
{
	if (!HasGraph())
	{
		return INDEX_NONE;
	}
	int32 BestIdx = INDEX_NONE;
	float BestDistSq = BIG_NUMBER;
	const TArray<FBNRoadNode>& Nodes = GraphAsset->Nodes;
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const float D2 = FVector::DistSquared(Pos, Nodes[i].Position);
		if (D2 < BestDistSq)
		{
			BestDistSq = D2;
			BestIdx = i;
		}
	}
	return BestIdx;
}

bool UBNRoadGraphSubsystem::FindNearestRoadPoint(const FVector& Pos, FVector& OutPoint, FVector& OutTangent,
	float& OutWidthCm, int32& OutEdgeIdx, float& OutDistanceAlongEdge) const
{
	if (!HasGraph())
	{
		return false;
	}

	float BestDistSq = BIG_NUMBER;
	OutEdgeIdx = INDEX_NONE;
	OutDistanceAlongEdge = 0.f;
	OutWidthCm = 1400.f;
	OutPoint = Pos;
	OutTangent = FVector::ForwardVector;

	const TArray<FBNRoadEdge>& Edges = GraphAsset->Edges;
	for (int32 e = 0; e < Edges.Num(); ++e)
	{
		const FBNRoadEdge& Edge = Edges[e];
		const TArray<FVector>& Pts = Edge.SampledPoints;
		if (Pts.Num() < 2)
		{
			continue;
		}
		for (int32 k = 0; k < Pts.Num() - 1; ++k)
		{
			const FVector& A = Pts[k];
			const FVector& B = Pts[k + 1];
			const FVector AB = B - A;
			const float LenSq = AB.SizeSquared();
			const float T = LenSq > KINDA_SMALL_NUMBER
				? FMath::Clamp(FVector::DotProduct(Pos - A, AB) / LenSq, 0.f, 1.f)
				: 0.f;
			const FVector Closest = A + AB * T;
			// Weight vertical difference strongly so bridges/overpasses don't snap to ground below
			const FVector Diff = Pos - Closest;
			const float ScoreSq = Diff.X * Diff.X + Diff.Y * Diff.Y + (Diff.Z * 2.2f) * (Diff.Z * 2.2f);
			if (ScoreSq < BestDistSq)
			{
				BestDistSq = ScoreSq;
				OutPoint = Closest;
				OutTangent = AB.GetSafeNormal();
				OutWidthCm = Edge.WidthCm;
				OutEdgeIdx = e;
				const float SegStartDist = EdgeCumulativeDist.IsValidIndex(e) && EdgeCumulativeDist[e].IsValidIndex(k)
					? EdgeCumulativeDist[e][k] : 0.f;
				OutDistanceAlongEdge = SegStartDist + FMath::Sqrt(LenSq) * T;
			}
		}
	}
	return OutEdgeIdx != INDEX_NONE;
}

float UBNRoadGraphSubsystem::GetEdgeLengthCm(int32 EdgeIdx) const
{
	return EdgeLengthsCm.IsValidIndex(EdgeIdx) ? EdgeLengthsCm[EdgeIdx] : 0.f;
}

bool UBNRoadGraphSubsystem::SampleEdgeAtDistance(int32 EdgeIdx, float DistanceCm, bool bReverse,
	FVector& OutLocation, FVector& OutTangent) const
{
	if (!GraphAsset || !GraphAsset->Edges.IsValidIndex(EdgeIdx))
	{
		return false;
	}
	const FBNRoadEdge& Edge = GraphAsset->Edges[EdgeIdx];
	const TArray<FVector>& Pts = Edge.SampledPoints;
	const TArray<float>& Cum = EdgeCumulativeDist[EdgeIdx];
	if (Pts.Num() < 2 || Cum.Num() != Pts.Num())
	{
		return false;
	}
	const float Total = EdgeLengthsCm[EdgeIdx];
	const float Dist = bReverse ? FMath::Clamp(Total - DistanceCm, 0.f, Total) : FMath::Clamp(DistanceCm, 0.f, Total);

	int32 Seg = 0;
	for (int32 k = 0; k < Cum.Num() - 1; ++k)
	{
		if (Dist <= Cum[k + 1] || k == Cum.Num() - 2)
		{
			Seg = k;
			break;
		}
	}
	const float SegLen = FMath::Max(Cum[Seg + 1] - Cum[Seg], KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((Dist - Cum[Seg]) / SegLen, 0.f, 1.f);
	OutLocation = FMath::Lerp(Pts[Seg], Pts[Seg + 1], Alpha);
	FVector Dir = (Pts[Seg + 1] - Pts[Seg]).GetSafeNormal();
	if (bReverse)
	{
		Dir = -Dir;
	}
	OutTangent = Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
	return true;
}

void UBNRoadGraphSubsystem::GetOutgoingEdges(int32 NodeIdx, TArray<TPair<int32, bool>>& OutEdges) const
{
	OutEdges.Reset();
	if (!Adjacency.IsValidIndex(NodeIdx))
	{
		return;
	}
	for (const FAdjEntry& Entry : Adjacency[NodeIdx])
	{
		OutEdges.Add(TPair<int32, bool>(Entry.EdgeIdx, Entry.bReversed));
	}
}

bool UBNRoadGraphSubsystem::FindRoadPointAhead(const FVector& Pos, const FVector& ForwardDir, float DistanceCm,
	FTransform& OutTransform, float& OutWidthCm) const
{
	FVector ProjPos, ProjTan;
	int32 EdgeIdx = INDEX_NONE;
	float EdgeDist = 0.f;
	if (!FindNearestRoadPoint(Pos, ProjPos, ProjTan, OutWidthCm, EdgeIdx, EdgeDist))
	{
		return false;
	}

	const FVector Fwd2D = ForwardDir.GetSafeNormal2D();
	bool bReverse = FVector::DotProduct(ProjTan.GetSafeNormal2D(), Fwd2D) < 0.f;
	if (GraphAsset->Edges[EdgeIdx].bOneWay)
	{
		bReverse = false;
	}

	float Remaining = FMath::Max(DistanceCm, 0.f);
	float CurDistAlongTravel = bReverse ? (EdgeLengthsCm[EdgeIdx] - EdgeDist) : EdgeDist;
	FVector CurLoc = ProjPos;
	FVector CurTan = bReverse ? -ProjTan : ProjTan;

	for (int32 Hops = 0; Hops < 8; ++Hops)
	{
		const float EdgeLen = EdgeLengthsCm[EdgeIdx];
		const float LeftOnEdge = EdgeLen - CurDistAlongTravel;
		if (Remaining <= LeftOnEdge || LeftOnEdge <= 1.f)
		{
			SampleEdgeAtDistance(EdgeIdx, CurDistAlongTravel + Remaining, bReverse, CurLoc, CurTan);
			OutWidthCm = GraphAsset->Edges[EdgeIdx].WidthCm;
			OutTransform = FTransform(CurTan.Rotation(), CurLoc);
			return true;
		}
		Remaining -= LeftOnEdge;

		// Reached end node of current edge; choose continuation most aligned with current heading
		const FBNRoadEdge& CurEdge = GraphAsset->Edges[EdgeIdx];
		const int32 EndNodeId = bReverse ? CurEdge.NodeA : CurEdge.NodeB;
		const int32* EndNodeIdxPtr = NodeIdToIndex.Find(EndNodeId);
		if (!EndNodeIdxPtr || !Adjacency.IsValidIndex(*EndNodeIdxPtr))
		{
			break;
		}
		SampleEdgeAtDistance(EdgeIdx, EdgeLen, bReverse, CurLoc, CurTan);

		int32 BestNextEdge = INDEX_NONE;
		bool bBestNextRev = false;
		float BestDot = -2.f;
		for (const FAdjEntry& Adj : Adjacency[*EndNodeIdxPtr])
		{
			if (Adj.EdgeIdx == EdgeIdx)
			{
				continue;
			}
			FVector DummyLoc, NextTan;
			if (SampleEdgeAtDistance(Adj.EdgeIdx, 100.f, Adj.bReversed, DummyLoc, NextTan))
			{
				const float Dot = FVector::DotProduct(CurTan.GetSafeNormal2D(), NextTan.GetSafeNormal2D());
				if (Dot > BestDot)
				{
					BestDot = Dot;
					BestNextEdge = Adj.EdgeIdx;
					bBestNextRev = Adj.bReversed;
				}
			}
		}
		if (BestNextEdge == INDEX_NONE)
		{
			break;
		}
		EdgeIdx = BestNextEdge;
		bReverse = bBestNextRev;
		CurDistAlongTravel = 0.f;
	}

	OutWidthCm = GraphAsset->Edges[EdgeIdx].WidthCm;
	OutTransform = FTransform(CurTan.Rotation(), CurLoc);
	return true;
}

bool UBNRoadGraphSubsystem::FindPath(const FVector& StartPos, const FVector& GoalPos, TArray<FVector>& OutWaypoints) const
{
	OutWaypoints.Reset();
	if (!HasGraph())
	{
		return false;
	}

	const int32 StartIdx = FindNearestNode(StartPos);
	const int32 GoalIdx = FindNearestNode(GoalPos);
	if (StartIdx == INDEX_NONE || GoalIdx == INDEX_NONE)
	{
		return false;
	}
	if (StartIdx == GoalIdx)
	{
		OutWaypoints.Add(GraphAsset->Nodes[GoalIdx].Position);
		OutWaypoints.Add(GoalPos);
		return true;
	}

	const int32 NumNodes = GraphAsset->Nodes.Num();
	TArray<float> GScore;
	TArray<float> FScore;
	TArray<int32> CameFromNode;
	TArray<int32> CameFromEdge;
	TArray<bool> CameFromRev;
	TArray<bool> Closed;
	TArray<int32> OpenSet;

	GScore.Init(BIG_NUMBER, NumNodes);
	FScore.Init(BIG_NUMBER, NumNodes);
	CameFromNode.Init(INDEX_NONE, NumNodes);
	CameFromEdge.Init(INDEX_NONE, NumNodes);
	CameFromRev.Init(false, NumNodes);
	Closed.Init(false, NumNodes);

	const FVector GoalNodePos = GraphAsset->Nodes[GoalIdx].Position;
	GScore[StartIdx] = 0.f;
	FScore[StartIdx] = FVector::Dist(GraphAsset->Nodes[StartIdx].Position, GoalNodePos);
	OpenSet.Add(StartIdx);

	bool bFound = false;
	while (OpenSet.Num() > 0)
	{
		int32 BestOpenPos = 0;
		for (int32 i = 1; i < OpenSet.Num(); ++i)
		{
			if (FScore[OpenSet[i]] < FScore[OpenSet[BestOpenPos]])
			{
				BestOpenPos = i;
			}
		}
		const int32 Current = OpenSet[BestOpenPos];
		OpenSet.RemoveAtSwap(BestOpenPos);
		if (Current == GoalIdx)
		{
			bFound = true;
			break;
		}
		Closed[Current] = true;

		for (const FAdjEntry& Adj : Adjacency[Current])
		{
			if (Closed[Adj.ToNodeIdx])
			{
				continue;
			}
			const float TentativeG = GScore[Current] + Adj.Cost;
			if (TentativeG < GScore[Adj.ToNodeIdx])
			{
				CameFromNode[Adj.ToNodeIdx] = Current;
				CameFromEdge[Adj.ToNodeIdx] = Adj.EdgeIdx;
				CameFromRev[Adj.ToNodeIdx] = Adj.bReversed;
				GScore[Adj.ToNodeIdx] = TentativeG;
				FScore[Adj.ToNodeIdx] = TentativeG + FVector::Dist(GraphAsset->Nodes[Adj.ToNodeIdx].Position, GoalNodePos) * 0.85f;
				if (!OpenSet.Contains(Adj.ToNodeIdx))
				{
					OpenSet.Add(Adj.ToNodeIdx);
				}
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	// Reconstruct sequence of edges from StartIdx -> GoalIdx
	TArray<TPair<int32, bool>> EdgeSeq;
	int32 Cursor = GoalIdx;
	while (Cursor != StartIdx && CameFromNode[Cursor] != INDEX_NONE)
	{
		EdgeSeq.Add(TPair<int32, bool>(CameFromEdge[Cursor], CameFromRev[Cursor]));
		Cursor = CameFromNode[Cursor];
	}
	Algo::Reverse(EdgeSeq);

	for (const TPair<int32, bool>& Step : EdgeSeq)
	{
		const FBNRoadEdge& Edge = GraphAsset->Edges[Step.Key];
		const TArray<FVector>& Pts = Edge.SampledPoints;
		if (Pts.Num() == 0)
		{
			continue;
		}
		// Subsample waypoints every ~25 m so AI steers smoothly along curves without jitter
		const int32 Stride = FMath::Max(1, Pts.Num() / 5);
		if (!Step.Value)
		{
			for (int32 i = 0; i < Pts.Num(); i += Stride)
			{
				if (OutWaypoints.Num() == 0 || FVector::DistSquared(OutWaypoints.Last(), Pts[i]) > 250000.f)
				{
					OutWaypoints.Add(Pts[i]);
				}
			}
			if (OutWaypoints.Num() == 0 || FVector::DistSquared(OutWaypoints.Last(), Pts.Last()) > 40000.f)
			{
				OutWaypoints.Add(Pts.Last());
			}
		}
		else
		{
			for (int32 i = Pts.Num() - 1; i >= 0; i -= Stride)
			{
				if (OutWaypoints.Num() == 0 || FVector::DistSquared(OutWaypoints.Last(), Pts[i]) > 250000.f)
				{
					OutWaypoints.Add(Pts[i]);
				}
			}
			if (OutWaypoints.Num() == 0 || FVector::DistSquared(OutWaypoints.Last(), Pts[0]) > 40000.f)
			{
				OutWaypoints.Add(Pts[0]);
			}
		}
	}
	OutWaypoints.Add(GoalPos);
	return OutWaypoints.Num() > 0;
}

EBNDistrict UBNRoadGraphSubsystem::GetDistrictAt(const FVector& Pos, FString* OutName, FString* OutSubtitle) const
{
	EBNDistrict Result = EBNDistrict::MerkezBulvar;
	const FBNDistrictBounds* Matched = nullptr;

	if (GraphAsset && GraphAsset->Districts.Num() > 0)
	{
		// Prefer exact bounding box containment; if overlapping or outside all, choose nearest center
		float BestDistSq = BIG_NUMBER;
		for (const FBNDistrictBounds& B : GraphAsset->Districts)
		{
			const float Dx = FMath::Abs(Pos.X - B.CenterCm.X);
			const float Dy = FMath::Abs(Pos.Y - B.CenterCm.Y);
			if (Dx <= B.HalfExtentCm.X && Dy <= B.HalfExtentCm.Y)
			{
				const float CenterDistSq = Dx * Dx + Dy * Dy;
				if (!Matched || CenterDistSq < BestDistSq)
				{
					Matched = &B;
					BestDistSq = CenterDistSq;
					Result = B.District;
				}
			}
		}
		if (!Matched)
		{
			for (const FBNDistrictBounds& B : GraphAsset->Districts)
			{
				const float D2 = FVector2D::DistSquared(FVector2D(Pos.X, Pos.Y), B.CenterCm);
				if (D2 < BestDistSq)
				{
					BestDistSq = D2;
					Matched = &B;
					Result = B.District;
				}
			}
		}
	}
	else if (HasGraph())
	{
		const int32 NearNode = FindNearestNode(Pos);
		if (NearNode != INDEX_NONE)
		{
			Result = GraphAsset->Nodes[NearNode].District;
		}
	}

	if (OutName)
	{
		*OutName = (Matched && !Matched->DisplayName.IsEmpty()) ? Matched->DisplayName : DefaultDistrictName(Result);
	}
	if (OutSubtitle)
	{
		*OutSubtitle = (Matched && !Matched->Subtitle.IsEmpty()) ? Matched->Subtitle : DefaultDistrictSubtitle(Result);
	}
	return Result;
}

bool UBNRoadGraphSubsystem::IsInsideCooldownSpot(const FVector& Pos, FBNCooldownSpot* OutSpot) const
{
	if (!GraphAsset)
	{
		return false;
	}
	for (const FBNCooldownSpot& Spot : GraphAsset->CooldownSpots)
	{
		if (FVector::Dist2D(Pos, Spot.Location) <= Spot.RadiusCm && FMath::Abs(Pos.Z - Spot.Location.Z) <= 900.f)
		{
			if (OutSpot)
			{
				*OutSpot = Spot;
			}
			return true;
		}
	}
	return false;
}

FVector UBNRoadGraphSubsystem::GetEscapeRouteLocation(int32 RouteIndex) const
{
	if (GraphAsset)
	{
		return RouteIndex == 1 ? GraphAsset->EscapeRouteBLocation : GraphAsset->EscapeRouteALocation;
	}
	return RouteIndex == 1 ? FVector(-76000.f, -80000.f, 4800.f) : FVector(86000.f, -12000.f, 0.f);
}

const TCHAR* UBNRoadGraphSubsystem::DefaultDistrictName(EBNDistrict District)
{
	switch (District)
	{
	case EBNDistrict::DoguSanayi:     return TEXT("DOĞU SANAYİ");
	case EBNDistrict::EskiLiman:      return TEXT("ESKİ LİMAN");
	case EBNDistrict::KuzeyViyadugu:  return TEXT("KUZEY VİYADÜĞÜ & ÇEVRE YOLU");
	case EBNDistrict::YamacMahallesi: return TEXT("YAMAÇ MAHALLESİ");
	case EBNDistrict::KuzeyGecidi:    return TEXT("KUZEY GEÇİDİ & SEYİR TEPESİ");
	default:                          return TEXT("MERKEZ BULVAR & ÇARŞI");
	}
}

const TCHAR* UBNRoadGraphSubsystem::DefaultDistrictSubtitle(EBNDistrict District)
{
	switch (District)
	{
	case EBNDistrict::DoguSanayi:     return TEXT("TARIK OTO TAMİR • KONTEYNER AVLUSU");
	case EBNDistrict::EskiLiman:      return TEXT("MARA RIHTIM • ESKİ LİMAN KÖPRÜSÜ");
	case EBNDistrict::KuzeyViyadugu:  return TEXT("O-5 ÇEVRE YOLU • YÜKSEK HIZ HATTI");
	case EBNDistrict::YamacMahallesi: return TEXT("SELİN'İN GARAJI • YOKUŞ SOKAKLAR");
	case EBNDistrict::KuzeyGecidi:    return TEXT("DAĞ SAÇ TOKALARI • EMRE ANITI");
	default:                          return TEXT("KARAMARA ŞEHİR MERKEZİ");
	}
}

void UBNRoadGraphSubsystem::SetGpsDestination(const FVector& WorldPos, const FString& Label, const FVector& PlayerPos)
{
	bHasGpsDestination = true;
	GpsTargetLocation = WorldPos;
	GpsTargetLabel = Label.IsEmpty() ? TEXT("HARİTA HEDEFİ") : Label;
	LastGpsStartPos = PlayerPos;
	GpsRoutePoints.Reset();

	if (!FindPath(PlayerPos, WorldPos, GpsRoutePoints) || GpsRoutePoints.Num() < 2)
	{
		GpsRoutePoints.Reset();
		GpsRoutePoints.Add(PlayerPos);
		GpsRoutePoints.Add(WorldPos);
	}
	else if (FVector::Dist2D(PlayerPos, GpsRoutePoints[0]) > 400.f)
	{
		GpsRoutePoints.Insert(PlayerPos, 0);
	}
}

void UBNRoadGraphSubsystem::ClearGpsDestination()
{
	bHasGpsDestination = false;
	GpsTargetLocation = FVector::ZeroVector;
	GpsTargetLabel.Reset();
	GpsRoutePoints.Reset();
}

float UBNRoadGraphSubsystem::GetGpsRemainingDistanceCm(const FVector& PlayerPos) const
{
	if (!bHasGpsDestination)
	{
		return 0.f;
	}
	if (GpsRoutePoints.Num() < 2)
	{
		return FVector::Dist2D(PlayerPos, GpsTargetLocation);
	}
	float Total = FVector::Dist2D(PlayerPos, GpsRoutePoints[0]);
	for (int32 i = 0; i + 1 < GpsRoutePoints.Num(); ++i)
	{
		Total += FVector::Dist2D(GpsRoutePoints[i], GpsRoutePoints[i + 1]);
	}
	return Total;
}

bool UBNRoadGraphSubsystem::UpdateGpsForPlayer(const FVector& PlayerPos)
{
	if (!bHasGpsDestination)
	{
		return false;
	}
	if (FVector::Dist2D(PlayerPos, GpsTargetLocation) <= 3500.f)
	{
		ClearGpsDestination();
		return true;
	}
	// Recompute shortest A* path cleanly whenever the player moves >20 m from the last origin sample.
	if (FVector::DistSquared2D(PlayerPos, LastGpsStartPos) >= FMath::Square(2000.f))
	{
		SetGpsDestination(GpsTargetLocation, GpsTargetLabel, PlayerPos);
	}
	return false;
}
