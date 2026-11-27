#include "Police/BNPursuitSubsystem.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPoliceHelicopter.h"
#include "Police/BNPoliceTactics.h"
#include "Police/BNPursuitBreaker.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehiclePawn.h"
#include "World/BNRoadGraph.h"

namespace
{
	constexpr float ChaserSpawnIntervalSeconds = 2.5f;
	constexpr float ChaserSpawnDistanceBehindCm = 3800.f;
	constexpr float ChaserSideOffsetCm = 340.f;

	constexpr float RoadblockIntervalSeconds = 18.f;
	constexpr float RoadblockDistanceAheadCm = 12500.f;
	constexpr float RoadblockDespawnDistCm = 18500.f;
	constexpr float RoadblockPassedDistCm = 4200.f;
	constexpr float RoadblockGapHalfWidthCm = 440.f;

	constexpr int32 TargetFreeRoamPatrols = 3;
	constexpr float PatrolTriggerDistCm = 2200.f;
	constexpr float PatrolTriggerSpeedKmh = 130.f;
	constexpr float PatrolRamTriggerDistCm = 520.f;
	constexpr float PatrolRecycleMaxDistCm = 62000.f;

	constexpr float SpikeLimpDurationSeconds = 7.0f;
	constexpr float SpikeLimpMaxSpeedCms = 2050.f; // ~74 km/h shredded-tire limp mode (MW 2005 feel!)
	constexpr float SearchPatrolRadiusCm = 11000.f;

	void DestroyUnit(TWeakObjectPtr<ABNVehiclePawn>& CarPtr, TWeakObjectPtr<ABNPoliceController>& CtrlPtr)
	{
		if (ABNPoliceController* Ctrl = CtrlPtr.Get())
		{
			Ctrl->Destroy();
		}
		if (ABNVehiclePawn* Car = CarPtr.Get())
		{
			Car->Destroy();
		}
	}

	bool IsPlayerInActiveRace(const UWorld* World, const ABNVehiclePawn* Player)
	{
		if (!World || !Player)
		{
			return false;
		}
		for (TActorIterator<ABNRaceEvent> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (It->IsRunningFor(Player))
			{
				return true;
			}
		}
		return false;
	}
}

bool UBNPursuitSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UBNPursuitSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBNPursuitSubsystem, STATGROUP_Tickables);
}

void UBNPursuitSubsystem::Deinitialize()
{
	ClearAllUnits();
	ClearPatrols();
	ClearSpikeStrips();
	Super::Deinitialize();
}

void UBNPursuitSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UBNStorySubsystem* Story = InWorld.GetGameInstance() ? InWorld.GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		Story->SyncWorldSystems(&InWorld);
	}
}

ABNVehiclePawn* UBNPursuitSubsystem::FindPlayerCar() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
}

void UBNPursuitSubsystem::SetFreeRoamPatrolsEnabled(bool bEnabled)
{
	bFreeRoamPatrolsEnabled = bEnabled;
	if (!bFreeRoamPatrolsEnabled)
	{
		ClearPatrols();
	}
}

void UBNPursuitSubsystem::StartPursuit(int32 InitialHeat)
{
	ClearAllUnits();
	ClearPatrols();
	ClearSpikeStrips();
	State.StartPursuit(InitialHeat);
	SpawnCooldown = 0.f;
	RoadblockCooldown = 8.f;
	SpikePenaltySeconds = 0.f;
	NextSlot = 0;

	if (ABNVehiclePawn* Player = FindPlayerCar())
	{
		const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(State.HeatLevel);
		SpawnChaser(*Player, Spec, NextSlot++);
		SpawnCooldown = ChaserSpawnIntervalSeconds;
	}
}

void UBNPursuitSubsystem::EscalateOrStartPursuit()
{
	if (!State.IsPursuitRunning())
	{
		StartPursuit(1);
		return;
	}
	if (State.HeatLevel < State.MaxAllowedHeatLevel)
	{
		++State.HeatLevel;
		const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(State.HeatLevel);
		State.PostDispatch(Spec.bDemirInvolved ? TEXT("BAŞKOMİSER DEMİR") : TEXT("MERKEZ"),
			FString::Printf(TEXT("Takip %s seviyesine yükseltildi!"), *Spec.Label));
	}
	else
	{
		StopPursuit();
	}
}

void UBNPursuitSubsystem::StopPursuit()
{
	bInRaceIntervention = false;
	State.EndPursuitImmediate();
	SpikePenaltySeconds = 0.f;
	ClearAllUnits();
	ClearSpikeStrips();
}

void UBNPursuitSubsystem::StartInRacePursuit(int32 InitialHeat)
{
	ABNVehiclePawn* Player = FindPlayerCar();
	if (!Player)
	{
		return;
	}
	bInRaceIntervention = true;
	if (!State.IsPursuitRunning())
	{
		ClearPatrols();
		ClearSpikeStrips();
		State.StartPursuit(FMath::Clamp(InitialHeat, 1, FMath::Min(State.MaxAllowedHeatLevel, 4)));
		State.PostDispatch(TEXT("MERKEZ"),
			TEXT("YASA DIŞI SOKAK YARIŞI TESPİT EDİLDİ! Devriyeler yarışa müdahale ediyor!"), 4.2f);
		SpawnCooldown = 0.5f;
		RoadblockCooldown = 14.f;
		NextSlot = 0;
		const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(State.HeatLevel);
		SpawnChaser(*Player, Spec, NextSlot++);
		if (Spec.MaxActiveUnits >= 2)
		{
			SpawnChaser(*Player, Spec, NextSlot++);
		}
	}
}

void UBNPursuitSubsystem::NotifyRaceEndedKeepPursuit()
{
	if (bInRaceIntervention && State.IsPursuitRunning())
	{
		bInRaceIntervention = false;
		State.PostDispatch(TEXT("MERKEZ"),
			TEXT("Yarış bitti ama şüpheli hâlâ kaçıyor! Tüm birimler takibi sürdürün!"), 3.8f);
	}
	else
	{
		bInRaceIntervention = false;
	}
}

void UBNPursuitSubsystem::SetMaxAllowedHeat(int32 MaxHeat)
{
	State.SetMaxAllowedHeat(MaxHeat);
}

void UBNPursuitSubsystem::ReportCollateralDamage(int32 Dollars, const FString& Reason)
{
	State.RecordCollateralDamage(Dollars, Reason.IsEmpty() ? nullptr : *Reason);
}

void UBNPursuitSubsystem::OnPursuitBreakerTriggered(ABNPursuitBreaker* Breaker, ABNVehiclePawn* PlayerCar)
{
	const ABNVehiclePawn* ActivePlayer = PlayerCar ? PlayerCar : FindPlayerCar();
	if (!Breaker || !State.IsPursuitRunning() || (!bInRaceIntervention && IsPlayerInActiveRace(GetWorld(), ActivePlayer)))
	{
		return;
	}

	const FVector Origin = Breaker->GetActorLocation();
	const float RadiusSq = FMath::Square(Breaker->DisableRadiusCm);
	int32 DisabledCount = 0;

	for (const FPoliceUnit& Unit : Units)
	{
		ABNVehiclePawn* Car = Unit.Car.Get();
		ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (!Car || !Ctrl || Ctrl->IsUnitDisabled())
		{
			continue;
		}
		if (FVector::DistSquared2D(Car->GetActorLocation(), Origin) <= RadiusSq)
		{
			Ctrl->DisableUnit();
			++DisabledCount;
		}
	}

	if (DisabledCount > 0)
	{
		TakedownBannerSeconds = 2.2f;
	}
	State.RecordPursuitBreaker(Breaker->CollateralCostDollars, DisabledCount);
	UE_LOG(LogTemp, Display, TEXT("BN PursuitBreaker '%s': cost +$%d (total $%d), disabled %d units"),
		*Breaker->StructureName, Breaker->CollateralCostDollars, State.CostToStateDollars, DisabledCount);
}

void UBNPursuitSubsystem::DisablePoliceNear(const FVector& Origin, float RadiusCm)
{
	if (!State.IsPursuitRunning())
	{
		return;
	}
	const float RadiusSq = FMath::Square(RadiusCm);
	for (const FPoliceUnit& Unit : Units)
	{
		ABNVehiclePawn* Car = Unit.Car.Get();
		ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (!Car || !Ctrl || Ctrl->IsUnitDisabled())
		{
			continue;
		}
		if (FVector::DistSquared2D(Car->GetActorLocation(), Origin) <= RadiusSq)
		{
			Ctrl->DisableUnit();
			State.RecordUnitDisabled();
			TakedownBannerSeconds = 2.2f;
		}
	}
}

void UBNPursuitSubsystem::NotifyPitEvent(bool bSuccess)
{
	if (!State.IsPursuitRunning())
	{
		return;
	}
	if (bSuccess)
	{
		State.PostDispatch(TEXT("BİRİM-05"), TEXT("PIT tuttu! Şüpheli savruldu, etrafını sarın!"), 3.5f);
	}
	else if (State.ActiveRadio.RemainingSeconds <= 0.5f)
	{
		State.PostDispatch(TEXT("BİRİM-05"), TEXT("Arka çeyreğine girdim, PIT deniyorum!"), 2.5f);
	}
}

void UBNPursuitSubsystem::OnPlayerRammedPolice(ABNVehiclePawn* PoliceCar, float PlayerSpeedKmh)
{
	if (!PoliceCar || !State.IsPursuitRunning())
	{
		return;
	}
	for (const FPoliceUnit& Unit : Units)
	{
		if (Unit.Car.Get() != PoliceCar)
		{
			continue;
		}
		ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (!Ctrl || Ctrl->IsUnitDisabled())
		{
			return;
		}
		// High-speed ram disables cruisers cleanly (>=105 km/h); Heavy Rhinos require an ultra-hard >=155 km/h slam!
		const float MinRamSpeed = Ctrl->IsHeavyRhino() ? 155.f : 105.f;
		if (PlayerSpeedKmh >= MinRamSpeed)
		{
			Ctrl->DisableUnit();
			State.RecordUnitDisabled(Ctrl->IsHeavyRhino() ? 25000 : 15000);
			State.RecordCollateralDamage(Ctrl->IsHeavyRhino() ? 12000 : 5000,
				Ctrl->IsHeavyRhino() ? TEXT("Ağır Rhino Devre Dışı") : TEXT("Polis Aracı Devre Dışı"));
			State.PostDispatch(TEXT("MERKEZ"),
				Ctrl->IsHeavyRhino() ? TEXT("Ağır Rhino birimi saf dışı! Destek ekip yönlendirin!")
				                     : TEXT("Birim çarpışmada devre dışı kaldı! Takviye yönlendiriliyor!"));
			TakedownBannerSeconds = 2.4f;
		}
		else
		{
			State.RecordCollateralDamage(3000, TEXT("Polis Aracı Hasarı"));
		}
		return;
	}
}

bool UBNPursuitSubsystem::GetNearestRoadblockAhead(const ABNVehiclePawn& Player, float& OutDistanceMeters,
	bool& bOutHasSpikes, FVector* OutWorldLoc) const
{
	if (!State.IsPursuitRunning())
	{
		return false;
	}
	const FVector PlayerLoc = Player.GetActorLocation();
	const FVector PlayerFwd = Player.GetActorForwardVector().GetSafeNormal2D();
	FVector SumLoc = FVector::ZeroVector;
	int32 Count = 0;
	float BestAlong = 24000.f;

	for (const FPoliceUnit& Unit : Units)
	{
		const ABNVehiclePawn* Car = Unit.Car.Get();
		const ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (!Car || !Ctrl || !Ctrl->IsRoadblockUnit() || Ctrl->IsUnitDisabled())
		{
			continue;
		}
		const FVector ToCar = Car->GetActorLocation() - PlayerLoc;
		const float Along = FVector::DotProduct(ToCar, PlayerFwd);
		const float Dist2D = ToCar.Size2D();
		if (Along > 400.f && Dist2D <= 24000.f)
		{
			SumLoc += Car->GetActorLocation();
			++Count;
			BestAlong = FMath::Min(BestAlong, Dist2D);
		}
	}
	if (Count == 0)
	{
		return false;
	}
	const FVector Center = SumLoc / static_cast<float>(Count);
	OutDistanceMeters = BestAlong / 100.f;
	bOutHasSpikes = SpikeStrips.Num() > 0;
	if (OutWorldLoc)
	{
		*OutWorldLoc = Center;
	}
	return true;
}

int32 UBNPursuitSubsystem::GetActivePoliceCount() const
{
	int32 Count = 0;
	for (const FPoliceUnit& Unit : Units)
	{
		const ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (Unit.Car.IsValid() && Ctrl && !Ctrl->IsUnitDisabled())
		{
			++Count;
		}
	}
	return Count;
}

int32 UBNPursuitSubsystem::GetActivePatrolCount() const
{
	int32 Count = 0;
	for (const FPoliceUnit& Unit : PatrolUnits)
	{
		if (Unit.Car.IsValid() && Unit.Controller.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void UBNPursuitSubsystem::EnsureFreeRoamPatrols(const FVector& PlayerLoc)
{
	UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!World || !RoadGraph || !RoadGraph->HasGraph())
	{
		return;
	}

	// Prune dead or very distant patrols
	for (int32 i = PatrolUnits.Num() - 1; i >= 0; --i)
	{
		ABNVehiclePawn* Car = PatrolUnits[i].Car.Get();
		if (!Car || !PatrolUnits[i].Controller.IsValid()
			|| FVector::Dist2D(Car->GetActorLocation(), PlayerLoc) > PatrolRecycleMaxDistCm)
		{
			DestroyUnit(PatrolUnits[i].Car, PatrolUnits[i].Controller);
			PatrolUnits.RemoveAtSwap(i);
		}
	}

	if (PatrolUnits.Num() >= TargetFreeRoamPatrols)
	{
		return;
	}

	const TArray<FBNRoadEdge>& Edges = RoadGraph->GetEdges();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Attempt = 0; Attempt < 24 && PatrolUnits.Num() < TargetFreeRoamPatrols; ++Attempt)
	{
		const int32 EdgeIdx = FMath::RandRange(0, Edges.Num() - 1);
		if (Edges[EdgeIdx].bDirt)
		{
			continue;
		}
		const float EdgeLen = RoadGraph->GetEdgeLengthCm(EdgeIdx);
		if (EdgeLen < 1200.f)
		{
			continue;
		}
		FVector Loc, Tan;
		const bool bRev = FMath::RandBool();
		RoadGraph->SampleEdgeAtDistance(EdgeIdx, EdgeLen * 0.5f, bRev, Loc, Tan);
		const float Dist = FVector::Dist2D(Loc, PlayerLoc);
		if (Dist < 12000.f || Dist > 44000.f)
		{
			continue;
		}

		const FVector Right2D(-Tan.Y, Tan.X, 0.f);
		const FVector SpawnLoc = Loc + Right2D.GetSafeNormal() * (Edges[EdgeIdx].WidthCm * 0.24f) + FVector(0.f, 0.f, 65.f);
		ABNVehiclePawn* Cruiser = World->SpawnActor<ABNVehiclePawn>(ABNVehiclePawn::StaticClass(),
			FTransform(FRotator(0.f, Tan.Rotation().Yaw, 0.f), SpawnLoc), Params);
		if (!Cruiser)
		{
			continue;
		}
		Cruiser->AIControllerClass = ABNPoliceController::StaticClass();
		Cruiser->SpawnDefaultController();
		ABNPoliceController* Ctrl = Cast<ABNPoliceController>(Cruiser->GetController());
		if (!Ctrl)
		{
			Cruiser->Destroy();
			continue;
		}
		Ctrl->SetupPatrolUnit(EdgeIdx, bRev);
		PatrolUnits.Add({ Cruiser, Ctrl });
	}
}

void UBNPursuitSubsystem::TickFreeRoamPatrols(ABNVehiclePawn* Player, float DeltaTime)
{
	UWorld* World = GetWorld();
	const UBNRoadGraphSubsystem* RoadGraph = World ? World->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr;
	if (!bFreeRoamPatrolsEnabled || !Player || !RoadGraph || !RoadGraph->HasGraph()
		|| IsPlayerInActiveRace(World, Player) || Player->IsInputLocked())
	{
		if (IsPlayerInActiveRace(World, Player) && PatrolUnits.Num() > 0)
		{
			ClearPatrols();
		}
		return;
	}

	PatrolSpawnCooldown = FMath::Max(0.f, PatrolSpawnCooldown - DeltaTime);
	if (PatrolSpawnCooldown <= 0.f)
	{
		EnsureFreeRoamPatrols(Player->GetActorLocation());
		PatrolSpawnCooldown = 4.0f;
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	const float PlayerSpeedKmh = FMath::Abs(Player->GetSpeedKmh());

	for (int32 i = 0; i < PatrolUnits.Num(); ++i)
	{
		ABNVehiclePawn* CopCar = PatrolUnits[i].Car.Get();
		ABNPoliceController* CopCtrl = PatrolUnits[i].Controller.Get();
		if (!CopCar || !CopCtrl)
		{
			continue;
		}
		const float Dist = FVector::Dist2D(PlayerLoc, CopCar->GetActorLocation());
		const bool bHeightMatch = FMath::Abs(PlayerLoc.Z - CopCar->GetActorLocation().Z) < 450.f;
		const bool bSpeedingPast = bHeightMatch && Dist <= PatrolTriggerDistCm && PlayerSpeedKmh >= PatrolTriggerSpeedKmh;
		const bool bRammed = bHeightMatch && Dist <= PatrolRamTriggerDistCm && PlayerSpeedKmh >= 25.f;
		if (bSpeedingPast || bRammed)
		{
			// Promote this patrol organically into live Heat 1 pursuit!
			State.StartPursuit(1);
			State.PostDispatch(TEXT("DEVRİYE-02"),
				bRammed ? TEXT("Devriye aracına çarpma! K. Vance takibe alındı!")
				        : TEXT("Aşırı hız ihlali tespit edildi! K. Vance takibe alındı!"));
			SpawnCooldown = ChaserSpawnIntervalSeconds;
			RoadblockCooldown = 10.f;
			NextSlot = 0;

			const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(State.HeatLevel);
			CopCtrl->PromotePatrolToPursuit(Player, Spec, NextSlot++);
			Units.Add(PatrolUnits[i]);
			PatrolUnits.RemoveAtSwap(i);
			ClearPatrols();
			return;
		}
	}
}

void UBNPursuitSubsystem::TickSpikeStrips(ABNVehiclePawn& Player, float DeltaTime)
{
	if (SpikePenaltySeconds > 0.f)
	{
		SpikePenaltySeconds = FMath::Max(0.f, SpikePenaltySeconds - DeltaTime);
		if (USkeletalMeshComponent* Mesh = Player.GetMesh(); Mesh && Player.IsOnGround())
		{
			const FVector Vel = Mesh->GetPhysicsLinearVelocity();
			if (Vel.Size2D() > SpikeLimpMaxSpeedCms)
			{
				const FVector Clamped2D = FVector(Vel.X, Vel.Y, 0.f).GetSafeNormal()
					* FMath::FInterpTo(Vel.Size2D(), SpikeLimpMaxSpeedCms, DeltaTime, 3.8f);
				Mesh->SetPhysicsLinearVelocity(FVector(Clamped2D.X, Clamped2D.Y, Vel.Z));
			}
		}
	}

	const FVector PlayerLoc = Player.GetActorLocation();
	for (FSpikeStripHazard& Strip : SpikeStrips)
	{
		if (Strip.bTriggered)
		{
			continue;
		}
		const FVector Delta = PlayerLoc - Strip.Center;
		if (FMath::Abs(Delta.Z) > 200.f)
		{
			continue;
		}
		const float Lat = FMath::Abs(FVector::DotProduct(Delta, Strip.RightDir));
		const float Lon = FMath::Sqrt(FMath::Max(0.f, Delta.SizeSquared2D() - Lat * Lat));
		if (Lat <= Strip.HalfLengthCm && Lon <= 220.f)
		{
			Strip.bTriggered = true;
			SpikePenaltySeconds = SpikeLimpDurationSeconds;
			State.PostDispatch(TEXT("MERKEZ"), TEXT("Şüpheli dikenli şeride bastı! Lastikler hasarlı, sıkıştırın!"), 4.0f);
			if (UBNVehicleEffectsComponent* Fx = Player.FindComponentByClass<UBNVehicleEffectsComponent>())
			{
				Fx->TriggerCollisionSparks(PlayerLoc + FVector(0.f, 0.f, 25.f), FVector::UpVector, 1.2f);
			}
			if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
			{
				Audio->PlaySfxAtLocation(EBNSfxId::CrashLight, PlayerLoc, 0.9f, 1.35f);
			}
		}
	}
}

void UBNPursuitSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State.DispatchSerial != LastDispatchSerial)
	{
		LastDispatchSerial = State.DispatchSerial;
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::RadioSquelch, 0.85f);
			// Garbled scanner voice under the text line; pitch varies per speaker so units sound different.
			Audio->PlaySfx2D(EBNSfxId::RadioChatter, 0.5f, 0.88f + 0.06f * static_cast<float>(State.DispatchSerial % 5));
		}
	}

	ABNVehiclePawn* Player = FindPlayerCar();

	if (State.Phase == EBNPursuitPhase::None)
	{
		if (Units.Num() > 0) { ClearAllUnits(); }
		if (SpikeStrips.Num() > 0) { ClearSpikeStrips(); }
		TickFreeRoamPatrols(Player, DeltaTime);
		return;
	}

	if (!State.IsPursuitRunning())
	{
		if (ABNPoliceHelicopter* Heli = Helicopter.Get(); Heli && !Heli->IsLeaving())
		{
			Heli->Leave(); // escaped/busted: fly off instead of popping out of the sky
		}
		State.Update(DeltaTime, 0.f, BIG_NUMBER, 0, false);
		if (State.Phase == EBNPursuitPhase::None)
		{
			ClearAllUnits();
			ClearSpikeStrips();
		}
		return;
	}

	if (!Player || (!bInRaceIntervention && IsPlayerInActiveRace(GetWorld(), Player)))
	{
		StopPursuit();
		return;
	}

	PruneUnits();
	TakedownBannerSeconds = FMath::Max(0.f, TakedownBannerSeconds - DeltaTime);
	TickSpikeStrips(*Player, DeltaTime);

	const FVector PlayerLoc = Player->GetActorLocation();
	float NearestDist = BIG_NUMBER;
	int32 ActiveChasers = 0;
	int32 ActiveRoadblocks = 0;
	int32 TotalActive = 0;

	for (const FPoliceUnit& Unit : Units)
	{
		const ABNVehiclePawn* Car = Unit.Car.Get();
		const ABNPoliceController* Ctrl = Unit.Controller.Get();
		if (!Car || !Ctrl || Ctrl->IsUnitDisabled())
		{
			continue;
		}
		++TotalActive;
		const float Dist = FVector::Dist2D(PlayerLoc, Car->GetActorLocation());
		NearestDist = FMath::Min(NearestDist, Dist);
		if (Ctrl->IsRoadblockUnit()) { ++ActiveRoadblocks; }
		else { ++ActiveChasers; }
	}

	bool bInCooldownSpot = false;
	FBNCooldownSpot ActiveSpot;
	if (const UBNRoadGraphSubsystem* RoadGraph = GetWorld() ? GetWorld()->GetSubsystem<UBNRoadGraphSubsystem>() : nullptr)
	{
		bInCooldownSpot = RoadGraph->IsInsideCooldownSpot(PlayerLoc, &ActiveSpot);
		State.ActiveCooldownSpotName = bInCooldownSpot ? ActiveSpot.DisplayName : FString();
	}

	const int32 PrevHeat = State.HeatLevel;
	const EBNPursuitPhase PrevPhase = State.Phase;
	const bool bAirSight = TickHelicopter(*Player, DeltaTime);
	State.Update(DeltaTime, Player->GetSpeedKmh(), NearestDist, TotalActive, bInCooldownSpot, bAirSight);

	UBNProgressionSubsystem* Prog = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	UBNStorySubsystem* Story = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;

	if (PrevPhase != EBNPursuitPhase::Escaped && State.Phase == EBNPursuitPhase::Escaped)
	{
		int32 NewMilestones = 0, BonusBounty = 0;
		if (Story)
		{
			Story->OnPursuitEscaped(State, NewMilestones, BonusBounty);
		}
		if (Prog)
		{
			Prog->GrantPursuitEscapeReward(State.Bounty + BonusBounty, State.HeatLevel);
		}
	}
	else if (PrevPhase != EBNPursuitPhase::Busted && State.Phase == EBNPursuitPhase::Busted)
	{
		if (Story)
		{
			int32 FinePaid = 0;
			bool bImpoundReset = false;
			Story->OnPursuitBusted(State.HeatLevel, Prog, FinePaid, bImpoundReset);
		}
	}
	else if (Story && State.IsPursuitRunning())
	{
		FString NewTitle;
		const int32 LiveMask = Story->EvaluateLivePursuitMilestones(State, NewTitle);
		if (LiveMask != State.PendingMilestonesMask)
		{
			State.PendingMilestonesMask = LiveMask;
			if (!NewTitle.IsEmpty())
			{
				State.MilestoneBannerText = FString::Printf(TEXT("DÖNÜM NOKTASI HAZIR (KAÇ & ONAYLA): %s"), *NewTitle);
				State.MilestoneBannerSeconds = 4.0f;
			}
		}
	}

	if (Story && Story->GetState().bFinalChaseActive && Story->CheckFinalChaseEscape(Player))
	{
		return;
	}

	if (!State.IsPursuitRunning())
	{
		return;
	}

	const FBNHeatTierSpec Spec = FBNHeatTierSpec::ForLevel(State.HeatLevel);
	const bool bCooldownMode = State.Phase == EBNPursuitPhase::Cooldown;
	const FVector PlayerFwd = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector PlayerRight = Player->GetActorRightVector().GetSafeNormal2D();
	const FVector SearchTarget = PlayerLoc - PlayerFwd * SearchPatrolRadiusCm + PlayerRight * 3500.f;

	for (const FPoliceUnit& Unit : Units)
	{
		if (ABNPoliceController* Ctrl = Unit.Controller.Get())
		{
			if (State.HeatLevel != PrevHeat)
			{
				Ctrl->UpdateHeatSpec(Spec);
			}
			Ctrl->SetSearchMode(bCooldownMode, SearchTarget);
		}
	}

	// Clean out old roadblocks once passed or far behind; driving past one counts on the rap sheet.
	bool bRoadblockPassed = false;
	for (int32 i = Units.Num() - 1; i >= 0; --i)
	{
		ABNVehiclePawn* Car = Units[i].Car.Get();
		ABNPoliceController* Ctrl = Units[i].Controller.Get();
		if (Car && Ctrl && Ctrl->IsRoadblockUnit())
		{
			const FVector ToBlock = Car->GetActorLocation() - PlayerLoc;
			const float Along = FVector::DotProduct(ToBlock, PlayerFwd);
			if (ToBlock.Size2D() > RoadblockDespawnDistCm || Along < -RoadblockPassedDistCm)
			{
				bRoadblockPassed |= Along < -RoadblockPassedDistCm;
				DestroyUnit(Units[i].Car, Units[i].Controller);
				Units.RemoveAtSwap(i);
				--ActiveRoadblocks;
			}
		}
	}
	if (bRoadblockPassed && ActiveRoadblocks == 0 && State.IsPursuitRunning()) // both units of the block gone -> count once
	{
		State.RecordRoadblockDodged();
		if (SpikeStrips.ContainsByPredicate([](const FSpikeStripHazard& Strip) { return !Strip.bTriggered; }))
		{
			State.RecordSpikeStripDodged();
		}
	}
	if (ActiveRoadblocks == 0 && SpikeStrips.Num() > 0)
	{
		ClearSpikeStrips();
	}

	if (State.Phase == EBNPursuitPhase::Pursuit)
	{
		SpawnCooldown = FMath::Max(0.f, SpawnCooldown - DeltaTime);
		if (ActiveChasers < Spec.MaxActiveUnits && SpawnCooldown <= 0.f)
		{
			SpawnChaser(*Player, Spec, NextSlot++);
			SpawnCooldown = ChaserSpawnIntervalSeconds;
		}

		if (Spec.bRoadblocksEnabled)
		{
			RoadblockCooldown = FMath::Max(0.f, RoadblockCooldown - DeltaTime);
			if (ActiveRoadblocks == 0 && RoadblockCooldown <= 0.f)
			{
				SpawnRoadblock(*Player, Spec);
				RoadblockCooldown = RoadblockIntervalSeconds;
			}
		}
	}
}

void UBNPursuitSubsystem::PruneUnits()
{
	for (int32 i = Units.Num() - 1; i >= 0; --i)
	{
		ABNVehiclePawn* Car = Units[i].Car.Get();
		ABNPoliceController* Ctrl = Units[i].Controller.Get();
		if (!Car || !Ctrl || Ctrl->IsReadyForCleanup())
		{
			DestroyUnit(Units[i].Car, Units[i].Controller);
			Units.RemoveAtSwap(i);
		}
	}
}

void UBNPursuitSubsystem::ClearAllUnits()
{
	DestroyHelicopter();
	for (FPoliceUnit& Unit : Units)
	{
		DestroyUnit(Unit.Car, Unit.Controller);
	}
	Units.Reset();
}

void UBNPursuitSubsystem::ClearPatrols()
{
	for (FPoliceUnit& Unit : PatrolUnits)
	{
		DestroyUnit(Unit.Car, Unit.Controller);
	}
	PatrolUnits.Reset();
}

void UBNPursuitSubsystem::ClearSpikeStrips()
{
	for (FSpikeStripHazard& Strip : SpikeStrips)
	{
		if (AActor* Visual = Strip.VisualActor.Get())
		{
			Visual->Destroy();
		}
	}
	SpikeStrips.Reset();
}
