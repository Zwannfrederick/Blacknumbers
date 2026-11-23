#include "World/BNTrafficSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitSubsystem.h"
#include "ReferenceSkeleton.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNDayNightCycle.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float KmhToCmPerSec = 100000.f / 3600.f;

	// Exact Karamara street & viaduct coordinates from Tools/build_karamara_map.py.
	constexpr float RoadTopZ = 2.f;
	constexpr float ViaductY = -24000.f;
	constexpr float ViaductDeckZ = 700.f;
	constexpr float ViaductRampWStart = -22000.f;
	constexpr float ViaductDeckStart = -12000.f;
	constexpr float ViaductDeckEnd = 12000.f;
	constexpr float ViaductRampEEnd = 22000.f;

	// Right-hand lane offsets (road half-width 800 cm; +/- 420 cm leaves curb & centerline clearance).
	constexpr float InnerRingX = 19580.f, InnerRingY = 13580.f;
	constexpr float OuterRingX = 20420.f, OuterRingY = 14420.f;
	constexpr float ViaductEastY = ViaductY + 420.f, ViaductWestY = ViaductY - 420.f;
	constexpr float ViaductStartX = -24200.f, ViaductEndX = 25200.f;

	constexpr float YieldGapMaxCm = 1900.f, YieldGapStopCm = 820.f;
	constexpr float SirenYieldRadiusCm = 3200.f;
	constexpr float CollisionRadiusCm = 360.f, NearMissRadiusCm = 460.f;
	constexpr float CollisionMinRelSpeedKmh = 30.f, NearMissMinPlayerSpeedKmh = 80.f;
	constexpr float VerticalInteractionTolCm = 180.f;
	constexpr int32 TrafficCollateralDollars = 3500;
	constexpr float KnockDurationSeconds = 1.35f, PauseDurationSeconds = 2.2f;
	constexpr float MinRespawnPlayerDistCm = 17000.f;

	constexpr float HeadlightBeamCandelas = 3200.f, HeadlightBeamRadius = 2400.f;
	const FLinearColor HeadlightBeamColor(1.f, 0.94f, 0.82f);
	const FLinearColor HeadlightLensColor(1.75f, 1.65f, 1.35f, 0.90f);
	const FLinearColor TaillightLensColor(1.55f, 0.08f, 0.04f, 0.90f);

	const FLinearColor CivilianPalette[] = {
		FLinearColor(0.78f, 0.79f, 0.80f), FLinearColor(0.42f, 0.45f, 0.49f),
		FLinearColor(0.14f, 0.15f, 0.17f), FLinearColor(0.08f, 0.14f, 0.28f),
		FLinearColor(0.84f, 0.62f, 0.06f), FLinearColor(0.42f, 0.07f, 0.08f),
		FLinearColor(0.09f, 0.22f, 0.14f), FLinearColor(0.20f, 0.34f, 0.46f),
		FLinearColor(0.58f, 0.53f, 0.44f), FLinearColor(0.48f, 0.24f, 0.12f),
		FLinearColor(0.08f, 0.24f, 0.27f), FLinearColor(0.24f, 0.25f, 0.27f),
		FLinearColor(0.76f, 0.72f, 0.63f), FLinearColor(0.32f, 0.05f, 0.10f),
	};

	struct FTrafficSpawnSpec { int32 RouteIndex; float Fraction; float CruiseKmh; };

	// 14 civilian cars: Inner Ring CW (6), Outer Ring CCW (5), Viaduct East (2), Viaduct West (1).
	const FTrafficSpawnSpec SpawnSpecs[] = {
		{ 0, 0.06f, 54.f }, { 0, 0.23f, 58.f }, { 0, 0.40f, 50.f },
		{ 0, 0.56f, 60.f }, { 0, 0.73f, 52.f }, { 0, 0.90f, 56.f },
		{ 1, 0.10f, 52.f }, { 1, 0.30f, 58.f }, { 1, 0.50f, 48.f },
		{ 1, 0.70f, 56.f }, { 1, 0.88f, 54.f },
		{ 2, 0.36f, 74.f }, { 2, 0.72f, 80.f }, { 3, 0.34f, 76.f },
	};

	FVector ComputeRefBoneLocation(const USkeletalMesh& SkelMesh, const TCHAR* BoneName, const FVector& Fallback)
	{
		const FReferenceSkeleton& RefSkel = SkelMesh.GetRefSkeleton();
		int32 Idx = RefSkel.FindBoneIndex(FName(BoneName));
		if (Idx == INDEX_NONE) { return Fallback; }
		const TArray<FTransform>& RefPose = RefSkel.GetRefBonePose();
		FTransform CompTM = FTransform::Identity;
		while (Idx != INDEX_NONE && RefPose.IsValidIndex(Idx))
		{
			CompTM = CompTM * RefPose[Idx];
			Idx = RefSkel.GetParentIndex(Idx);
		}
		return CompTM.GetLocation();
	}

	void BuildRoundedLoopPoints(const TArray<FVector>& Corners, float Radius, int32 Subdivisions, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		const int32 N = Corners.Num();
		for (int32 i = 0; i < N && N >= 3; ++i)
		{
			const FVector& Prev = Corners[(i + N - 1) % N];
			const FVector& Curr = Corners[i];
			const FVector& Next = Corners[(i + 1) % N];
			const FVector P0 = Curr - (Curr - Prev).GetSafeNormal() * Radius;
			const FVector P2 = Curr + (Next - Curr).GetSafeNormal() * Radius;
			for (int32 s = 0; s <= Subdivisions; ++s)
			{
				const float T = static_cast<float>(s) / static_cast<float>(Subdivisions);
				const float Inv = 1.f - T;
				OutPoints.Add(Inv * Inv * P0 + 2.f * Inv * T * Curr + T * T * P2);
			}
		}
	}

	FVector SamplePolylinePoint(const TArray<FVector>& Points, const TArray<float>& CumDist,
		float TotalLength, bool bClosed, float Dist)
	{
		const int32 Count = Points.Num();
		if (Count == 0 || TotalLength <= KINDA_SMALL_NUMBER) { return FVector::ZeroVector; }
		if (bClosed)
		{
			Dist = FMath::Fmod(Dist, TotalLength);
			if (Dist < 0.f) { Dist += TotalLength; }
		}
		else
		{
			Dist = FMath::Clamp(Dist, 0.f, TotalLength);
		}

		const int32 SegCount = bClosed ? Count : (Count - 1);
		for (int32 i = 0; i < SegCount; ++i)
		{
			const float D0 = CumDist[i], D1 = CumDist[i + 1];
			if (Dist <= D1 || i == SegCount - 1)
			{
				const float Alpha = FMath::Clamp((Dist - D0) / FMath::Max(D1 - D0, KINDA_SMALL_NUMBER), 0.f, 1.f);
				return FMath::Lerp(Points[i], Points[(i + 1) % Count], Alpha);
			}
		}
		return Points[0];
	}

	UStaticMeshComponent* AttachTrafficMesh(AActor* Actor, USceneComponent* Root, UStaticMesh* Mesh,
		const FVector& RelLoc, const FRotator& RelRot, const FVector& RelScale, bool bCastShadow)
	{
		if (!Actor || !Root || !Mesh) { return nullptr; }
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Actor);
		Comp->SetStaticMesh(Mesh);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(bCastShadow);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetupAttachment(Root);
		Comp->SetRelativeLocationAndRotation(RelLoc, RelRot);
		Comp->SetRelativeScale3D(RelScale);
		Comp->RegisterComponent();
		return Comp;
	}
}

float UBNTrafficSubsystem::SampleViaductElevationZ(float X)
{
	if (X <= ViaductRampWStart || X >= ViaductRampEEnd) { return RoadTopZ; }
	if (X < ViaductDeckStart)
	{
		return FMath::Lerp(RoadTopZ, ViaductDeckZ, (X - ViaductRampWStart) / (ViaductDeckStart - ViaductRampWStart));
	}
	if (X <= ViaductDeckEnd) { return ViaductDeckZ; }
	return FMath::Lerp(ViaductDeckZ, RoadTopZ, (X - ViaductDeckEnd) / (ViaductRampEEnd - ViaductDeckEnd));
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

	BuildKaramaraRoutes();
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
	Routes.Reset();
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

void UBNTrafficSubsystem::BuildKaramaraRoutes()
{
	Routes.Reset();
	auto FinalizeRoute = [](FTrafficRoute& Route)
	{
		Route.CumulativeDist.Reset();
		Route.TotalLength = 0.f;
		const int32 N = Route.Points.Num();
		if (N < 2) { return; }
		Route.CumulativeDist.Add(0.f);
		const int32 SegCount = Route.bClosed ? N : (N - 1);
		for (int32 i = 0; i < SegCount; ++i)
		{
			Route.TotalLength += FVector::Dist(Route.Points[i], Route.Points[(i + 1) % N]);
			Route.CumulativeDist.Add(Route.TotalLength);
		}
	};

	// Route 0 (Inner CW ring) & Route 1 (Outer CCW ring): non-overlapping right-hand lanes across all 4 boulevards.
	FTrafficRoute LoopCW;
	LoopCW.bClosed = true;
	BuildRoundedLoopPoints({
		FVector(-InnerRingX, -InnerRingY, RoadTopZ), FVector( InnerRingX, -InnerRingY, RoadTopZ),
		FVector( InnerRingX,  InnerRingY, RoadTopZ), FVector(-InnerRingX,  InnerRingY, RoadTopZ)
	}, 700.f, 6, LoopCW.Points);
	FinalizeRoute(LoopCW);
	Routes.Add(MoveTemp(LoopCW));

	FTrafficRoute LoopCCW;
	LoopCCW.bClosed = true;
	BuildRoundedLoopPoints({
		FVector( OuterRingX, -OuterRingY, RoadTopZ), FVector(-OuterRingX, -OuterRingY, RoadTopZ),
		FVector(-OuterRingX,  OuterRingY, RoadTopZ), FVector( OuterRingX,  OuterRingY, RoadTopZ)
	}, 1400.f, 6, LoopCCW.Points);
	FinalizeRoute(LoopCCW);
	Routes.Add(MoveTemp(LoopCCW));

	// Route 2 (Kuzey Viyadugu Eastbound) & Route 3 (Westbound): smooth 3.99-deg ramps up to Z = 700 deck.
	FTrafficRoute ViaductE;
	ViaductE.bClosed = false;
	for (float X = ViaductStartX; X <= ViaductEndX + 1.f; X += 500.f)
	{
		const float CX = FMath::Min(X, ViaductEndX);
		ViaductE.Points.Add(FVector(CX, ViaductEastY, SampleViaductElevationZ(CX)));
	}
	FinalizeRoute(ViaductE);
	Routes.Add(MoveTemp(ViaductE));

	FTrafficRoute ViaductW;
	ViaductW.bClosed = false;
	for (float X = ViaductEndX; X >= ViaductStartX - 1.f; X -= 500.f)
	{
		const float CX = FMath::Max(X, ViaductStartX);
		ViaductW.Points.Add(FVector(CX, ViaductWestY, SampleViaductElevationZ(CX)));
	}
	FinalizeRoute(ViaductW);
	Routes.Add(MoveTemp(ViaductW));
}

void UBNTrafficSubsystem::LoadTrafficAssets()
{
	if (!CarBodyMesh) { CarBodyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar")); }
	if (!CarGlassMesh) { CarGlassMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass")); }
	if (!CarWheelMesh) { CarWheelMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel")); }
	if (!CubeMesh) { CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); }
	if (!LightLensMaterial)
	{
		LightLensMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
		if (!LightLensMaterial)
		{
			LightLensMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
	}
	if (const USkeletalMesh* Skel = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar")))
	{
		static const TCHAR* const Bones[4] = { TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"), TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR") };
		for (int32 i = 0; i < 4; ++i)
		{
			WheelLocalOffsets[i] = ComputeRefBoneLocation(*Skel, Bones[i], WheelLocalOffsets[i]);
		}
	}
}

void UBNTrafficSubsystem::SampleRoute(int32 RouteIndex, float Distance, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!Routes.IsValidIndex(RouteIndex))
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}
	const FTrafficRoute& R = Routes[RouteIndex];
	OutLocation = SamplePolylinePoint(R.Points, R.CumulativeDist, R.TotalLength, R.bClosed, Distance);
	const float Ahead = R.bClosed ? (Distance + 220.f) : FMath::Min(Distance + 220.f, R.TotalLength);
	const float Behind = R.bClosed ? (Distance - 120.f) : FMath::Max(Distance - 120.f, 0.f);
	const FVector Dir = (SamplePolylinePoint(R.Points, R.CumulativeDist, R.TotalLength, R.bClosed, Ahead)
		- SamplePolylinePoint(R.Points, R.CumulativeDist, R.TotalLength, R.bClosed, Behind)).GetSafeNormal();
	OutRotation = Dir.IsNearlyZero() ? FRotator::ZeroRotator : Dir.Rotation();
}

void UBNTrafficSubsystem::SpawnAllTraffic()
{
	ClearTraffic();
	UWorld* World = GetWorld();
	if (!World || Routes.IsEmpty()) { return; }

	const ABNVehiclePawn* Player = FindPlayerCar();
	const FVector AvoidLoc = Player ? Player->GetActorLocation() : FVector(-24500.f, -14000.f, RoadTopZ);

	for (int32 i = 0; i < UE_ARRAY_COUNT(SpawnSpecs); ++i)
	{
		const FTrafficSpawnSpec& Spec = SpawnSpecs[i];
		if (!Routes.IsValidIndex(Spec.RouteIndex) || Routes[Spec.RouteIndex].TotalLength <= 0.f) { continue; }

		float StartDist = Routes[Spec.RouteIndex].TotalLength * Spec.Fraction;
		FVector SpawnLoc;
		FRotator SpawnRot;
		SampleRoute(Spec.RouteIndex, StartDist, SpawnLoc, SpawnRot);
		if (FVector::Dist2D(SpawnLoc, AvoidLoc) < 2400.f)
		{
			StartDist = FMath::Fmod(StartDist + 4500.f, Routes[Spec.RouteIndex].TotalLength);
			SampleRoute(Spec.RouteIndex, StartDist, SpawnLoc, SpawnRot);
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(SpawnRot, SpawnLoc), Params);
		if (!Actor) { continue; }

		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("TrafficRoot"));
		Root->SetMobility(EComponentMobility::Movable);
		Actor->SetRootComponent(Root);
		Root->SetRelativeLocationAndRotation(SpawnLoc, SpawnRot);
		Root->RegisterComponent();
		Actor->SetActorLocationAndRotation(SpawnLoc, SpawnRot);

		FTrafficCar Car;
		Car.Actor = Actor;
		Car.RouteIndex = Spec.RouteIndex;
		Car.DistanceAlong = StartDist;
		Car.CruiseSpeedCmPerSec = Spec.CruiseKmh * KmhToCmPerSec;
		Car.CurrentSpeedCmPerSec = Car.CruiseSpeedCmPerSec;

		if (UStaticMeshComponent* Body = AttachTrafficMesh(Actor, Root, CarBodyMesh, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, true))
		{
			const FLinearColor Paint = CivilianPalette[i % UE_ARRAY_COUNT(CivilianPalette)];
			for (int32 m = 0; m < Body->GetNumMaterials(); ++m)
			{
				if (UMaterialInterface* Mat = Body->GetMaterial(m); Mat && Mat->GetName().Contains(TEXT("SportsCarBody")))
				{
					if (UMaterialInstanceDynamic* Mid = Body->CreateDynamicMaterialInstance(m, Mat))
					{
						Mid->SetVectorParameterValue(TEXT("Paint Tint"), Paint);
					}
					break;
				}
			}
		}
		AttachTrafficMesh(Actor, Root, CarGlassMesh, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, false);

		for (int32 w = 0; w < 4; ++w)
		{
			const FRotator WheelRot = (w % 2 == 0) ? FRotator(0.f, 180.f, 0.f) : FRotator::ZeroRotator;
			if (UStaticMeshComponent* Wheel = AttachTrafficMesh(Actor, Root, CarWheelMesh, WheelLocalOffsets[w], WheelRot, FVector::OneVector, false))
			{
				Car.Wheels.Add(Wheel);
			}
		}

		for (const float Side : { -1.f, 1.f })
		{
			auto AddLens = [&](const FVector& Pos, const FVector& Scale, const FLinearColor& Col)
			{
				if (UStaticMeshComponent* Lens = AttachTrafficMesh(Actor, Root, CubeMesh, Pos, FRotator::ZeroRotator, Scale, false))
				{
					if (LightLensMaterial)
					{
						if (UMaterialInstanceDynamic* Mid = Lens->CreateDynamicMaterialInstance(0, LightLensMaterial))
						{
							Mid->SetVectorParameterValue(TEXT("Color"), Col);
						}
					}
					Car.LightLenses.Add(Lens);
				}
			};
			AddLens(FVector(236.f, Side * 70.f, 62.f), FVector(0.06f, 0.24f, 0.10f), HeadlightLensColor);
			AddLens(FVector(-224.f, Side * 68.f, 66.f), FVector(0.05f, 0.30f, 0.08f), TaillightLensColor);
		}

		USpotLightComponent* Beam = NewObject<USpotLightComponent>(Actor);
		Beam->SetMobility(EComponentMobility::Movable);
		Beam->SetupAttachment(Root);
		Beam->SetRelativeLocationAndRotation(FVector(255.f, 0.f, 68.f), FRotator(-6.f, 0.f, 0.f));
		Beam->SetIntensityUnits(ELightUnits::Candelas);
		Beam->SetIntensity(HeadlightBeamCandelas);
		Beam->SetLightColor(HeadlightBeamColor);
		Beam->SetAttenuationRadius(HeadlightBeamRadius);
		Beam->SetInnerConeAngle(16.f);
		Beam->SetOuterConeAngle(36.f);
		Beam->SetCastShadows(false);
		Beam->SetMaxDrawDistance(12000.f);
		Beam->SetMaxDistanceFadeRange(2500.f);
		Beam->RegisterComponent();
		Car.HeadlightBeam = Beam;

		ApplyNightLightsToCar(Car, bNightLightsOn);
		Cars.Add(MoveTemp(Car));
	}
}

void UBNTrafficSubsystem::ApplyNightLightsToCar(const FTrafficCar& Car, bool bOn) const
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& Lens : Car.LightLenses)
	{
		if (UStaticMeshComponent* Comp = Lens.Get()) { Comp->SetVisibility(bOn, true); }
	}
	if (USpotLightComponent* Beam = Car.HeadlightBeam.Get()) { Beam->SetVisibility(bOn); }
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

float UBNTrafficSubsystem::FindSafeRespawnDistance(int32 RouteIndex, const FVector& PlayerLoc, int32 IgnoreCarIndex) const
{
	if (!Routes.IsValidIndex(RouteIndex) || Routes[RouteIndex].TotalLength <= 0.f) { return 0.f; }
	const float TotalLen = Routes[RouteIndex].TotalLength;
	float BestDist = 0.f, BestScore = -1.f;

	for (int32 c = 0; c < 12; ++c)
	{
		const float CandidateDist = TotalLen * (static_cast<float>(c) + 0.5f) / 12.f;
		FVector Loc;
		FRotator Rot;
		SampleRoute(RouteIndex, CandidateDist, Loc, Rot);

		float MinTrafficGap = TotalLen;
		for (int32 j = 0; j < Cars.Num(); ++j)
		{
			if (j == IgnoreCarIndex || Cars[j].RouteIndex != RouteIndex) { continue; }
			const float RawDiff = FMath::Abs(Cars[j].DistanceAlong - CandidateDist);
			MinTrafficGap = FMath::Min(MinTrafficGap, Routes[RouteIndex].bClosed ? FMath::Min(RawDiff, TotalLen - RawDiff) : RawDiff);
		}
		const float Score = FMath::Min(FVector::Dist2D(Loc, PlayerLoc), MinRespawnPlayerDistCm * 1.4f) + FMath::Min(MinTrafficGap, 6000.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDist = CandidateDist;
		}
	}
	return BestDist;
}

void UBNTrafficSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.1f);
	UWorld* World = GetWorld();
	if (!bIsKaramaraMap || !bTrafficEnabled || Cars.IsEmpty() || !World || Dt <= 0.f) { return; }

	ABNVehiclePawn* Player = FindPlayerCar();
	const FVector PlayerLoc = Player ? Player->GetActorLocation() : FVector(BIG_NUMBER);
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
		if (!Actor || !Routes.IsValidIndex(Car.RouteIndex)) { continue; }
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
				Car.DistanceAlong = FindSafeRespawnDistance(Car.RouteIndex, PlayerLoc, i);
				Car.CurrentSpeedCmPerSec = Car.CruiseSpeedCmPerSec * 0.5f;
				Car.LateralPullOffset = 0.f;
				Car.State = ETrafficCarState::Cruising;
				FVector RespawnLoc;
				FRotator RespawnRot;
				SampleRoute(Car.RouteIndex, Car.DistanceAlong, RespawnLoc, RespawnRot);
				Actor->SetActorLocationAndRotation(RespawnLoc, RespawnRot);
			}
			continue;
		}

		// Cruising: yield smoothly for cars ahead, nearby police sirens, and a slow/stopped player in lane.
		const FTrafficRoute& Route = Routes[Car.RouteIndex];
		float TargetSpeed = Car.CruiseSpeedCmPerSec;
		float DesiredLateralPull = 0.f;

		for (int32 j = 0; j < Cars.Num(); ++j)
		{
			if (j == i || Cars[j].RouteIndex != Car.RouteIndex || Cars[j].State != ETrafficCarState::Cruising) { continue; }
			float Gap = Cars[j].DistanceAlong - Car.DistanceAlong;
			if (Route.bClosed && Gap < 0.f) { Gap += Route.TotalLength; }
			if (Gap > 0.f && Gap < YieldGapMaxCm)
			{
				TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec
					* FMath::Clamp((Gap - YieldGapStopCm) / (YieldGapMaxCm - YieldGapStopCm), 0.f, 1.f));
			}
		}

		const FVector CurLoc = Actor->GetActorLocation();
		const FVector CurFwd = Actor->GetActorForwardVector().GetSafeNormal2D();
		if (bPursuitActive)
		{
			for (const FVector& SirenPos : SirenPositions)
			{
				if (FVector::DistSquared2D(CurLoc, SirenPos) <= FMath::Square(SirenYieldRadiusCm))
				{
					TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec * 0.22f);
					DesiredLateralPull = 95.f;
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
				if (Along > 260.f && FMath::Abs(FVector::CrossProduct(CurFwd, ToPlayer).Z) < 250.f)
				{
					TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeedCmPerSec * FMath::Clamp((Along - 520.f) / 1000.f, 0.f, 1.f));
				}
			}
		}

		const float AccelRate = (TargetSpeed < Car.CurrentSpeedCmPerSec) ? 1400.f : 650.f;
		Car.CurrentSpeedCmPerSec = FMath::FInterpConstantTo(Car.CurrentSpeedCmPerSec, TargetSpeed, Dt, AccelRate);
		Car.LateralPullOffset = FMath::FInterpTo(Car.LateralPullOffset, DesiredLateralPull, Dt, 2.5f);
		Car.DistanceAlong += Car.CurrentSpeedCmPerSec * Dt;
		if (Route.TotalLength > KINDA_SMALL_NUMBER)
		{
			if (Route.bClosed) { Car.DistanceAlong = FMath::Fmod(Car.DistanceAlong, Route.TotalLength); }
			else if (Car.DistanceAlong >= Route.TotalLength) { Car.DistanceAlong = 0.f; }
		}

		FVector NextLoc;
		FRotator NextRot;
		SampleRoute(Car.RouteIndex, Car.DistanceAlong, NextLoc, NextRot);
		if (Car.LateralPullOffset > 1.f)
		{
			NextLoc += FRotationMatrix(NextRot).GetUnitAxis(EAxis::Y).GetSafeNormal2D() * Car.LateralPullOffset;
		}
		Actor->SetActorLocationAndRotation(NextLoc, NextRot);

		Car.WheelSpinDeg = FMath::Fmod(Car.WheelSpinDeg + FMath::RadiansToDegrees((Car.CurrentSpeedCmPerSec * Dt) / 40.f), 360.f);
		for (int32 w = 0; w < Car.Wheels.Num(); ++w)
		{
			if (UStaticMeshComponent* Wheel = Car.Wheels[w].Get())
			{
				Wheel->SetRelativeRotation((w % 2 == 0)
					? FRotator(Car.WheelSpinDeg, 180.f, 0.f)
					: FRotator(-Car.WheelSpinDeg, 0.f, 0.f));
			}
		}

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
					Pursuit->ReportCollateralDamage(TrafficCollateralDollars, TEXT("Sivil Trafik Carpismasi"));
				}
			}
			else if (Dist2D <= NearMissRadiusCm && Car.NearMissCooldown <= 0.f
				&& FMath::Abs(PlayerSpeedKmh) >= NearMissMinPlayerSpeedKmh
				&& Player->IsPlayerControlled() && !Player->IsInputLocked())
			{
				Car.NearMissCooldown = 3.0f;
				Player->GrantNearMissBonus();
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
