#include "World/BNTrafficSubsystem.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitSubsystem.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNDayNightCycle.h"
#include "World/BNRoadGraph.h"

namespace
{
	constexpr int32 TargetTrafficCount = 20;
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float KmhToCmPerSec = 100000.f / 3600.f;

	constexpr float YieldGapMaxCm = 2000.f;
	constexpr float YieldGapStopCm = 850.f;
	constexpr float SirenYieldRadiusCm = 3400.f;
	constexpr float CollisionRadiusCm = 350.f;
	constexpr float NearMissRadiusCm = 480.f;
	constexpr float CollisionMinRelSpeedKmh = 30.f;
	constexpr float NearMissMinPlayerSpeedKmh = 80.f;
	constexpr float VerticalInteractionTolCm = 220.f;
	constexpr int32 TrafficCollateralDollars = 3500;
	constexpr float KnockDurationSeconds = 1.35f;
	constexpr float PauseDurationSeconds = 2.0f;

	constexpr float RecycleMaxDistCm = 58000.f;
	constexpr float RecycleMinDistCm = 14000.f;
	constexpr float RecycleSpawnMaxCm = 46000.f;
	constexpr float HeadlightBeamCandelas = 3000.f;
	constexpr float HeadlightBeamRadius = 2200.f;
	const FLinearColor HeadlightBeamColor(1.f, 0.94f, 0.82f);

	// Realistic Turkish urban street palette (includes classic yellow taxis, white commercial vans, silver/dark sedans)
	const FLinearColor CivilianPalette[] = {
		FLinearColor(0.92f, 0.72f, 0.04f, 1.f), // Karamara Taksi Sarısı
		FLinearColor(0.84f, 0.85f, 0.86f, 1.f), // Ticari Beyaz
		FLinearColor(0.46f, 0.49f, 0.53f, 1.f), // Metalik Gri
		FLinearColor(0.14f, 0.15f, 0.17f, 1.f), // Antrasit
		FLinearColor(0.08f, 0.16f, 0.34f, 1.f), // Gece Lacivert
		FLinearColor(0.52f, 0.07f, 0.08f, 1.f), // Bordo
		FLinearColor(0.10f, 0.25f, 0.16f, 1.f), // Koyu Yeşil
		FLinearColor(0.64f, 0.58f, 0.48f, 1.f), // Şampanya Bej
		FLinearColor(0.20f, 0.36f, 0.48f, 1.f), // Buz Mavi
		FLinearColor(0.92f, 0.72f, 0.04f, 1.f), // Karamara Taksi Sarısı
	};

	const TCHAR* const TrafficMeshPackages[] = {
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Sedan"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Hatchback"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Van"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_BoxTruck"),
		TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body"),
	};
	const TCHAR* const TrafficMeshAssets[] = {
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Sedan.SM_BN_Traffic_Sedan"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Hatchback.SM_BN_Traffic_Hatchback"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_Van.SM_BN_Traffic_Van"),
		TEXT("/Game/World/Meshes/SM_BN_Traffic_BoxTruck.SM_BN_Traffic_BoxTruck"),
		TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body"),
	};
}

float UBNTrafficSubsystem::SampleViaductElevationZ(float X)
{
	if (X <= -22000.f || X >= 22000.f) { return 2.f; }
	if (X < -12000.f) { return FMath::Lerp(2.f, 700.f, (X + 22000.f) / 10000.f); }
	if (X <= 12000.f) { return 700.f; }
	return FMath::Lerp(700.f, 2.f, (X - 12000.f) / 10000.f);
}

bool UBNTrafficSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UBNTrafficSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBNTrafficSubsystem, STATGROUP_Tickables);
}

void UBNTrafficSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWorldBegunPlay = true;
	bIsKaramaraMap = InWorld.GetMapName().Contains(TEXT("Karamara"));
	if (!bIsKaramaraMap) { return; }

	LoadTrafficAssets();

	if (TActorIterator<ABNDayNightCycle> Cycle(&InWorld); Cycle)
	{
		Cycle->OnNightChanged.AddUniqueDynamic(this, &UBNTrafficSubsystem::HandleNightChanged);
		bNightLightsOn = Cycle->IsNight();
	}
	if (bTrafficEnabled)
	{
		SpawnAllTraffic();
	}
}

void UBNTrafficSubsystem::Deinitialize()
{
	ClearTraffic();
	VehicleMeshes.Reset();
	bIsKaramaraMap = false;
	bWorldBegunPlay = false;
	Super::Deinitialize();
}

void UBNTrafficSubsystem::HandleNightChanged(bool bInIsNight)
{
	bNightLightsOn = bInIsNight;
	for (const FTrafficCar& Car : Cars)
	{
		ApplyNightLightsToCar(Car, bNightLightsOn);
	}
}

void UBNTrafficSubsystem::SetTrafficEnabled(bool bEnabled)
{
	if (bTrafficEnabled == bEnabled) { return; }
	bTrafficEnabled = bEnabled;
	if (!bIsKaramaraMap || !bWorldBegunPlay) { return; }
	bTrafficEnabled ? SpawnAllTraffic() : ClearTraffic();
}

int32 UBNTrafficSubsystem::GetActiveTrafficCount() const
{
	int32 Count = 0;
	for (const FTrafficCar& Car : Cars)
	{
		Count += Car.Actor.IsValid() ? 1 : 0;
	}
	return Count;
}

void UBNTrafficSubsystem::GetTrafficLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset(Cars.Num());
	for (const FTrafficCar& Car : Cars)
	{
		if (const AActor* Actor = Car.Actor.Get())
		{
			OutLocations.Add(Actor->GetActorLocation());
		}
	}
}

void UBNTrafficSubsystem::LoadTrafficAssets()
{
	VehicleMeshes.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(TrafficMeshPackages); ++i)
	{
		if (FPackageName::DoesPackageExist(TrafficMeshPackages[i]))
		{
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TrafficMeshAssets[i]))
			{
				VehicleMeshes.Add(Mesh);
			}
		}
	}
	if (VehicleMeshes.IsEmpty())
	{
		if (UStaticMesh* Fallback = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar")))
		{
			VehicleMeshes.Add(Fallback);
		}
	}
}

bool UBNTrafficSubsystem::PickEdgeNearPlayer(const FVector& PlayerLoc, float MinDistCm, float MaxDistCm,
	int32 IgnoreCarIdx, int32& OutEdgeIdx, bool& bOutRev, float& OutDistAlong) const
{
	const UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!RoadGraph || !RoadGraph->HasGraph())
	{
		return false;
	}
	const TArray<FBNRoadEdge>& Edges = RoadGraph->GetEdges();
	if (Edges.IsEmpty())
	{
		return false;
	}

	int32 BestEdge = INDEX_NONE;
	bool bBestRev = false;
	float BestDistAlong = 0.f;
	float BestScore = -BIG_NUMBER;

	for (int32 Attempt = 0; Attempt < 32; ++Attempt)
	{
		const int32 EIdx = FMath::RandRange(0, Edges.Num() - 1);
		if (Edges[EIdx].bDirt)
		{
			continue;
		}
		const float EdgeLen = RoadGraph->GetEdgeLengthCm(EIdx);
		if (EdgeLen < 1200.f)
		{
			continue;
		}
		const bool bRev = FMath::RandBool();
		const float DistAlong = FMath::FRandRange(200.f, EdgeLen - 200.f);
		FVector Loc, Tan;
		if (!RoadGraph->SampleEdgeAtDistance(EIdx, DistAlong, bRev, Loc, Tan))
		{
			continue;
		}
		const float DistToPlayer = FVector::Dist2D(Loc, PlayerLoc);
		if (DistToPlayer < MinDistCm || DistToPlayer > MaxDistCm)
		{
			continue;
		}

		// Ensure no immediate overlap with another traffic car on same edge & direction
		float MinCarGap = 99999.f;
		for (int32 j = 0; j < Cars.Num(); ++j)
		{
			if (j == IgnoreCarIdx || Cars[j].EdgeIdx != EIdx || Cars[j].bReversed != bRev)
			{
				continue;
			}
			MinCarGap = FMath::Min(MinCarGap, FMath::Abs(Cars[j].DistanceAlong - DistAlong));
		}
		if (MinCarGap < 2200.f)
		{
			continue;
		}
		const float Score = FMath::Min(MinCarGap, 8000.f) - FMath::Abs(DistToPlayer - 0.5f * (MinDistCm + MaxDistCm)) * 0.2f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestEdge = EIdx;
			bBestRev = bRev;
			BestDistAlong = DistAlong;
		}
	}

	if (BestEdge == INDEX_NONE)
	{
		// Fallback to any paved edge on graph
		for (int32 e = 0; e < Edges.Num(); ++e)
		{
			if (!Edges[e].bDirt && RoadGraph->GetEdgeLengthCm(e) >= 1200.f)
			{
				BestEdge = e;
				bBestRev = (IgnoreCarIdx % 2 == 1);
				BestDistAlong = RoadGraph->GetEdgeLengthCm(e) * 0.5f;
				break;
			}
		}
	}
	if (BestEdge == INDEX_NONE)
	{
		return false;
	}
	OutEdgeIdx = BestEdge;
	bOutRev = bBestRev;
	OutDistAlong = BestDistAlong;
	return true;
}

bool UBNTrafficSubsystem::ComputeCarPose(const FTrafficCar& Car, FVector& OutLocation, FRotator& OutRotation) const
{
	const UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!RoadGraph || !RoadGraph->HasGraph())
	{
		return false;
	}
	const TArray<FBNRoadEdge>& Edges = RoadGraph->GetEdges();
	if (!Edges.IsValidIndex(Car.EdgeIdx))
	{
		return false;
	}
	FVector CenterLoc, Tan;
	if (!RoadGraph->SampleEdgeAtDistance(Car.EdgeIdx, Car.DistanceAlong, Car.bReversed, CenterLoc, Tan))
	{
		return false;
	}
	const FVector Right2D(-Tan.Y, Tan.X, 0.f);
	const float LaneOffset = FMath::Clamp(Edges[Car.EdgeIdx].WidthCm * 0.25f, 320.f, 520.f) + Car.LateralPullOffset;
	OutLocation = CenterLoc + Right2D.GetSafeNormal() * LaneOffset + FVector(0.f, 0.f, 2.f);
	OutRotation = Tan.Rotation();
	return true;
}

void UBNTrafficSubsystem::AdvanceToNextEdge(FTrafficCar& Car) const
{
	const UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!RoadGraph || !RoadGraph->HasGraph())
	{
		return;
	}
	const TArray<FBNRoadEdge>& Edges = RoadGraph->GetEdges();
	if (!Edges.IsValidIndex(Car.EdgeIdx))
	{
		return;
	}

	FVector EndLoc, CurTan;
	RoadGraph->SampleEdgeAtDistance(Car.EdgeIdx, RoadGraph->GetEdgeLengthCm(Car.EdgeIdx), Car.bReversed, EndLoc, CurTan);
	const int32 EndNodeIdx = RoadGraph->FindNearestNode(EndLoc);
	TArray<TPair<int32, bool>> OutEdges;
	RoadGraph->GetOutgoingEdges(EndNodeIdx, OutEdges);

	int32 BestEdge = Car.EdgeIdx;
	bool bBestRev = !Car.bReversed;
	float BestScore = -10.f;

	for (const TPair<int32, bool>& Cand : OutEdges)
	{
		if (Cand.Key == Car.EdgeIdx || Edges[Cand.Key].bDirt)
		{
			continue;
		}
		FVector DummyL, CandTan;
		if (RoadGraph->SampleEdgeAtDistance(Cand.Key, 200.f, Cand.Value, DummyL, CandTan))
		{
			// Favor straight/gentle continuations over sharp turns, with mild randomness for natural variety
			const float Score = FVector::DotProduct(CurTan.GetSafeNormal2D(), CandTan.GetSafeNormal2D())
				+ FMath::FRandRange(-0.35f, 0.35f);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestEdge = Cand.Key;
				bBestRev = Cand.Value;
			}
		}
	}

	Car.EdgeIdx = BestEdge;
	Car.bReversed = bBestRev;
	Car.DistanceAlong = 60.f;
	const float RoadLimit = Edges[BestEdge].SpeedLimitKmh;
	Car.CruiseSpeedCmPerSec = FMath::Clamp(RoadLimit * FMath::FRandRange(0.72f, 0.92f), 46.f, 105.f) * KmhToCmPerSec;
}

void UBNTrafficSubsystem::SpawnAllTraffic()
{
	ClearTraffic();
	UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!World || !RoadGraph || !RoadGraph->HasGraph() || VehicleMeshes.IsEmpty())
	{
		return;
	}

	const ABNVehiclePawn* Player = FindPlayerCar();
	const FVector PlayerLoc = Player ? Player->GetActorLocation() : FVector(-18000.f, -4000.f, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	for (int32 i = 0; i < TargetTrafficCount; ++i)
	{
		int32 EdgeIdx = 0;
		bool bRev = false;
		float DistAlong = 0.f;
		// Spread first 12 cars closer (45..320 m) so Merkez feels immediately alive, rest out to 520 m!
		const float MinD = (i < 12) ? 4500.f : 18000.f;
		const float MaxD = (i < 12) ? 34000.f : 54000.f;
		if (!PickEdgeNearPlayer(PlayerLoc, MinD, MaxD, i, EdgeIdx, bRev, DistAlong))
		{
			continue;
		}

		FTrafficCar Car;
		Car.EdgeIdx = EdgeIdx;
		Car.bReversed = bRev;
		Car.DistanceAlong = DistAlong;
		const float BaseLimit = RoadGraph->GetEdges()[EdgeIdx].SpeedLimitKmh;
		Car.CruiseSpeedCmPerSec = FMath::Clamp(BaseLimit * FMath::FRandRange(0.70f, 0.92f), 46.f, 102.f) * KmhToCmPerSec;
		Car.CurrentSpeedCmPerSec = Car.CruiseSpeedCmPerSec;

		FVector SpawnLoc;
		FRotator SpawnRot;
		if (!ComputeCarPose(Car, SpawnLoc, SpawnRot))
		{
			continue;
		}

		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(SpawnRot, SpawnLoc), Params);
		if (!Actor) { continue; }

		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("TrafficRoot"));
		Root->SetMobility(EComponentMobility::Movable);
		Actor->SetRootComponent(Root);
		Root->SetRelativeLocationAndRotation(SpawnLoc, SpawnRot);
		Root->RegisterComponent();
		Actor->SetActorLocationAndRotation(SpawnLoc, SpawnRot);

		UStaticMesh* ChosenMesh = VehicleMeshes[i % VehicleMeshes.Num()];
		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Actor, TEXT("TrafficBody"));
		Body->SetStaticMesh(ChosenMesh);
		Body->SetMobility(EComponentMobility::Movable);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetGenerateOverlapEvents(false);
		Body->SetCastShadow(true);
		Body->SetCanEverAffectNavigation(false);
		Body->SetupAttachment(Root);

		// Align imported UE Offroad SUV mesh if its length runs along Y instead of X
		const FBox Bounds = ChosenMesh->GetBoundingBox();
		if (Bounds.IsValid && Bounds.GetSize().Y > Bounds.GetSize().X * 1.15f)
		{
			Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
			Body->SetRelativeLocation(FVector(0.f, 0.f, -Bounds.Min.Z));
		}
		Body->RegisterComponent();

		// Apply civilian paint tint (works for both MI_Traffic_Paint "BaseColor" and Offroad "Paint Tint")
		const FLinearColor Paint = CivilianPalette[i % UE_ARRAY_COUNT(CivilianPalette)];
		for (int32 m = 0; m < Body->GetNumMaterials(); ++m)
		{
			UMaterialInterface* Mat = Body->GetMaterial(m);
			if (!Mat) { continue; }
			const FString MName = Mat->GetName();
			if (MName.Contains(TEXT("Traffic_Paint")) || MName.Contains(TEXT("OffroadCar_Main")) || MName.Contains(TEXT("SportsCarBody")))
			{
				if (UMaterialInstanceDynamic* Mid = Body->CreateDynamicMaterialInstance(m, Mat))
				{
					Mid->SetVectorParameterValue(TEXT("BaseColor"), Paint);
					Mid->SetVectorParameterValue(TEXT("Paint Tint"), Paint);
				}
			}
			else if (MName.Contains(TEXT("Traffic_Taillight")))
			{
				Car.TaillightMid = Body->CreateDynamicMaterialInstance(m, Mat);
			}
		}

		// Night headlight beam (low draw cost, fades out beyond 110 m)
		USpotLightComponent* Beam = NewObject<USpotLightComponent>(Actor, TEXT("TrafficHeadlight"));
		Beam->SetMobility(EComponentMobility::Movable);
		Beam->SetupAttachment(Root);
		Beam->SetRelativeLocationAndRotation(FVector(240.f, 0.f, 70.f), FRotator(-6.f, 0.f, 0.f));
		Beam->SetIntensityUnits(ELightUnits::Candelas);
		Beam->SetIntensity(HeadlightBeamCandelas);
		Beam->SetLightColor(HeadlightBeamColor);
		Beam->SetAttenuationRadius(HeadlightBeamRadius);
		Beam->SetInnerConeAngle(16.f);
		Beam->SetOuterConeAngle(36.f);
		Beam->SetCastShadows(false);
		Beam->SetMaxDrawDistance(11000.f);
		Beam->SetMaxDistanceFadeRange(2500.f);
		Beam->RegisterComponent();

		Car.Actor = Actor;
		Car.BodyComp = Body;
		Car.HeadlightBeam = Beam;
		ApplyNightLightsToCar(Car, bNightLightsOn);
		Cars.Add(MoveTemp(Car));
	}
}

void UBNTrafficSubsystem::ApplyNightLightsToCar(const FTrafficCar& Car, bool bOn) const
{
	if (USpotLightComponent* Beam = Car.HeadlightBeam.Get())
	{
		Beam->SetVisibility(bOn);
	}
}

void UBNTrafficSubsystem::ClearTraffic()
{
	for (FTrafficCar& Car : Cars)
	{
		if (AActor* Actor = Car.Actor.Get()) { Actor->Destroy(); }
	}
	Cars.Reset();
}

ABNVehiclePawn* UBNTrafficSubsystem::FindPlayerCar() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
}

void UBNTrafficSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.1f);
	UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!bIsKaramaraMap || !bTrafficEnabled || !World || !RoadGraph || !RoadGraph->HasGraph() || Dt <= 0.f)
	{
		return;
	}
	if (Cars.IsEmpty())
	{
		SpawnAllTraffic();
		return;
	}

	ABNVehiclePawn* Player = FindPlayerCar();
	const FVector PlayerLoc = Player ? Player->GetActorLocation() : FVector(-18000.f, -4000.f, 0.f);
	const FVector PlayerVel = Player ? Player->GetVelocity() : FVector::ZeroVector;
	const float PlayerSpeedKmh = Player ? Player->GetSpeedKmh() : 0.f;

	UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>();
	const bool bPursuitActive = Pursuit && Pursuit->GetState().IsPursuitRunning();
	TArray<FVector, TInlineAllocator<8>> SirenPositions;
	if (bPursuitActive)
	{
		if (Player) { SirenPositions.Add(PlayerLoc); }
		for (TActorIterator<ABNPoliceController> It(World); It; ++It)
		{
			if (const APawn* Cop = It->GetPawn(); Cop && !It->IsUnitDisabled())
			{
				SirenPositions.Add(Cop->GetActorLocation());
			}
		}
	}

	for (int32 i = 0; i < Cars.Num(); ++i)
	{
		FTrafficCar& Car = Cars[i];
		AActor* Actor = Car.Actor.Get();
		if (!Actor) { continue; }
		Car.NearMissCooldown = FMath::Max(0.f, Car.NearMissCooldown - Dt);

		if (Car.State == ETrafficCarState::Knocked)
		{
			Car.StateTimer -= Dt;
			const float Drag = FMath::Exp(-3.0f * Dt);
			Car.KnockVelocity *= Drag;
			Car.KnockYawRateDeg *= Drag;
			FRotator NewRot = Actor->GetActorRotation();
			NewRot.Yaw += Car.KnockYawRateDeg * Dt;
			Actor->SetActorLocationAndRotation(Actor->GetActorLocation() + Car.KnockVelocity * Dt, NewRot);
			if (Car.StateTimer <= 0.f)
			{
				Car.State = ETrafficCarState::Paused;
				Car.StateTimer = PauseDurationSeconds;
				Car.CurrentSpeedCmPerSec = 0.f;
			}
			continue;
		}
		if (Car.State == ETrafficCarState::Paused)
		{
			Car.StateTimer -= Dt;
			if (Car.StateTimer <= 0.f)
			{
				int32 NewEdge = Car.EdgeIdx;
				bool bNewRev = Car.bReversed;
				float NewDist = 0.f;
				if (PickEdgeNearPlayer(PlayerLoc, RecycleMinDistCm, RecycleSpawnMaxCm, i, NewEdge, bNewRev, NewDist))
				{
					Car.EdgeIdx = NewEdge;
					Car.bReversed = bNewRev;
					Car.DistanceAlong = NewDist;
				}
				Car.CurrentSpeedCmPerSec = Car.CruiseSpeedCmPerSec * 0.5f;
				Car.LateralPullOffset = 0.f;
				Car.State = ETrafficCarState::Cruising;
				FVector Loc;
				FRotator Rot;
				if (ComputeCarPose(Car, Loc, Rot))
				{
					Actor->SetActorLocationAndRotation(Loc, Rot);
				}
			}
			continue;
		}

		const FVector CurLoc = Actor->GetActorLocation();
		// Seamlessly recycle traffic cars that have fallen > 580 m behind the player onto edges around the player
		if (Player && FVector::Dist2D(CurLoc, PlayerLoc) > RecycleMaxDistCm)
		{
			int32 NewEdge = Car.EdgeIdx;
			bool bNewRev = Car.bReversed;
			float NewDist = 0.f;
			if (PickEdgeNearPlayer(PlayerLoc, RecycleMinDistCm, RecycleSpawnMaxCm, i, NewEdge, bNewRev, NewDist))
			{
				Car.EdgeIdx = NewEdge;
				Car.bReversed = bNewRev;
				Car.DistanceAlong = NewDist;
				Car.LateralPullOffset = 0.f;
				FVector Loc;
				FRotator Rot;
				if (ComputeCarPose(Car, Loc, Rot))
				{
					Actor->SetActorLocationAndRotation(Loc, Rot);
				}
				continue;
			}
		}

		// Yield for traffic cars directly ahead on same edge/direction, sirens, and stopped player
		float TargetSpeed = Car.CruiseSpeedCmPerSec;
		float DesiredLateralPull = 0.f;
		for (int32 j = 0; j < Cars.Num(); ++j)
		{
			if (j == i || Cars[j].EdgeIdx != Car.EdgeIdx || Cars[j].bReversed != Car.bReversed
				|| Cars[j].State != ETrafficCarState::Cruising)
			{
				continue;
			}
			const float Gap = Cars[j].DistanceAlong - Car.DistanceAlong;
			if (Gap > 0.f && Gap < YieldGapMaxCm)
			{
				TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec
					* FMath::Clamp((Gap - YieldGapStopCm) / (YieldGapMaxCm - YieldGapStopCm), 0.f, 1.f));
			}
		}

		const FVector CurFwd = Actor->GetActorForwardVector().GetSafeNormal2D();
		if (bPursuitActive)
		{
			for (const FVector& SirenPos : SirenPositions)
			{
				if (FVector::DistSquared2D(CurLoc, SirenPos) <= FMath::Square(SirenYieldRadiusCm))
				{
					TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec * 0.22f);
					DesiredLateralPull = 110.f;
					break;
				}
			}
		}
		if (Player)
		{
			const FVector ToPlayer = PlayerLoc - CurLoc;
			if (ToPlayer.Size2D() < 1550.f && FMath::Abs(ToPlayer.Z) < VerticalInteractionTolCm)
			{
				const float Along = FVector::DotProduct(ToPlayer, CurFwd);
				if (Along > 260.f && FMath::Abs(FVector::CrossProduct(CurFwd, ToPlayer).Z) < 260.f)
				{
					TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec * FMath::Clamp((Along - 520.f) / 1000.f, 0.f, 1.f));
				}
			}
		}

		const bool bBraking = TargetSpeed < Car.CurrentSpeedCmPerSec - 120.f;
		if (UMaterialInstanceDynamic* TailMid = Car.TaillightMid.Get())
		{
			TailMid->SetVectorParameterValue(TEXT("Emissive"),
				bBraking ? FLinearColor(9.0f, 0.22f, 0.08f, 1.f) : FLinearColor(1.8f, 0.08f, 0.04f, 1.f));
		}

		const float AccelRate = bBraking ? 1450.f : 680.f;
		Car.CurrentSpeedCmPerSec = FMath::FInterpConstantTo(Car.CurrentSpeedCmPerSec, TargetSpeed, Dt, AccelRate);
		Car.LateralPullOffset = FMath::FInterpTo(Car.LateralPullOffset, DesiredLateralPull, Dt, 2.5f);
		Car.DistanceAlong += Car.CurrentSpeedCmPerSec * Dt;

		const float EdgeLen = RoadGraph->GetEdgeLengthCm(Car.EdgeIdx);
		if (Car.DistanceAlong >= EdgeLen - 80.f)
		{
			AdvanceToNextEdge(Car);
		}

		FVector NextLoc;
		FRotator NextRot;
		if (!ComputeCarPose(Car, NextLoc, NextRot))
		{
			continue;
		}
		Actor->SetActorLocationAndRotation(NextLoc, NextRot);

		// Collision & Near-Miss interaction with player car
		if (Player && FMath::Abs(NextLoc.Z - PlayerLoc.Z) < VerticalInteractionTolCm)
		{
			const float Dist2D = FVector::Dist2D(NextLoc, PlayerLoc);
			const float RelSpeedKmh = (PlayerVel - NextRot.Vector() * Car.CurrentSpeedCmPerSec).Size2D() * CmPerSecToKmh;
			if (Dist2D < CollisionRadiusCm && RelSpeedKmh > CollisionMinRelSpeedKmh)
			{
				Car.State = ETrafficCarState::Knocked;
				Car.StateTimer = KnockDurationSeconds;
				Car.NearMissCooldown = 4.0f;

				const FVector Away2D = (NextLoc - PlayerLoc).GetSafeNormal2D();
				Car.KnockVelocity = (Away2D * 680.f + FVector(PlayerVel.X, PlayerVel.Y, 0.f) * 0.65f).GetClampedToMaxSize(1550.f);
				Car.KnockYawRateDeg = (FVector::CrossProduct(PlayerVel.GetSafeNormal2D(), Away2D).Z >= 0.f ? 1.f : -1.f) * 210.f;

				if (USkeletalMeshComponent* PlayerMesh = Player->GetMesh())
				{
					const FVector Vel = PlayerMesh->GetPhysicsLinearVelocity();
					PlayerMesh->SetPhysicsLinearVelocity(FVector(Vel.X * 0.88f, Vel.Y * 0.88f, Vel.Z));
				}
				if (UBNVehicleEffectsComponent* Fx = Player->FindComponentByClass<UBNVehicleEffectsComponent>())
				{
					Fx->TriggerCollisionSparks(0.5f * (NextLoc + PlayerLoc) + FVector(0.f, 0.f, 55.f), -Away2D, 1.f);
				}
				if (Pursuit)
				{
					Pursuit->ReportCollateralDamage(TrafficCollateralDollars, TEXT("Sivil Trafik Çarpışması"));
				}
				if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
				{
					Audio->PlaySfxAtLocation(EBNSfxId::CrashHeavy, 0.5f * (NextLoc + PlayerLoc), 0.95f);
				}
			}
			else if (Dist2D <= NearMissRadiusCm && Car.NearMissCooldown <= 0.f
				&& FMath::Abs(PlayerSpeedKmh) >= NearMissMinPlayerSpeedKmh
				&& Player->IsPlayerControlled() && !Player->IsInputLocked())
			{
				Car.NearMissCooldown = 3.0f;
				Player->GrantNearMissBonus();
				if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
				{
					Audio->PlaySfx2D(EBNSfxId::NearMissWhoosh, 0.85f);
				}
			}
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBNTrafficCmd(
	TEXT("bn.Traffic"),
	TEXT("bn.Traffic <0|1>  Enables or disables Karamara civilian traffic."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNTrafficSubsystem* Traffic = World ? World->GetSubsystem<UBNTrafficSubsystem>() : nullptr)
		{
			Traffic->SetTrafficEnabled(Args.Num() > 0 && Args[0].IsNumeric()
				? FCString::Atoi(*Args[0]) != 0
				: !Traffic->IsTrafficEnabled());
			UE_LOG(LogTemp, Display, TEXT("bn.Traffic: %s (%d active cars)"),
				Traffic->IsTrafficEnabled() ? TEXT("ON") : TEXT("OFF"), Traffic->GetActiveTrafficCount());
		}
	}));
