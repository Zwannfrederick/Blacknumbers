#include "Police/BNPursuitSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPoliceHelicopter.h"
#include "Police/BNPoliceTactics.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"

namespace
{
	constexpr float SpawnChaserDistBehindCm = 3800.f;
	constexpr float SpawnChaserSideOffsetCm = 340.f;
	constexpr float SpawnRoadblockDistAheadCm = 12500.f;
	constexpr float SpawnRoadblockGapHalfCm = 440.f;
}

void UBNPursuitSubsystem::SpawnChaser(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec, int32 Slot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector SpawnLoc;
	FRotator SpawnRot;
	const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>();
	FTransform RoadTf;
	float RoadW = 1400.f;
	if (RoadGraph && RoadGraph->HasGraph()
		&& RoadGraph->FindRoadPointAhead(Player.GetActorLocation(), -Player.GetActorForwardVector().GetSafeNormal2D(), SpawnChaserDistBehindCm, RoadTf, RoadW))
	{
		const FVector RoadFwd = -RoadTf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		const FVector RoadRight = RoadTf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
		const float Side = (Slot % 2 == 0 ? 1.f : -1.f) * FMath::Min(SpawnChaserSideOffsetCm, RoadW * 0.24f);
		SpawnLoc = RoadTf.GetLocation() + RoadRight * Side + FVector(0.f, 0.f, 65.f);
		SpawnRot = FRotator(0.f, RoadFwd.Rotation().Yaw, 0.f);
	}
	else
	{
		const FVector Forward = Player.GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = Player.GetActorRightVector().GetSafeNormal2D();
		const float Side = (Slot % 2 == 0 ? 1.f : -1.f) * SpawnChaserSideOffsetCm;
		SpawnLoc = Player.GetActorLocation() - Forward * SpawnChaserDistBehindCm + Right * Side + FVector(0.f, 0.f, 60.f);
		SpawnRot = FRotator(0.f, Forward.Rotation().Yaw, 0.f);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABNVehiclePawn* Cruiser = World->SpawnActor<ABNVehiclePawn>(ABNVehiclePawn::StaticClass(), FTransform(SpawnRot, SpawnLoc), Params);
	if (!Cruiser)
	{
		return;
	}
	Cruiser->AIControllerClass = ABNPoliceController::StaticClass();
	Cruiser->SpawnDefaultController();
	ABNPoliceController* Ctrl = Cast<ABNPoliceController>(Cruiser->GetController());
	if (!Ctrl)
	{
		Cruiser->Destroy();
		return;
	}
	const bool bHeavyRhino = Spec.bRhinosEnabled && ((Spec.Level >= 5 && (Slot % 2 == 1)) || (Spec.Level < 5 && (Slot % 3 == 2)));
	Ctrl->SetupPursuitUnit(FindPlayerCar(), Spec, Slot, false, bHeavyRhino);
	Units.Add({ Cruiser, Ctrl });
}

void UBNPursuitSubsystem::SpawnRoadblock(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Forward = Player.GetActorForwardVector().GetSafeNormal2D();
	FVector Right = Player.GetActorRightVector().GetSafeNormal2D();
	FVector Center = Player.GetActorLocation() + Forward * SpawnRoadblockDistAheadCm + FVector(0.f, 0.f, 60.f);
	float RoadW = 1600.f;

	if (const UBNRoadGraphSubsystem* RoadGraph = World->GetSubsystem<UBNRoadGraphSubsystem>(); RoadGraph && RoadGraph->HasGraph())
	{
		FTransform RoadTf;
		if (RoadGraph->FindRoadPointAhead(Player.GetActorLocation(), Forward, SpawnRoadblockDistAheadCm, RoadTf, RoadW))
		{
			Forward = RoadTf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
			Right = RoadTf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
			Center = RoadTf.GetLocation() + FVector(0.f, 0.f, 65.f);
		}
	}

	const float BaseYaw = Forward.Rotation().Yaw;
	const float GapHalf = FMath::Clamp(RoadW * 0.28f, 380.f, SpawnRoadblockGapHalfCm);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const float Side : { -1.f, 1.f })
	{
		const FVector Loc = Center + Right * (Side * GapHalf);
		const FRotator Rot(0.f, BaseYaw + Side * 68.f, 0.f);
		ABNVehiclePawn* Unit = World->SpawnActor<ABNVehiclePawn>(ABNVehiclePawn::StaticClass(), FTransform(Rot, Loc), Params);
		if (!Unit)
		{
			continue;
		}
		Unit->AIControllerClass = ABNPoliceController::StaticClass();
		Unit->SpawnDefaultController();
		ABNPoliceController* Ctrl = Cast<ABNPoliceController>(Unit->GetController());
		if (!Ctrl)
		{
			Unit->Destroy();
			continue;
		}
		const bool bRhinoBlock = Spec.bRhinosEnabled && (Spec.Level >= 5);
		Ctrl->SetupPursuitUnit(&Player, Spec, 0, true, bRhinoBlock);
		Units.Add({ Unit, Ctrl });
	}

	// Heat 4+: deploy glowing Spike Strip across the left flank (right/center remains a skillful Speedbreaker gap!)
	if (Spec.bSpikesEnabled)
	{
		ClearSpikeStrips();
		const FVector StripCenter = Center - Right * (GapHalf * 0.38f) - FVector(0.f, 0.f, 52.f);
		FSpikeStripHazard Hazard;
		Hazard.Center = StripCenter;
		Hazard.RightDir = Right;
		Hazard.HalfLengthCm = GapHalf * 0.42f;
		Hazard.bTriggered = false;

		if (AStaticMeshActor* StripActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FTransform(FRotator(0.f, BaseYaw, 0.f), StripCenter), Params))
		{
			if (UStaticMeshComponent* Comp = StripActor->GetStaticMeshComponent())
			{
				Comp->SetMobility(EComponentMobility::Movable);
				if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
				{
					Comp->SetStaticMesh(Cube);
				}
				if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_SpikeStrip")))
				{
					if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_SpikeStrip.MI_SpikeStrip")))
					{
						Comp->SetMaterial(0, Mat);
					}
				}
				Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Comp->SetCastShadow(false);
				StripActor->SetActorScale3D(FVector(0.65f, (Hazard.HalfLengthCm * 2.f) / 100.f, 0.14f));
			}
			Hazard.VisualActor = StripActor;
		}
		SpikeStrips.Add(Hazard);
	}
}

bool UBNPursuitSubsystem::TickHelicopter(ABNVehiclePawn& Player, float DeltaTime)
{
	HeliCooldownSeconds = FMath::Max(0.f, HeliCooldownSeconds - DeltaTime);
	ABNPoliceHelicopter* Heli = Helicopter.Get();
	if (!Heli)
	{
		if (State.Phase != EBNPursuitPhase::Pursuit || State.HeatLevel < BNPoliceTactics::HeliMinHeat || HeliCooldownSeconds > 0.f)
		{
			return false;
		}
		const FVector Fwd = Player.GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector(-Fwd.Y, Fwd.X, 0.f);
		const FVector SpawnLoc = Player.GetActorLocation() + Fwd * 6500.f + Side * 3000.f
			+ FVector(0.f, 0.f, BNPoliceTactics::HeliHoverHeightCm + 1500.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Heli = GetWorld()->SpawnActor<ABNPoliceHelicopter>(ABNPoliceHelicopter::StaticClass(), FTransform(Player.GetActorRotation(), SpawnLoc), Params);
		if (!Heli)
		{
			return false;
		}
		Heli->SetTarget(&Player);
		Helicopter = Heli;
		HeliStationSeconds = 0.f;
		State.PostDispatch(TEXT("HAVA-1"), TEXT("Hava birimi bölgede. Şüpheliyi yukarıdan görüyorum, ışığı tutuyorum."), 4.f);
		return false;
	}
	if (Heli->IsLeaving())
	{
		return false;
	}
	HeliStationSeconds += DeltaTime;
	if (HeliStationSeconds >= BNPoliceTactics::HeliOnStationSeconds)
	{
		Heli->Leave();
		HeliCooldownSeconds = BNPoliceTactics::HeliRespawnCooldownSeconds;
		State.PostDispatch(TEXT("HAVA-1"), TEXT("Yakıt kritik, üsse dönüyorum. Yer ekipleri devralsın."), 4.f);
		return false;
	}
	return Heli->HasSightOfTarget();
}

void UBNPursuitSubsystem::DestroyHelicopter()
{
	if (ABNPoliceHelicopter* Heli = Helicopter.Get())
	{
		Heli->Destroy();
	}
	Helicopter.Reset();
	HeliStationSeconds = 0.f;
}

static FAutoConsoleCommandWithWorldAndArgs GBNPursuitStartCmd(
	TEXT("bn.Pursuit"),
	TEXT("bn.Pursuit [1-11]  Starts a Karamara police pursuit at the given Heat level."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			const int32 Heat = Args.Num() > 0 && Args[0].IsNumeric() ? FCString::Atoi(*Args[0]) : 1;
			Pursuit->StartPursuit(Heat);
		}
	}));

static FAutoConsoleCommandWithWorld GBNPursuitStopCmd(
	TEXT("bn.PursuitStop"),
	TEXT("Ends any active Karamara police pursuit."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->StopPursuit();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBNMaxHeatCmd(
	TEXT("bn.SetMaxHeat"),
	TEXT("bn.SetMaxHeat <1-11>  Sets the story Heat cap (default 5 before R4)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr; Args.Num() == 1 && Args[0].IsNumeric())
		{
			Pursuit->SetMaxAllowedHeat(FCString::Atoi(*Args[0]));
		}
	}));
