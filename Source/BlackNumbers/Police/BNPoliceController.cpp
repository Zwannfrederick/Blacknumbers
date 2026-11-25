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
#include "Misc/PackageName.h"
#include "Police/BNPoliceSirenComponent.h"
#include "Police/BNPoliceTactics.h"
#include "Police/BNPursuitSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float CmPerSecToMph = 0.022369f;
	constexpr float MinChaseSpeedKmh = 68.f;
	constexpr float SearchCruiseSpeedKmh = 95.f;
	constexpr float PatrolCruiseSpeedKmh = 58.f;
	constexpr float FullThrottleError = 300.f;
	constexpr float FullBrakeError = 550.f;
	constexpr float MinBrakingSpeed = 200.f;
	constexpr float GripAssistDegPerSec = 145.f;
	constexpr float MinGripAssistSpeed = 450.f;
	constexpr float StuckSpeed = 220.f;
	constexpr float UpsideDownZ = 0.35f;
	constexpr float StuckSecondsToRecover = 3.8f;
	constexpr float MinRecoverDistCm = 2400.f;
	constexpr float YelpDistanceCm = 3000.f;
	constexpr float DirectChaseMaxDistCm = 4600.f;
	constexpr float WaypointReachDistCm = 1450.f;
	constexpr float RepathIntervalSeconds = 1.15f;

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
	NavWaypoints.Reset();
	NavWaypointIndex = 0;
	NavRepathCooldown = 0.f;

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		if (bHeavyRhino)
		{
			const int32 RhinoTiers[FBNUpgradeSpec::NumSlots] = { 4, 4, 4, 4, 3 };
			FBNComputedVehicleStats Stats = FBNComputedVehicleStats::Compute(EBNCarId::TauronV6Rally, RhinoTiers);
			Car->ApplyVehicleSpec(EBNCarId::TauronV6Rally, Stats);
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

	const float RoofZ = bHeavyRhino ? 174.f : 123.f;
	UStaticMesh* BarMesh = FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_PoliceLightbar"))
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_PoliceLightbar.SM_BN_PoliceLightbar"))
		: nullptr;
	if (BarMesh || CubeMesh)
	{
		LightbarMesh = NewObject<UStaticMeshComponent>(&Car, TEXT("PoliceLightbar"));
		LightbarMesh->SetStaticMesh(BarMesh ? BarMesh : CubeMesh.Get());
		LightbarMesh->SetupAttachment(Body);
		LightbarMesh->SetRelativeLocation(FVector(-12.f, 0.f, RoofZ));
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
			PushBarMesh->SetRelativeLocation(FVector(bHeavyRhino ? 248.f : 238.f, 0.f, bHeavyRhino ? 52.f : 44.f));
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
	Car.ApplyPaintTint(bHeavyRhino ? FLinearColor(0.025f, 0.028f, 0.032f, 1.f) : FLinearColor(0.03f, 0.045f, 0.08f, 1.f));
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

	const float EdgeLen = RoadGraph->GetEdgeLengthCm(PatrolEdgeIdx);
	const FVector MyLoc = Car.GetActorLocation();
	FVector LookLoc, LookTan;
	RoadGraph->SampleEdgeAtDistance(PatrolEdgeIdx, FMath::Min(PatrolDistAlong + 1500.f, EdgeLen),
		bPatrolReversed, LookLoc, LookTan);

	// Right-hand lane offset
	const FVector Right2D(-LookTan.Y, LookTan.X, 0.f);
	const FVector LaneAim = LookLoc + Right2D.GetSafeNormal() * (Edges[PatrolEdgeIdx].WidthCm * 0.24f);

	if (FVector::Dist2D(MyLoc, LaneAim) < 1600.f)
	{
		PatrolDistAlong += 1200.f;
	}

	if (PatrolDistAlong >= EdgeLen - 800.f)
	{
		const FBNRoadEdge& CurEdge = Edges[PatrolEdgeIdx];
		const int32 EndNodeId = bPatrolReversed ? CurEdge.NodeA : CurEdge.NodeB;
		const int32 EndNodeIdx = RoadGraph->FindNearestNode(MyLoc);
		TArray<TPair<int32, bool>> OutEdges;
		RoadGraph->GetOutgoingEdges(EndNodeIdx, OutEdges);
		// Pick an outgoing paved edge that isn't an immediate U-turn
		int32 BestEdge = PatrolEdgeIdx;
		bool bBestRev = !bPatrolReversed;
		float BestScore = -10.f;
		const FVector CurFwd = Car.GetActorForwardVector().GetSafeNormal2D();
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
		(void)EndNodeId;
		PatrolEdgeIdx = BestEdge;
		bPatrolReversed = bBestRev;
		PatrolDistAlong = 300.f;
	}

	SteerTowardPoint(Car, LaneAim, PatrolCruiseSpeedKmh, DeltaSeconds);
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

	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || Car.GetActorUpVector().Z < UpsideDownZ;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;
	if (StuckSeconds >= StuckSecondsToRecover)
	{
		if (FVector::Dist2D(MyLoc, Target->GetActorLocation()) > MinRecoverDistCm)
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
			if (FormationSlot % 4 == 1) { LateralOffset = -340.f; }
			else if (FormationSlot % 4 == 2) { LateralOffset = 340.f; }
			else if (FormationSlot % 4 == 3 && DistToTarget < 3500.f)
			{
				LongitudinalOffset = 480.f;
				LateralOffset = 280.f;
			}

			const float LeadSeconds = FMath::Clamp(DistToTarget / 3500.f, 0.15f, 0.7f);
			AimPoint = TargetLoc + TargetVel * LeadSeconds + TargetForward * LongitudinalOffset + TargetRight * LateralOffset;
			UpdatePit(*Target, MyLoc, DeltaSeconds, AimPoint);
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
		// Slow down cleanly for sharp urban road-graph turns so units hold the corner instead of hitting buildings
		if (bUsedRoadNav)
		{
			const FVector LocalAim = Car.GetActorTransform().InverseTransformPosition(AimPoint);
			const float TurnAngleDeg = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(LocalAim.Y, FMath::Max(LocalAim.X, 1.f))));
			if (TurnAngleDeg > 28.f)
			{
				DesiredSpeedKmh = FMath::Min(DesiredSpeedKmh, FMath::GetMappedRangeValueClamped(FVector2f(28.f, 75.f), FVector2f(135.f, 68.f), TurnAngleDeg));
			}
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

	float SteerInput = FMath::Clamp(WheelAngle / MaxWheelAngle, -1.f, 1.f);
	if (LocalAim.X < -200.f && FMath::Abs(Speed) < 600.f)
	{
		SteerInput = LocalAim.Y >= 0.f ? 1.f : -1.f;
	}
	Move->SetSteeringInput(SteerInput);

	const float TargetSpeedCms = DesiredSpeedKmh / CmPerSecToKmh;
	const float Error = TargetSpeedCms - Speed;
	Move->SetThrottleInput(FMath::Clamp(Error / FullThrottleError, 0.f, 1.f));
	Move->SetBrakeInput(Speed > MinBrakingSpeed ? FMath::Clamp(-Error / FullBrakeError, 0.f, 1.f) : 0.f);
	Move->SetHandbrakeInput(false);

	ApplyGripAssist(Car, DeltaSeconds);
}

void ABNPoliceController::ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds) const
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (!Car.IsOnGround() || Velocity.Size2D() < MinGripAssistSpeed)
	{
		return;
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
