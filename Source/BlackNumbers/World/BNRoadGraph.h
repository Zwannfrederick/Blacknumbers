#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "BNRoadGraph.generated.h"

UENUM(BlueprintType)
enum class EBNDistrict : uint8
{
	MerkezBulvar = 0,
	DoguSanayi = 1,
	EskiLiman = 2,
	KuzeyViyadugu = 3,
	YamacMahallesi = 4,
	KuzeyGecidi = 5,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNRoadNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	int32 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	EBNDistrict District = EBNDistrict::MerkezBulvar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	bool bSignalized = false;
};

USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNRoadEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	int32 NodeA = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	int32 NodeB = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	TArray<FVector> SampledPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	float WidthCm = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	float SpeedLimitKmh = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	EBNDistrict District = EBNDistrict::MerkezBulvar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	bool bHighway = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	bool bOneWay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	bool bDirt = false;
};

USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNCooldownSpot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	float RadiusCm = 1300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	EBNDistrict District = EBNDistrict::MerkezBulvar;
};

USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNDistrictBounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	EBNDistrict District = EBNDistrict::MerkezBulvar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector2D CenterCm = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector2D HalfExtentCm = FVector2D(30000.f, 30000.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FString Subtitle;
};

/**
 * Single source of truth for Karamara's 2 km x 2 km open-world road network,
 * districts, cooldown hiding spots, and Act 5 escape portals.
 * Authored by Tools/build_karamara_map.py as /Game/World/DA_KaramaraRoadGraph.
 */
UCLASS(BlueprintType)
class BLACKNUMBERS_API UBNRoadGraphAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	TArray<FBNRoadNode> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	TArray<FBNRoadEdge> Edges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	TArray<FBNCooldownSpot> CooldownSpots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	TArray<FBNDistrictBounds> Districts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector EscapeRouteALocation = FVector(86000.f, -12000.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoadGraph")
	FVector EscapeRouteBLocation = FVector(-76000.f, -80000.f, 4800.f);
};

/**
 * Runtime query & A* navigation subsystem shared by Police AI, Roadblocks,
 * Civilian Traffic, Radar HUD, and Story Escape checks.
 */
UCLASS()
class BLACKNUMBERS_API UBNRoadGraphSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Injects a graph directly (used by unit tests or dynamic levels). */
	void SetRoadGraphAsset(UBNRoadGraphAsset* InAsset);

	UFUNCTION(BlueprintPure, Category = "RoadGraph")
	bool HasGraph() const { return GraphAsset != nullptr && GraphAsset->Nodes.Num() > 0 && GraphAsset->Edges.Num() > 0; }

	const TArray<FBNRoadNode>& GetNodes() const;
	const TArray<FBNRoadEdge>& GetEdges() const;
	const TArray<FBNCooldownSpot>& GetCooldownSpots() const;
	const TArray<FBNDistrictBounds>& GetDistricts() const;

	int32 FindNearestNode(const FVector& Pos) const;

	/** Projects Pos onto the closest sampled segment of the road network. */
	bool FindNearestRoadPoint(const FVector& Pos, FVector& OutPoint, FVector& OutTangent,
		float& OutWidthCm, int32& OutEdgeIdx, float& OutDistanceAlongEdge) const;

	/** Walks DistanceCm forward along connected road edges from Pos along ForwardDir. */
	bool FindRoadPointAhead(const FVector& Pos, const FVector& ForwardDir, float DistanceCm,
		FTransform& OutTransform, float& OutWidthCm) const;

	/** A* shortest-path query returning ordered 3D world waypoints along road curves. */
	bool FindPath(const FVector& StartPos, const FVector& GoalPos, TArray<FVector>& OutWaypoints) const;

	/** Returns the canon Karamara district covering Pos (and optional localized banner text). */
	EBNDistrict GetDistrictAt(const FVector& Pos, FString* OutName = nullptr, FString* OutSubtitle = nullptr) const;

	/** True when Pos lies inside one of Karamara's cooldown hideouts. */
	bool IsInsideCooldownSpot(const FVector& Pos, FBNCooldownSpot* OutSpot = nullptr) const;

	/** Returns Perde 5 Escape Route A (0 = Eski Liman Tuneli) or B (1 = Kuzey Gecidi Toprak Yol). */
	FVector GetEscapeRouteLocation(int32 RouteIndex) const;

	/** Evaluates world position & unit tangent at DistanceCm along EdgeIdx (0..EdgeLength). */
	bool SampleEdgeAtDistance(int32 EdgeIdx, float DistanceCm, bool bReverse,
		FVector& OutLocation, FVector& OutTangent) const;

	float GetEdgeLengthCm(int32 EdgeIdx) const;

	/** Returns outgoing (EdgeIdx, bTraverseReversed) pairs connected to NodeIdx. */
	void GetOutgoingEdges(int32 NodeIdx, TArray<TPair<int32, bool>>& OutEdges) const;

	// --- Interactive City Map A* GPS Navigation ---
	void SetGpsDestination(const FVector& WorldPos, const FString& Label, const FVector& PlayerPos);
	void ClearGpsDestination();
	bool HasGpsDestination() const { return bHasGpsDestination; }
	FVector GetGpsTargetLocation() const { return GpsTargetLocation; }
	const FString& GetGpsTargetLabel() const { return GpsTargetLabel; }
	const TArray<FVector>& GetGpsRoutePoints() const { return GpsRoutePoints; }
	float GetGpsRemainingDistanceCm(const FVector& PlayerPos) const;
	/** Refreshes the A* path when the player moves; returns true once upon reaching the destination. */
	bool UpdateGpsForPlayer(const FVector& PlayerPos);

	static const TCHAR* DefaultDistrictName(EBNDistrict District);
	static const TCHAR* DefaultDistrictSubtitle(EBNDistrict District);

private:
	struct FAdjEntry
	{
		int32 ToNodeIdx = INDEX_NONE;
		int32 EdgeIdx = INDEX_NONE;
		float Cost = 0.f;
		bool bReversed = false;
	};

	void TryLoadDefaultGraph();
	void RebuildIndex();

	UPROPERTY(Transient)
	TObjectPtr<UBNRoadGraphAsset> GraphAsset;

	TMap<int32, int32> NodeIdToIndex;
	TArray<TArray<FAdjEntry>> Adjacency;
	TArray<float> EdgeLengthsCm;
	TArray<TArray<float>> EdgeCumulativeDist;

	bool bHasGpsDestination = false;
	FVector GpsTargetLocation = FVector::ZeroVector;
	FString GpsTargetLabel;
	FVector LastGpsStartPos = FVector::ZeroVector;
	TArray<FVector> GpsRoutePoints;
};
