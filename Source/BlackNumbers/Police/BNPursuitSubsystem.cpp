#include "Police/BNPursuitSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitBreaker.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float ChaserSpawnIntervalSeconds = 2.5f;
	constexpr float ChaserSpawnDistanceBehindCm = 3500.f;
	constexpr float ChaserSideOffsetCm = 360.f;

	constexpr float RoadblockIntervalSeconds = 18.f;
	constexpr float RoadblockDistanceAheadCm = 11500.f;
	constexpr float RoadblockDespawnDistCm = 16500.f;
	constexpr float RoadblockPassedDistCm = 4200.f;
	constexpr float RoadblockGapHalfWidthCm = 460.f;

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

void UBNPursuitSubsystem::StartPursuit(int32 InitialHeat)
{
	ClearAllUnits();
	State.StartPursuit(InitialHeat);
	SpawnCooldown = 0.f;
	RoadblockCooldown = 8.f;
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
		State.PostDispatch(Spec.bDemirInvolved ? TEXT("BASKOMISER DEMIR") : TEXT("MERKEZ"),
			FString::Printf(TEXT("Takip %s seviyesine yukseltildi!"), *Spec.Label));
	}
	else
	{
		StopPursuit();
	}
}

void UBNPursuitSubsystem::StopPursuit()
{
	State.EndPursuitImmediate();
	ClearAllUnits();
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
	if (!Breaker || !State.IsPursuitRunning() || IsPlayerInActiveRace(GetWorld(), ActivePlayer))
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
		}
	}
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

void UBNPursuitSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State.Phase == EBNPursuitPhase::None)
	{
		if (Units.Num() > 0)
		{
			ClearAllUnits();
		}
		return;
	}

	if (!State.IsPursuitRunning())
	{
		State.Update(DeltaTime, 0.f, BIG_NUMBER, 0);
		if (State.Phase == EBNPursuitPhase::None)
		{
			ClearAllUnits();
		}
		return;
	}

	ABNVehiclePawn* Player = FindPlayerCar();
	if (!Player || IsPlayerInActiveRace(GetWorld(), Player))
	{
		StopPursuit();
		return;
	}

	PruneUnits();

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
		if (Ctrl->IsRoadblockUnit())
		{
			++ActiveRoadblocks;
		}
		else
		{
			++ActiveChasers;
		}
	}

	const int32 PrevHeat = State.HeatLevel;
	const EBNPursuitPhase PrevPhase = State.Phase;
	State.Update(DeltaTime, Player->GetSpeedKmh(), NearestDist, TotalActive);

	if (PrevPhase != EBNPursuitPhase::Escaped && State.Phase == EBNPursuitPhase::Escaped)
	{
		if (UBNProgressionSubsystem* Prog = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->GrantPursuitEscapeReward(State.Bounty, State.HeatLevel);
		}
	}

	if (UBNStorySubsystem* Story = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		if (Story->GetState().bFinalChaseActive && Story->CheckFinalChaseEscape(Player))
		{
			return;
		}
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

	// Clean out old roadblocks once passed or far behind.
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
				DestroyUnit(Units[i].Car, Units[i].Controller);
				Units.RemoveAtSwap(i);
				--ActiveRoadblocks;
			}
		}
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

void UBNPursuitSubsystem::SpawnChaser(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec, int32 Slot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Forward = Player.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Player.GetActorRightVector().GetSafeNormal2D();
	const float Side = (Slot % 2 == 0 ? 1.f : -1.f) * ChaserSideOffsetCm;
	const FVector SpawnLoc = Player.GetActorLocation() - Forward * ChaserSpawnDistanceBehindCm + Right * Side + FVector(0.f, 0.f, 60.f);
	const FRotator SpawnRot(0.f, Forward.Rotation().Yaw, 0.f);

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
	Ctrl->SetupPursuitUnit(FindPlayerCar(), Spec, Slot, false);
	Units.Add({ Cruiser, Ctrl });
}

void UBNPursuitSubsystem::SpawnRoadblock(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Forward = Player.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Player.GetActorRightVector().GetSafeNormal2D();
	const FVector Center = Player.GetActorLocation() + Forward * RoadblockDistanceAheadCm + FVector(0.f, 0.f, 60.f);
	const float BaseYaw = Forward.Rotation().Yaw;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Two angled interceptors leaving a central tactical lane wide enough for a Speedbreaker pass.
	for (const float Side : { -1.f, 1.f })
	{
		const FVector Loc = Center + Right * (Side * RoadblockGapHalfWidthCm);
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
		Ctrl->SetupPursuitUnit(&Player, Spec, 0, true);
		Units.Add({ Unit, Ctrl });
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
	for (FPoliceUnit& Unit : Units)
	{
		DestroyUnit(Unit.Car, Unit.Controller);
	}
	Units.Reset();
}

static FAutoConsoleCommandWithWorldAndArgs GBNPursuitStartCmd(
	TEXT("bn.Pursuit"),
	TEXT("bn.Pursuit [1-5]  Starts a Karamara police pursuit at the given Heat level."),
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
