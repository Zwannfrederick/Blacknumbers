#include "Police/BNPoliceController.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Garage/BNGarageTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Police/BNPoliceSirenComponent.h"
#include "Police/BNPoliceTactics.h"
#include "Police/BNPursuitSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"
#include "Vehicle/BNVehicleVisuals.h"
#include "World/BNRoadGraph.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float CmPerSecToMph = 0.022369f;
	constexpr float MinChaseSpeedKmh = 68.f;
	constexpr float SearchCruiseSpeedKmh = 95.f;
	constexpr float PatrolCruiseSpeedKmh = 58.f;
	constexpr float FullThrottleError = 300.f;
	constexpr float FullBrakeError = 520.f;
	constexpr float MinBrakingSpeed = 200.f;
	constexpr float GripAssistDegPerSec = 150.f;
	constexpr float YawAssistGain = 1.90f;
	constexpr float YawAssistMaxDegPerSec = 70.f;
	constexpr float MinGripAssistSpeed = 420.f;
	constexpr float StuckSpeed = 210.f;
	constexpr float UpsideDownZ = 0.35f;
	constexpr float ReverseTriggerSeconds = 0.90f;
	constexpr float ReverseDurationSeconds = 0.90f;
	constexpr float StuckSecondsToRecover = 4.2f;
	constexpr float MinRecoverDistCm = 2400.f;
	constexpr float YelpDistanceCm = 3000.f;
	constexpr float DirectChaseMaxDistCm = 4200.f;
	constexpr float WaypointReachDistCm = 1350.f;
	constexpr float RepathIntervalSeconds = 1.0f;

	constexpr float StrobeCandelas = 280.f;
	constexpr float DisabledStrobeCandelas = 80.f;
	constexpr float StrobeRadius = 1100.f;
	constexpr float StrobeRateHz = 7.f;
	const FLinearColor RedStrobeColor(1.f, 0.06f, 0.04f);
	const FLinearColor BlueStrobeColor(0.08f, 0.35f, 1.f);
}

ABNPoliceController::ABNPoliceController()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;
}

void ABNPoliceController::SetupPursuitUnit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec,
	int32 InFormationSlot, bool bInRoadblockUnit, bool bInHeavyRhino)
{
	TargetCar = InTarget;
	HeatSpec = InSpec;
	FormationSlot = InFormationSlot;
	bRoadblockUnit = bInRoadblockUnit;
	bHeavyRhino = bInHeavyRhino;
	bPatrolUnit = false;
	bDisabled = false;
	bSearchMode = false;
	DisabledSeconds = 0.f;
	StuckSeconds = 0.f;
	ReverseUnstuckSeconds = 0.f;
	ReverseCooldownSeconds = 0.f;
	NavWaypoints.Reset();
	NavWaypointIndex = 0;
	NavRepathCooldown = 0.f;

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		if (bHeavyRhino)
		{
			const int32 RhinoTiers[FBNUpgradeSpec::NumSlots] = { 4, 4, 4, 4, 3 };
			const FBNComputedVehicleStats Stats = FBNComputedVehicleStats::Compute(EBNCarId::TauronV6Rally, RhinoTiers);
			Car->ApplyVehicleBuild(Stats, FLinearColor(0.022f, 0.025f, 0.030f, 1.f));
		}
		AttachPoliceGear(*Car);
		Car->SetInputLocked(bRoadblockUnit);
	}
	UpdateLightsAndSiren();
}

void ABNPoliceController::SetupPatrolUnit(int32 StartEdgeIdx, bool bStartReversed)
{
	TargetCar = nullptr;
	HeatSpec = FBNHeatTierSpec::ForLevel(1);
	FormationSlot = 0;
	bRoadblockUnit = false;
	bHeavyRhino = false;
	bPatrolUnit = true;
	bDisabled = false;
	bSearchMode = false;
	PatrolEdgeIdx = StartEdgeIdx;
	bPatrolReversed = bStartReversed;
	PatrolDistAlong = 400.f;
	StuckSeconds = 0.f;

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		AttachPoliceGear(*Car);
		Car->SetInputLocked(false);
	}
	UpdateLightsAndSiren();
}

void ABNPoliceController::PromotePatrolToPursuit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec, int32 InFormationSlot)
{
	TargetCar = InTarget;
	HeatSpec = InSpec;
	FormationSlot = InFormationSlot;
	bPatrolUnit = false;
	bRoadblockUnit = false;
	bSearchMode = false;
	NavWaypoints.Reset();
	NavWaypointIndex = 0;
	NavRepathCooldown = 0.f;
	UpdateLightsAndSiren();
}

void ABNPoliceController::UpdateHeatSpec(const FBNHeatTierSpec& InSpec)
{
	HeatSpec = InSpec;
}

void ABNPoliceController::SetSearchMode(bool bInSearchMode, const FVector& InSearchPoint)
{
	bSearchMode = bInSearchMode;
	SearchPoint = InSearchPoint;
}

void ABNPoliceController::DisableUnit()
{
	if (bDisabled)
	{
		return;
	}
	bDisabled = true;
	DisabledSeconds = 0.f;
	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		Car->SetInputLocked(true);
	}
	UpdateLightsAndSiren();
}

bool ABNPoliceController::IsSirenActive() const
{
	return Siren && Siren->IsSirenActive();
}

void ABNPoliceController::AttachPoliceGear(ABNVehiclePawn& Car)
{
	if (bGearAttached)
	{
		return;
	}
	USkeletalMeshComponent* Body = Car.GetMesh();
	if (!Body)
	{
		return;
	}
	bGearAttached = true;

	// Heavy Rhino: prefer real CC-BY 4.0 BMW X5 armored SUV (/Game/Vehicles/External/SM_Ext_HeavyRhino) with live Chaos wheels,
	// falling back to SM_BN_Traffic_SUV if external vehicle import was skipped.
	bool bUsingRhinoSuv = false;
	bool bUsingExternalRhino = false;
	if (bHeavyRhino && Car.Chassis)
	{
		UStaticMesh* ExtRhino = FPackageName::DoesPackageExist(TEXT("/Game/Vehicles/External/SM_Ext_HeavyRhino"))
			? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/External/SM_Ext_HeavyRhino.SM_Ext_HeavyRhino"))
			: nullptr;
		UStaticMesh* FallbackSuv = (!ExtRhino && FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_Traffic_SUV")))
			? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_Traffic_SUV.SM_BN_Traffic_SUV"))
			: nullptr;
		if (UStaticMesh* SuvMesh = ExtRhino ? ExtRhino : FallbackSuv)
		{
			bUsingRhinoSuv = true;
			bUsingExternalRhino = (ExtRhino != nullptr);
			Car.Chassis->EmptyOverrideMaterials();
			Car.Chassis->SetStaticMesh(SuvMesh);
			if (bUsingExternalRhino)
			{
				const FBox B = SuvMesh->GetBoundingBox();
				const FVector C = B.IsValid ? B.GetCenter() : FVector::ZeroVector;
				const float OffsetZ = 16.f - (B.IsValid ? B.Min.Z * 1.03f : 0.f);
				Car.Chassis->SetRelativeLocation(FVector(10.f - C.X * 1.03f, -C.Y * 1.05f, OffsetZ));
				Car.Chassis->SetRelativeRotation(FRotator::ZeroRotator);
				Car.Chassis->SetRelativeScale3D(FVector(1.03f, 1.05f, 1.03f));
				// Bind precompiled dark tactical obsidian PBR materials immediately (zero shader-compile white flash!)
				UMaterialInterface* DarkMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Metal_Dark.MI_Metal_Dark"));
				UMaterialInterface* TrimMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Traffic_Trim.MI_Traffic_Trim"));
				UMaterialInterface* GlassMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Traffic_Glass.MI_Traffic_Glass"));
				UMaterialInterface* TailMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Traffic_Taillight.MI_Traffic_Taillight"));
				UMaterialInterface* HeadMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Traffic_Headlight.MI_Traffic_Headlight"));
				const TArray<FName> SlotNames = Car.Chassis->GetMaterialSlotNames();
				for (int32 s = 0; s < Car.Chassis->GetNumMaterials(); ++s)
				{
					const FString SN = SlotNames.IsValidIndex(s) ? SlotNames[s].ToString() : FString();
					if (SN.Contains(TEXT("Glass")) && GlassMat) { Car.Chassis->SetMaterial(s, GlassMat); }
					else if (SN.Contains(TEXT("Tail")) && TailMat) { Car.Chassis->SetMaterial(s, TailMat); }
					else if (SN.Contains(TEXT("Head")) && HeadMat) { Car.Chassis->SetMaterial(s, HeadMat); }
					else if (SN.Contains(TEXT("Trim")) && TrimMat) { Car.Chassis->SetMaterial(s, TrimMat); }
					else if (DarkMat)
					{
						if (UMaterialInstanceDynamic* Mid = Car.Chassis->CreateDynamicMaterialInstance(s, DarkMat))
						{
							Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.015f, 0.018f, 0.026f, 1.f));
							Mid->SetScalarParameterValue(TEXT("Roughness"), 0.50f);
							Mid->SetScalarParameterValue(TEXT("Metallic"), 0.28f);
						}
					}
				}
			}
			else
			{
				Car.Chassis->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
				Car.Chassis->SetRelativeRotation(FRotator::ZeroRotator);
				Car.Chassis->SetRelativeScale3D(FVector(1.06f, 1.08f, 1.06f));
			}
			if (Car.ChassisGlass) { Car.ChassisGlass->SetVisibility(false); }
			for (UStaticMeshComponent* W : Car.WheelMeshes)
			{
				if (W) { W->SetVisibility(bUsingExternalRhino); }
			}
			if (Car.Visuals) { Car.Visuals->SetVisibility(false, true); }
			if (!bUsingExternalRhino)
			{
				for (int32 SlotIdx = 0; SlotIdx < Car.Chassis->GetNumMaterials(); ++SlotIdx)
				{
					UMaterialInterface* Mat = Car.Chassis->GetMaterial(SlotIdx);
					if (!Mat) { continue; }
					const FString MName = Mat->GetName();
					if (MName.Contains(TEXT("Traffic_Paint")) || SlotIdx == 0)
					{
						if (UMaterialInstanceDynamic* Mid = Car.Chassis->CreateDynamicMaterialInstance(SlotIdx, Mat))
						{
							Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.018f, 0.022f, 0.028f, 1.f));
							Mid->SetScalarParameterValue(TEXT("Roughness"), 0.56f);
							Mid->SetScalarParameterValue(TEXT("Metallic"), 0.20f);
						}
					}
				}
			}
		}
	}

	const float RoofZ = bUsingExternalRhino ? 182.f : (bUsingRhinoSuv ? 186.f : (bHeavyRhino ? 174.f : 123.f));
	UStaticMesh* BarMesh = FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_PoliceLightbar"))
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_PoliceLightbar.SM_BN_PoliceLightbar"))
		: nullptr;
	if (BarMesh || CubeMesh)
	{
		LightbarMesh = NewObject<UStaticMeshComponent>(&Car, TEXT("PoliceLightbar"));
		LightbarMesh->SetStaticMesh(BarMesh ? BarMesh : CubeMesh.Get());
		LightbarMesh->SetupAttachment(Body);
		LightbarMesh->SetRelativeLocation(FVector(bUsingRhinoSuv ? -20.f : -12.f, 0.f, RoofZ));
		LightbarMesh->SetRelativeScale3D(BarMesh ? FVector::OneVector : FVector(0.35f, 1.15f, 0.14f));
		LightbarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LightbarMesh->SetCastShadow(false);
		LightbarMesh->RegisterComponent();
	}

	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_PushBar")))
	{
		if (UStaticMesh* RamMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_PushBar.SM_BN_PushBar")))
		{
			PushBarMesh = NewObject<UStaticMeshComponent>(&Car, TEXT("PolicePushBar"));
			PushBarMesh->SetStaticMesh(RamMesh);
			PushBarMesh->SetupAttachment(Body);
			PushBarMesh->SetRelativeLocation(FVector(bUsingRhinoSuv ? 252.f : (bHeavyRhino ? 248.f : 238.f), 0.f, bUsingRhinoSuv ? 56.f : (bHeavyRhino ? 52.f : 44.f)));
			PushBarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PushBarMesh->SetCastShadow(false);
			PushBarMesh->RegisterComponent();
		}
	}

	auto MakeStrobe = [&](const TCHAR* Name, float Y, const FLinearColor& Color) -> UPointLightComponent*
	{
		UPointLightComponent* Strobe = NewObject<UPointLightComponent>(&Car, Name);
		Strobe->SetupAttachment(Body);
		Strobe->SetRelativeLocation(FVector(-12.f, Y, RoofZ + 22.f));
		Strobe->SetIntensityUnits(ELightUnits::Candelas);
		Strobe->SetIntensity(StrobeCandelas);
		Strobe->SetLightColor(Color);
		Strobe->SetAttenuationRadius(StrobeRadius);
		Strobe->SetSpecularScale(0.18f);
		Strobe->SetCastShadows(false);
		Strobe->RegisterComponent();
		return Strobe;
	};

	RedLight = MakeStrobe(TEXT("PoliceRedStrobe"), -45.f, RedStrobeColor);
	BlueLight = MakeStrobe(TEXT("PoliceBlueStrobe"), 45.f, BlueStrobeColor);

	Siren = NewObject<UBNPoliceSirenComponent>(&Car, TEXT("PoliceSiren"));
	Siren->SetupAttachment(Body);
	Siren->RegisterComponent();

	// Matte obsidian tactical finish for Heavy Rhino SUV; classic black-and-white interceptor livery for cruisers.
	if (!bUsingRhinoSuv)
	{
		Car.ApplyPaintTint(bHeavyRhino ? FLinearColor(0.025f, 0.028f, 0.032f, 1.f) : FLinearColor(0.03f, 0.045f, 0.08f, 1.f));
	}
	if (!bHeavyRhino && CubeMesh)
	{
		UMaterialInterface* WhiteMat = FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_Traffic_Paint"))
			? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_Traffic_Paint.MI_Traffic_Paint"))
			: nullptr;
		for (const float Side : { -1.f, 1.f })
		{
			UStaticMeshComponent* DoorStripe = NewObject<UStaticMeshComponent>(&Car,
				Side < 0.f ? TEXT("PoliceDoorL") : TEXT("PoliceDoorR"));
			DoorStripe->SetStaticMesh(CubeMesh);
			DoorStripe->SetupAttachment(Body);
			DoorStripe->SetRelativeLocation(FVector(4.f, Side * 93.f, 52.f));
			DoorStripe->SetRelativeScale3D(FVector(1.36f, 0.025f, 0.34f));
			DoorStripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DoorStripe->SetCastShadow(false);
			if (WhiteMat)
			{
				DoorStripe->SetMaterial(0, WhiteMat);
			}
			DoorStripe->RegisterComponent();
			LiveryPanels.Add(DoorStripe);
		}
	}
}

void ABNPoliceController::CleanupPoliceGear()
{
	if (LightbarMesh) { LightbarMesh->DestroyComponent(); LightbarMesh = nullptr; }
	if (PushBarMesh) { PushBarMesh->DestroyComponent(); PushBarMesh = nullptr; }
	for (UStaticMeshComponent* Panel : LiveryPanels)
	{
		if (IsValid(Panel))
		{
			Panel->DestroyComponent();
		}
	}
	LiveryPanels.Reset();
	if (RedLight) { RedLight->DestroyComponent(); RedLight = nullptr; }
	if (BlueLight) { BlueLight->DestroyComponent(); BlueLight = nullptr; }
	if (Siren)
	{
		Siren->SetSirenState(false, false, 1.f);
		Siren->DestroyComponent();
		Siren = nullptr;
	}
	bGearAttached = false;
}

void ABNPoliceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupPoliceGear();
	Super::EndPlay(EndPlayReason);
}

void ABNPoliceController::UpdateLightsAndSiren()
{
	const UWorld* World = GetWorld();
	const double Time = World ? World->GetTimeSeconds() : 0.0;
	const bool bRedPhase = FMath::Fmod(Time * StrobeRateHz, 1.0) < 0.5;
	const float Dilation = World && World->GetWorldSettings() ? World->GetWorldSettings()->GetEffectiveTimeDilation() : 1.f;

	if (bPatrolUnit)
	{
		// Calm free-roam patrol: slow subtle blue cruise beacon, sirens silent until pursuit begins.
		if (RedLight) { RedLight->SetVisibility(false); }
		if (BlueLight)
		{
			BlueLight->SetIntensity(75.f);
			BlueLight->SetVisibility(FMath::Fmod(Time * 1.4, 1.0) < 0.45);
		}
		if (Siren) { Siren->SetSirenState(false, false, Dilation); }
		return;
	}

	if (bDisabled)
	{
		const bool bHazard = FMath::Fmod(Time * 2.0, 1.0) < 0.5;
		if (RedLight)
		{
			RedLight->SetIntensity(DisabledStrobeCandelas);
			RedLight->SetVisibility(bHazard);
		}
		if (BlueLight)
		{
			BlueLight->SetIntensity(DisabledStrobeCandelas);
			BlueLight->SetVisibility(false);
		}
		if (Siren) { Siren->SetSirenState(false, false, Dilation); }
		return;
	}

	if (RedLight)
	{
		RedLight->SetIntensity(StrobeCandelas);
		RedLight->SetVisibility(bRedPhase);
	}
	if (BlueLight)
	{
		BlueLight->SetIntensity(StrobeCandelas);
		BlueLight->SetVisibility(!bRedPhase);
	}

	if (Siren)
	{
		const ABNVehiclePawn* MyCar = Cast<ABNVehiclePawn>(GetPawn());
		const ABNVehiclePawn* Target = TargetCar.Get();
		const float Dist = (MyCar && Target) ? FVector::Dist2D(MyCar->GetActorLocation(), Target->GetActorLocation()) : 99999.f;
		const bool bYelp = Dist < YelpDistanceCm;
		Siren->SetSirenState(!bRoadblockUnit, bYelp, Dilation);
	}
}

void ABNPoliceController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn());
	if (!Car)
	{
		return;
	}

	if (bDisabled)
	{
		DisabledSeconds += DeltaSeconds;
		UpdateLightsAndSiren();
		return;
	}

	UpdateLightsAndSiren();

	if (bRoadblockUnit || Car->IsInputLocked())
	{
		return;
	}

	if (bPatrolUnit)
	{
		DrivePatrol(*Car, DeltaSeconds);
		return;
	}

	DriveChase(*Car, DeltaSeconds);
}

bool ABNPoliceController::HasLineOfSightTo(const FVector& FromLoc, const FVector& ToLoc,
	const AActor* IgnoreA, const AActor* IgnoreB) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNPoliceLOS), false);
	if (IgnoreA) { Params.AddIgnoredActor(IgnoreA); }
	if (IgnoreB) { Params.AddIgnoredActor(IgnoreB); }
	return !World->LineTraceSingleByChannel(Hit, FromLoc + FVector(0.f, 0.f, 110.f),
		ToLoc + FVector(0.f, 0.f, 110.f), ECC_WorldStatic, Params);
}

void ABNPoliceController::DrivePatrol(ABNVehiclePawn& Car, float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!RoadGraph || !RoadGraph->HasGraph())
	{
		return;
	}

	const TArray<FBNRoadEdge>& Edges = RoadGraph->GetEdges();
	if (!Edges.IsValidIndex(PatrolEdgeIdx))
	{
		PatrolEdgeIdx = 0;
		bPatrolReversed = false;
		PatrolDistAlong = 0.f;
	}

	ReverseCooldownSeconds = FMath::Max(0.f, ReverseCooldownSeconds - DeltaSeconds);
	if (ReverseUnstuckSeconds > 0.f)
	{
		ReverseUnstuckSeconds = FMath::Max(0.f, ReverseUnstuckSeconds - DeltaSeconds);
		UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(1.f);
		Move->SetSteeringInput(ReverseUnstuckSteer);
		Move->SetHandbrakeInput(false);
		return;
	}

	const FVector MyLoc = Car.GetActorLocation();
	const float Speed = Car.GetChaosMovement()->GetForwardSpeed();
	StuckSeconds = (FMath::Abs(Speed) < 160.f) ? StuckSeconds + DeltaSeconds : 0.f;
	if (StuckSeconds >= ReverseTriggerSeconds && ReverseCooldownSeconds <= 0.f)
	{
		ReverseUnstuckSeconds = ReverseDurationSeconds;
		ReverseCooldownSeconds = 3.5f;
		ReverseUnstuckSteer = FMath::RandBool() ? 0.85f : -0.85f;
		StuckSeconds = 0.f;
		return;
	}

	const float EdgeLen = RoadGraph->GetEdgeLengthCm(PatrolEdgeIdx);
	// Project actual car location onto PatrolEdgeIdx when we are on it so PatrolDistAlong never drifts ahead or behind!
	FVector ProjPt, ProjTan;
	float ProjW = 1400.f, ProjDist = 0.f;
	int32 ProjEdge = INDEX_NONE;
	if (RoadGraph->FindNearestRoadPoint(MyLoc, ProjPt, ProjTan, ProjW, ProjEdge, ProjDist) && ProjEdge == PatrolEdgeIdx)
	{
		const float ActualAlong = bPatrolReversed ? (EdgeLen - ProjDist) : ProjDist;
		PatrolDistAlong = FMath::Clamp(ActualAlong, 0.f, EdgeLen);
	}

	FVector LookLoc, LookTan;
	RoadGraph->SampleEdgeAtDistance(PatrolEdgeIdx, FMath::Min(PatrolDistAlong + 1450.f, EdgeLen),
		bPatrolReversed, LookLoc, LookTan);

	// Right-hand lane offset strictly inside road half-width
	const FVector Right2D(-LookTan.Y, LookTan.X, 0.f);
	const float HalfW = Edges[PatrolEdgeIdx].WidthCm * 0.5f;
	const float LaneOff = FMath::Clamp(HalfW * 0.42f, 180.f, FMath::Max(180.f, HalfW - 180.f));
	const FVector LaneAim = LookLoc + Right2D.GetSafeNormal() * LaneOff;

	if (PatrolDistAlong >= EdgeLen - 1100.f)
	{
		FVector EndLoc, EndTan;
		RoadGraph->SampleEdgeAtDistance(PatrolEdgeIdx, EdgeLen, bPatrolReversed, EndLoc, EndTan);
		const int32 EndNodeIdx = RoadGraph->FindNearestNode(EndLoc);
		TArray<TPair<int32, bool>> OutEdges;
		RoadGraph->GetOutgoingEdges(EndNodeIdx, OutEdges);
		// Pick an outgoing paved edge that isn't an immediate U-turn
		int32 BestEdge = PatrolEdgeIdx;
		bool bBestRev = !bPatrolReversed;
		float BestScore = -10.f;
		const FVector CurFwd = EndTan.GetSafeNormal2D();
		for (const TPair<int32, bool>& Cand : OutEdges)
		{
			if (Cand.Key == PatrolEdgeIdx || Edges[Cand.Key].bDirt)
			{
				continue;
			}
			FVector DummyL, CandTan;
			if (RoadGraph->SampleEdgeAtDistance(Cand.Key, 200.f, Cand.Value, DummyL, CandTan))
			{
				const float Score = FVector::DotProduct(CurFwd, CandTan.GetSafeNormal2D()) + FMath::FRandRange(-0.25f, 0.25f);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestEdge = Cand.Key;
					bBestRev = Cand.Value;
				}
			}
		}
		PatrolEdgeIdx = BestEdge;
		bPatrolReversed = bBestRev;
		PatrolDistAlong = 200.f;
	}

	const float PatrolSpeed = (EdgeLen - PatrolDistAlong < 1800.f) ? 42.f : PatrolCruiseSpeedKmh;
	SteerTowardPoint(Car, LaneAim, PatrolSpeed, DeltaSeconds);
}

void ABNPoliceController::DriveChase(ABNVehiclePawn& Car, float DeltaSeconds)
{
	const ABNVehiclePawn* Target = TargetCar.Get();
	if (!Target)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	const FVector MyLoc = Car.GetActorLocation();
	const float Speed = Move->GetForwardSpeed();
	ReverseCooldownSeconds = FMath::Max(0.f, ReverseCooldownSeconds - DeltaSeconds);

	const bool bFlipped = Car.GetActorUpVector().Z < UpsideDownZ;
	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || bFlipped;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;

	if (ReverseUnstuckSeconds > 0.f)
	{
		ReverseUnstuckSeconds = FMath::Max(0.f, ReverseUnstuckSeconds - DeltaSeconds);
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(1.f);
		Move->SetSteeringInput(ReverseUnstuckSteer);
		Move->SetHandbrakeInput(false);
		if (bFlipped || StuckSeconds >= StuckSecondsToRecover)
		{
			ReverseUnstuckSeconds = 0.f;
			if (FVector::Dist2D(MyLoc, Target->GetActorLocation()) > MinRecoverDistCm)
			{
				RecoverStuck(Car);
			}
		}
		return;
	}

	if (StuckSeconds >= StuckSecondsToRecover || (bFlipped && StuckSeconds >= 1.6f))
	{
		if (FVector::Dist2D(MyLoc, Target->GetActorLocation()) > MinRecoverDistCm || bFlipped)
		{
			RecoverStuck(Car);
			return;
		}
		StuckSeconds = 0.f;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const float DistToTarget = FVector::Dist2D(MyLoc, TargetLoc);
	PitCooldown = FMath::Max(0.f, PitCooldown - DeltaSeconds);
	const FVector GoalPos = bSearchMode ? SearchPoint : TargetLoc;

	FVector AimPoint = GoalPos;
	float DesiredSpeedKmh = SearchCruiseSpeedKmh;

	// Use A* RoadGraph navigation whenever LOS is obstructed or distance exceeds direct visual pursuit range.
	const UBNRoadGraphSubsystem* RoadGraph = GetWorld() ? GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	const bool bClearLOS = !bSearchMode && DistToTarget <= DirectChaseMaxDistCm
		&& HasLineOfSightTo(MyLoc, TargetLoc, &Car, Target);

	bool bUsedRoadNav = false;
	if (RoadGraph && RoadGraph->HasGraph() && !bClearLOS)
	{
		NavRepathCooldown = FMath::Max(0.f, NavRepathCooldown - DeltaSeconds);
		if (NavRepathCooldown <= 0.f || !NavWaypoints.IsValidIndex(NavWaypointIndex))
		{
			RoadGraph->FindPath(MyLoc, GoalPos, NavWaypoints);
			NavWaypointIndex = 0;
			// Skip waypoints already behind or right on top of us
			while (NavWaypoints.IsValidIndex(NavWaypointIndex)
				&& FVector::Dist2D(MyLoc, NavWaypoints[NavWaypointIndex]) < WaypointReachDistCm)
			{
				++NavWaypointIndex;
			}
			NavRepathCooldown = RepathIntervalSeconds;
		}
		while (NavWaypoints.IsValidIndex(NavWaypointIndex)
			&& FVector::Dist2D(MyLoc, NavWaypoints[NavWaypointIndex]) < WaypointReachDistCm)
		{
			++NavWaypointIndex;
		}
		if (NavWaypoints.IsValidIndex(NavWaypointIndex))
		{
			AimPoint = NavWaypoints[NavWaypointIndex];
			bUsedRoadNav = true;
		}
	}

	if (!bSearchMode)
	{
		if (!bUsedRoadNav)
		{
			const FVector TargetVel = Target->GetVelocity();
			const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
			const FVector TargetRight = Target->GetActorRightVector().GetSafeNormal2D();

			float LateralOffset = 0.f;
			float LongitudinalOffset = bHeavyRhino ? 180.f : -200.f;
			if (FormationSlot % 4 == 1) { LateralOffset = -300.f; }
			else if (FormationSlot % 4 == 2) { LateralOffset = 300.f; }
			else if (FormationSlot % 4 == 3 && DistToTarget < 3500.f)
			{
				LongitudinalOffset = 440.f;
				LateralOffset = 260.f;
			}

			const float LeadSeconds = FMath::Clamp(DistToTarget / 3800.f, 0.12f, 0.55f);
			AimPoint = TargetLoc + TargetVel * LeadSeconds + TargetForward * LongitudinalOffset + TargetRight * LateralOffset;
			UpdatePit(*Target, MyLoc, DeltaSeconds, AimPoint);

			// Keep LOS pursuit aim point inside the drivable road corridor so flanking cops never steer into roadside walls!
			if (!bPitActive && DistToTarget > 700.f && RoadGraph && RoadGraph->HasGraph())
			{
				FVector RoadPt, RoadTan;
				float RoadW = 1400.f, EdgeD = 0.f;
				int32 EdgeI = INDEX_NONE;
				if (RoadGraph->FindNearestRoadPoint(AimPoint, RoadPt, RoadTan, RoadW, EdgeI, EdgeD))
				{
					const FVector Off = AimPoint - RoadPt;
					const float MaxLat = FMath::Max(RoadW * 0.5f - 190.f, 220.f);
					if (Off.Size2D() > MaxLat)
					{
						AimPoint = RoadPt + Off.GetSafeNormal2D() * MaxLat;
						AimPoint.Z = RoadPt.Z;
					}
				}
			}
		}
		else
		{
			bPitActive = false;
		}

		const float PlayerSpeedKmh = FMath::Abs(Target->GetSpeedKmh());
		const float CatchupBonus = DistToTarget > 2500.f ? 46.f : (DistToTarget > 900.f ? 24.f : 8.f);
		DesiredSpeedKmh = FMath::Clamp(PlayerSpeedKmh + CatchupBonus, MinChaseSpeedKmh, HeatSpec.MaxPursuitSpeedKmh);
		if (bPitActive)
		{
			DesiredSpeedKmh = FMath::Min(PlayerSpeedKmh + 16.f, HeatSpec.MaxPursuitSpeedKmh);
		}
		else if (DistToTarget < 650.f && HeatSpec.Aggression < 0.6f && !bHeavyRhino)
		{
			DesiredSpeedKmh = FMath::Min(DesiredSpeedKmh, PlayerSpeedKmh + 5.f);
		}

		// Slow down smoothly for BOTH current AND upcoming sharp road-graph corners (2-waypoint lookahead)
		const FVector LocalAim = Car.GetActorTransform().InverseTransformPosition(AimPoint);
		const float CurTurnDeg = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(LocalAim.Y, FMath::Max(LocalAim.X, 1.f))));
		float CornerDeg = CurTurnDeg;
		if (bUsedRoadNav && NavWaypoints.IsValidIndex(NavWaypointIndex + 1))
		{
			const FVector ToCur = (NavWaypoints[NavWaypointIndex] - MyLoc).GetSafeNormal2D();
			const FVector ToNext = (NavWaypoints[NavWaypointIndex + 1] - NavWaypoints[NavWaypointIndex]).GetSafeNormal2D();
			const float DistToCorner = FVector::Dist2D(MyLoc, NavWaypoints[NavWaypointIndex]);
			if (!ToCur.IsNearlyZero() && !ToNext.IsNearlyZero() && DistToCorner < 5500.f)
			{
				const float NextBendDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToCur, ToNext), -1.f, 1.f)));
				const float Weight = FMath::GetMappedRangeValueClamped(FVector2f(1500.f, 5500.f), FVector2f(1.f, 0.35f), DistToCorner);
				CornerDeg = FMath::Max(CornerDeg, NextBendDeg * Weight);
			}
		}
		if (CornerDeg > 24.f)
		{
			DesiredSpeedKmh = FMath::Min(DesiredSpeedKmh,
				FMath::GetMappedRangeValueClamped(FVector2f(24.f, 75.f), FVector2f(138.f, 64.f), CornerDeg));
		}

		// Trigger brief reverse-unstuck if pinned nose-first against a wall or roadblock
		if (!bFlipped && StuckSeconds >= ReverseTriggerSeconds && ReverseCooldownSeconds <= 0.f && DistToTarget > 620.f)
		{
			ReverseUnstuckSeconds = ReverseDurationSeconds;
			ReverseCooldownSeconds = 3.2f;
			ReverseUnstuckSteer = LocalAim.Y >= 0.f ? -0.9f : 0.9f;
			return;
		}
	}

	SteerTowardPoint(Car, AimPoint, DesiredSpeedKmh, DeltaSeconds);
}

void ABNPoliceController::UpdatePit(const ABNVehiclePawn& Target, const FVector& MyLoc, float DeltaSeconds, FVector& InOutAimPoint)
{
	const FTransform& TargetXf = Target.GetActorTransform();
	if (!bPitActive)
	{
		const FVector LocalMe = TargetXf.InverseTransformPosition(MyLoc);
		if (PitCooldown > 0.f || bHeavyRhino || bRoadblockUnit
			|| !BNPoliceTactics::CanAttemptPit(HeatSpec.Aggression, LocalMe, Target.GetSpeedKmh()))
		{
			return;
		}
		bPitActive = true;
		PitSide = LocalMe.Y >= 0.f ? 1.f : -1.f;
		PitTimer = BNPoliceTactics::PitAttemptSeconds;
		if (UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->NotifyPitEvent(false);
		}
	}
	PitTimer -= DeltaSeconds;
	if (PitTimer <= 0.f)
	{
		bPitActive = false;
		PitCooldown = BNPoliceTactics::PitCooldownSeconds;
		return;
	}
	InOutAimPoint = TargetXf.TransformPosition(BNPoliceTactics::PitAimLocal(PitSide));
}

void ABNPoliceController::ApplyPitContact(ABNVehiclePawn& Suspect)
{
	if (!bPitActive || bDisabled)
	{
		return;
	}
	bPitActive = false;
	PitCooldown = BNPoliceTactics::PitCooldownSeconds;
	Suspect.ApplyExternalSpin(BNPoliceTactics::PitSpinYawRate(PitSide, Suspect.GetSpeedKmh()), 0.9f);
	if (UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
	{
		Pursuit->NotifyPitEvent(true);
	}
}

float ABNPoliceController::ComputeWallAvoidance(const ABNVehiclePawn& Car, float SpeedCms, bool& bOutBlockedAhead) const
{
	bOutBlockedAhead = false;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const FVector Fwd = Car.GetActorForwardVector().GetSafeNormal();
	const FVector Right = Car.GetActorRightVector().GetSafeNormal();
	const FVector Origin = Car.GetActorLocation() + Car.GetActorUpVector() * 68.f + Fwd * 220.f;
	const float SideReach = FMath::Clamp(460.f + FMath::Abs(SpeedCms) * 0.08f, 460.f, 700.f);
	const float FrontReach = 340.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNPoliceWallAvoid), false);
	Params.AddIgnoredActor(&Car);
	if (const AActor* Target = TargetCar.Get())
	{
		Params.AddIgnoredActor(Target);
	}

	auto Probe = [&](float AngleDeg, float Dist) -> float
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Dir = (Fwd * FMath::Cos(Rad) + Right * FMath::Sin(Rad)).GetSafeNormal();
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * Dist, ECC_WorldStatic, Params)
			&& Hit.ImpactNormal.Z < 0.55f)
		{
			return Hit.Time;
		}
		return 1.f;
	};

	const float LeftHit = Probe(-30.f, SideReach);
	const float RightHit = Probe(30.f, SideReach);
	const float CenterHit = Probe(0.f, FrontReach);
	bOutBlockedAhead = (CenterHit < 0.92f);
	if (LeftHit >= 0.99f && RightHit >= 0.99f)
	{
		return 0.f;
	}
	return FMath::Clamp((RightHit - LeftHit) * 0.90f, -0.55f, 0.55f);
}

void ABNPoliceController::SteerTowardPoint(ABNVehiclePawn& Car, const FVector& AimPoint, float DesiredSpeedKmh, float DeltaSeconds) const
{
	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	const float Speed = Move->GetForwardSpeed();
	const FVector LocalAim = Car.GetActorTransform().InverseTransformPosition(AimPoint);
	const USkeletalMeshComponent* Mesh = Car.GetMesh();
	const float Wheelbase = FVector::Dist(Mesh->GetSocketLocation(TEXT("Phys_Wheel_FL")), Mesh->GetSocketLocation(TEXT("Phys_Wheel_BL")));
	const float WheelAngle = FMath::RadiansToDegrees(FMath::Atan(Wheelbase * 2.f * LocalAim.Y / FMath::Max(LocalAim.SizeSquared2D(), 1.f)));
	const FRichCurve* SteerCurve = Move->SteeringSetup.SteeringCurve.GetRichCurveConst();
	const float LockScale = SteerCurve && SteerCurve->GetNumKeys() > 0 ? SteerCurve->Eval(FMath::Abs(Speed) * CmPerSecToMph) : 1.f;
	const float MaxWheelAngle = FMath::Max(Move->Wheels[0]->MaxSteerAngle * LockScale, 1.f);

	bool bWallBlockedAhead = false;
	const float WallAvoid = ComputeWallAvoidance(Car, Speed, bWallBlockedAhead);

	float SteerInput = FMath::Clamp(WheelAngle / MaxWheelAngle + WallAvoid, -1.f, 1.f);
	if (LocalAim.X < -200.f && FMath::Abs(Speed) < 600.f)
	{
		SteerInput = LocalAim.Y >= 0.f ? 1.f : -1.f;
	}
	Move->SetSteeringInput(SteerInput);

	const float ClampedSpeedKmh = bWallBlockedAhead ? FMath::Min(DesiredSpeedKmh, 40.f) : DesiredSpeedKmh;
	const float TargetSpeedCms = ClampedSpeedKmh / CmPerSecToKmh;
	const float Error = TargetSpeedCms - Speed;
	Move->SetThrottleInput(FMath::Clamp(Error / FullThrottleError, 0.f, 1.f));
	Move->SetBrakeInput(Speed > MinBrakingSpeed ? FMath::Clamp(-Error / FullBrakeError, 0.f, 1.f) : 0.f);
	Move->SetHandbrakeInput(false);

	const float SignedHeadingErrDeg = FMath::RadiansToDegrees(FMath::Atan2(LocalAim.Y, FMath::Max(LocalAim.X, 100.f)));
	ApplyGripAssist(Car, DeltaSeconds, SignedHeadingErrDeg);
}

void ABNPoliceController::ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds, float HeadingErrorDeg) const
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Speed2D = Velocity.Size2D();
	if (!Car.IsOnGround() || Speed2D < MinGripAssistSpeed)
	{
		return;
	}
	const float HighSpeedFade = FMath::GetMappedRangeValueClamped(FVector2f(2400.f, 3600.f), FVector2f(1.f, 0.f), Speed2D);
	if (FMath::Abs(HeadingErrorDeg) > 10.f && HighSpeedFade > 0.01f)
	{
		FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();
		const float LowSpeedRamp = FMath::GetMappedRangeValueClamped(FVector2f(420.f, 1500.f), FVector2f(0.45f, 1.f), Speed2D);
		const float DesiredYawRate = FMath::Clamp(HeadingErrorDeg * YawAssistGain, -YawAssistMaxDegPerSec, YawAssistMaxDegPerSec)
			* LowSpeedRamp * HighSpeedFade;
		AngVel.Z = FMath::FInterpTo(AngVel.Z, DesiredYawRate, DeltaSeconds, 4.2f);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);
	}
	const float Slip = Car.GetSlipAngleDegrees();
	const float MaxTurn = GripAssistDegPerSec * FMath::Lerp(0.85f, 1.15f, HeatSpec.UnitSkill) * DeltaSeconds;
	const float Turn = -FMath::Clamp(Slip, -MaxTurn, MaxTurn);
	const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
}

void ABNPoliceController::RecoverStuck(ABNVehiclePawn& Car)
{
	const ABNVehiclePawn* Target = TargetCar.Get();
	if (!Target)
	{
		StuckSeconds = 0.f;
		return;
	}

	FVector SpawnLoc = FVector::ZeroVector;
	FRotator SpawnRot = FRotator::ZeroRotator;
	const UBNRoadGraphSubsystem* RoadGraph = GetWorld() ? GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	FTransform RoadTf;
	float RoadW = 1400.f;
	if (RoadGraph && RoadGraph->HasGraph()
		&& RoadGraph->FindRoadPointAhead(Target->GetActorLocation(), -Target->GetActorForwardVector().GetSafeNormal2D(), 3800.f, RoadTf, RoadW))
	{
		const FVector RoadFwd = -RoadTf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		const FVector RoadRight = RoadTf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
		const float Side = (FormationSlot % 2 == 0 ? 1.f : -1.f) * FMath::Min(320.f, RoadW * 0.22f);
		SpawnLoc = RoadTf.GetLocation() + RoadRight * Side + FVector(0.f, 0.f, 65.f);
		SpawnRot = FRotator(0.f, RoadFwd.Rotation().Yaw, 0.f);
	}
	else
	{
		const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = Target->GetActorRightVector().GetSafeNormal2D();
		const float Side = (FormationSlot % 2 == 0 ? 1.f : -1.f) * 350.f;
		SpawnLoc = Target->GetActorLocation() - Forward * 3400.f + Right * Side + FVector(0.f, 0.f, 60.f);
		SpawnRot = FRotator(0.f, Forward.Rotation().Yaw, 0.f);
	}

	Car.SetActorLocationAndRotation(SpawnLoc, SpawnRot, false, nullptr, ETeleportType::TeleportPhysics);
	Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	NavWaypoints.Reset();
	NavWaypointIndex = 0;
	NavRepathCooldown = 0.f;
	StuckSeconds = 0.f;
}
