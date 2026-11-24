#include "Race/BNRaceEvent.h"

#include "Audio/BNAudioSubsystem.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceDefinition.h"
#include "Race/BNRivalController.h"
#include "Story/BNStorySubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float CountdownSeconds = 3.f;
	constexpr float ResultsSeconds = 6.f;
	/** The beacon re-arms once the car is this much farther away than StartRadius. */
	constexpr float RearmRadiusScale = 1.5f;
	constexpr float BeaconHeight = 3000.f;
	constexpr float PostHeight = 600.f;
	/** Drift scoring: only firm hits on walls/objects (not landings) cost the running drift. */
	constexpr float MinImpactImpulse = 100000.f;
	constexpr float WallNormalMaxZ = 0.6f;
	/** Rival grid: rows behind the player, alternating right and left of the player's line. */
	constexpr float GridRowSpacing = 900.f;
	constexpr float GridLaneOffset = 300.f;

	ABNVehiclePawn* PlayerCar(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	}

	UStaticMeshComponent* MakePillar(AActor& Owner, const TCHAR* Name, UStaticMesh* Mesh, float Radius, float Height)
	{
		UStaticMeshComponent* Pillar = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Pillar->SetupAttachment(Owner.GetRootComponent());
		Pillar->SetStaticMesh(Mesh);
		Pillar->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, Height / 100.f));
		Pillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pillar->SetCastShadow(false);
		return Pillar;
	}
}

ABNRaceEvent::ABNRaceEvent()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RacingLine = CreateDefaultSubobject<USplineComponent>(TEXT("RacingLine"));
	RacingLine->SetupAttachment(RootComponent);

	Beacon = MakePillar(*this, TEXT("Beacon"), Cylinder.Object, 150.f, BeaconHeight);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, BeaconHeight * 0.5f));
	for (const TCHAR* Name : { TEXT("GatePostL"), TEXT("GatePostR") })
	{
		UStaticMeshComponent* Post = MakePillar(*this, Name, Cylinder.Object, 40.f, PostHeight);
		Post->SetUsingAbsoluteLocation(true);
		Post->SetUsingAbsoluteRotation(true);
		Post->SetVisibility(false);
		GatePosts.Add(Post);
	}
}

void ABNRaceEvent::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	UStaticMesh* RingMesh = FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_RaceRing"))
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_RaceRing.SM_BN_RaceRing"))
		: nullptr;
	UStaticMesh* ChevronMesh = FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_CheckpointChevron"))
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_CheckpointChevron.SM_BN_CheckpointChevron"))
		: nullptr;
	bUsingHoloRaceMeshes = (RingMesh != nullptr && ChevronMesh != nullptr);
	if (bUsingHoloRaceMeshes)
	{
		if (Beacon)
		{
			Beacon->SetStaticMesh(RingMesh);
			Beacon->SetRelativeLocation(FVector::ZeroVector);
			Beacon->SetRelativeScale3D(FVector::OneVector);
		}
		for (UStaticMeshComponent* Post : GatePosts)
		{
			if (Post)
			{
				Post->SetStaticMesh(ChevronMesh);
				Post->SetRelativeScale3D(FVector::OneVector);
			}
		}
	}
}

void ABNRaceEvent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Elapsed = GetWorld()->GetTimeSeconds() - StateStart;
	if (State == EBNRaceState::Finished)
	{
		// Results run on a timer and do not need the player's car.
		if (Elapsed >= ResultsSeconds)
		{
			State = EBNRaceState::Idle;
			Beacon->SetVisibility(true);
			ClearRivals();
		}
		return;
	}

	ABNVehiclePawn* Car = State == EBNRaceState::Idle ? PlayerCar(GetWorld()) : Player.Car.Get();
	if (!Car)
	{
		if (State != EBNRaceState::Idle)
		{
			Finish(); // racer gone (respawned or level change)
		}
		return;
	}

	switch (State)
	{
	case EBNRaceState::Idle:
		TickIdle(*Car);
		break;
	case EBNRaceState::Countdown:
		if (Elapsed >= CountdownSeconds)
		{
			State = EBNRaceState::Racing;
			StateStart = GetWorld()->GetTimeSeconds();
			LastCountdownDigit = 0;
			if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
			{
				Audio->PlaySfx2D(EBNSfxId::CountdownGo, 0.95f);
			}
			Player.LastLocation = Car->GetActorLocation();
			Car->SetInputLocked(false);
			for (FBNRacer& Rival : Rivals)
			{
				if (ABNVehiclePawn* RivalCar = Rival.Car.Get())
				{
					Rival.LastLocation = RivalCar->GetActorLocation();
					RivalCar->SetInputLocked(false);
				}
			}
		}
		else
		{
			const int32 Digit = FMath::CeilToInt(CountdownSeconds - static_cast<float>(Elapsed));
			if (Digit < LastCountdownDigit && Digit >= 1)
			{
				LastCountdownDigit = Digit;
				if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
				{
					Audio->PlaySfx2D(EBNSfxId::CountdownBeep, 0.85f);
				}
			}
		}
		break;
	case EBNRaceState::Racing:
		TickRacing(*Car, DeltaSeconds);
		break;
	default:
		break;
	}
}

ABNRaceEvent* ABNRaceEvent::FindActiveRace(const UWorld* World, const ABNRaceEvent* IgnoreEvent)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABNRaceEvent> It(const_cast<UWorld*>(World)); It; ++It)
	{
		ABNRaceEvent* Event = *It;
		if (!Event || Event == IgnoreEvent)
		{
			continue;
		}
		if (Event->State == EBNRaceState::Countdown || Event->State == EBNRaceState::Racing)
		{
			return Event;
		}
	}
	return nullptr;
}

void ABNRaceEvent::TickIdle(ABNVehiclePawn& Car)
{
	// Never display or trigger another race's world beacon while ANY race is already active!
	if (FindActiveRace(GetWorld(), this) != nullptr)
	{
		bArmed = false;
		if (Beacon && Beacon->IsVisible())
		{
			Beacon->SetVisibility(false);
		}
		return;
	}
	if (Beacon && !Beacon->IsVisible())
	{
		Beacon->SetVisibility(true);
	}

	const float Distance = FVector::Dist2D(Car.GetActorLocation(), GetActorLocation());
	if (Distance > StartRadius * RearmRadiusScale)
	{
		bArmed = true;
	}
	else if (bArmed && Distance <= StartRadius)
	{
		StartRace(&Car);
	}
}

void ABNRaceEvent::StartRace(ABNVehiclePawn* Car)
{
	BeginRaceInternal(Car, 0, 0.f, nullptr, FLinearColor::White, 0, EBNCarId::KuroganeMSpec);
}

void ABNRaceEvent::StartBossRace(ABNVehiclePawn* Car, int32 BossRank, float BossSkill,
	const FBNComputedVehicleStats& BossStats, const FLinearColor& BossTint, int32 DriftTargetScore,
	EBNCarId BossCarId)
{
	BeginRaceInternal(Car, BossRank, BossSkill, &BossStats, BossTint, DriftTargetScore, BossCarId);
}

void ABNRaceEvent::BeginRaceInternal(ABNVehiclePawn* Car, int32 BossRank, float BossSkill,
	const FBNComputedVehicleStats* BossStats, const FLinearColor& BossTint, int32 DriftTargetScore,
	EBNCarId BossCarId)
{
	if (!Car || !Definition || State == EBNRaceState::Countdown || State == EBNRaceState::Racing)
	{
		return;
	}
	// Hard invariant: never allow two races to run concurrently in the same world.
	if (ABNRaceEvent* OtherActive = FindActiveRace(GetWorld(), this))
	{
		OtherActive->EndRace();
		OtherActive->ClearRivals();
		OtherActive->State = EBNRaceState::Idle;
		OtherActive->bArmed = false;
		if (OtherActive->Beacon)
		{
			OtherActive->Beacon->SetVisibility(false);
		}
	}
	ActiveBossRank = BossRank;
	ActiveBossDriftTarget = DriftTargetScore;
	ActiveBossSkill = BossSkill;
	ActiveBossCarId = BossCarId;
	ActiveBossStats = BossStats ? *BossStats : FBNComputedVehicleStats();
	ActiveBossTint = BossTint;

	Player.Car = Car;
	bArmed = false;
	ClearRivals();

	// Never carry an active free-roam police chase into a race grid (teleporting away from cops would trigger phantom Evade).
	if (UWorld* World = GetWorld())
	{
		if (UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>())
		{
			if (Pursuit->GetState().Phase != EBNPursuitPhase::None)
			{
				Pursuit->StopPursuit();
			}
		}
	}

	Car->SetActorTransform(Definition->Grid, false, nullptr, ETeleportType::TeleportPhysics);
	Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Car->SetInputLocked(true);
	Car->GetMesh()->OnComponentHit.AddUniqueDynamic(this, &ABNRaceEvent::OnCarHit);

	Player.Progress.Reset(Definition->Checkpoints.Num(), Definition->Laps, Definition->Discipline == EBNRaceDiscipline::Circuit);
	DriftScore.Reset();
	Beacon->SetVisibility(false);
	const bool bDrift = Definition->Discipline == EBNRaceDiscipline::Drift;
	ShowGate(bDrift ? INDEX_NONE : Player.Progress.NextCheckpoint());
	if (!bDrift)
	{
		BuildRacingLine();
		SpawnRivals();
	}

	State = EBNRaceState::Countdown;
	StateStart = GetWorld()->GetTimeSeconds();
	LastCountdownDigit = 4;
}

void ABNRaceEvent::TickRacing(ABNVehiclePawn& Car, float DeltaSeconds)
{
	if (Definition->Discipline == EBNRaceDiscipline::Drift)
	{
		DriftScore.Update(DeltaSeconds, Car.GetSlipAngleDegrees(), Car.GetSpeedKmh());
		if (GetTimeLeft() <= 0.f)
		{
			Finish();
		}
		return;
	}

	for (FBNRacer& Rival : Rivals)
	{
		AdvanceRacer(Rival);
	}
	const EBNRaceStep Step = AdvanceRacer(Player);
	if (Step == EBNRaceStep::Finished)
	{
		Finish();
	}
	else if (Step != EBNRaceStep::None)
	{
		ShowGate(Player.Progress.NextCheckpoint());
	}
}

EBNRaceStep ABNRaceEvent::AdvanceRacer(FBNRacer& Racer)
{
	const ABNVehiclePawn* Car = Racer.Car.Get();
	if (!Car)
	{
		return EBNRaceStep::None;
	}
	const FVector Location = Car->GetActorLocation();
	EBNRaceStep Step = EBNRaceStep::None;
	const int32 Next = Racer.Progress.NextCheckpoint();
	if (!Racer.Progress.IsFinished() && Definition->Checkpoints.IsValidIndex(Next))
	{
		// Crossing = the car went from behind the gate plane to in front of it, between the posts.
		const FTransform& Gate = Definition->Checkpoints[Next];
		const FVector Before = Gate.InverseTransformPosition(Racer.LastLocation);
		const FVector After = Gate.InverseTransformPosition(Location);
		if (Before.X < 0.f && After.X >= 0.f && FMath::Abs(After.Y) <= Definition->GateWidth * 0.5f)
		{
			Step = Racer.Progress.Pass(Next, GetRaceTime());
		}
	}
	Racer.LastLocation = Location;
	return Step;
}

bool ABNRaceEvent::IsAhead(const FBNRacer& A, const FBNRacer& B) const
{
	const FBNRaceProgress& PA = A.Progress;
	const FBNRaceProgress& PB = B.Progress;
	if (PA.IsFinished() || PB.IsFinished())
	{
		return PA.IsFinished() && (!PB.IsFinished() || PA.FinishTime() < PB.FinishTime());
	}
	if (PA.Passed() != PB.Passed())
	{
		return PA.Passed() > PB.Passed();
	}
	// Same stretch of road: whoever is closer to the next gate leads.
	const ABNVehiclePawn* CarA = A.Car.Get();
	const ABNVehiclePawn* CarB = B.Car.Get();
	if (!CarA || !CarB || !Definition->Checkpoints.IsValidIndex(PA.NextCheckpoint()))
	{
		return false;
	}
	const FVector Gate = Definition->Checkpoints[PA.NextCheckpoint()].GetLocation();
	return FVector::DistSquared2D(CarA->GetActorLocation(), Gate) < FVector::DistSquared2D(CarB->GetActorLocation(), Gate);
}

int32 ABNRaceEvent::GetPlayerPosition() const
{
	int32 Position = 1;
	for (const FBNRacer& Rival : Rivals)
	{
		Position += IsAhead(Rival, Player) ? 1 : 0;
	}
	return Position;
}

void ABNRaceEvent::BuildRacingLine()
{
	const bool bCircuit = Definition->Discipline == EBNRaceDiscipline::Circuit;
	TArray<FVector> Points = Definition->RacingLine;
	if (Points.Num() < 2)
	{
		Points.Reset();
		if (!bCircuit)
		{
			Points.Add(Definition->Grid.GetLocation());
		}
		for (const FTransform& Gate : Definition->Checkpoints)
		{
			Points.Add(Gate.GetLocation());
		}
	}
	RacingLine->SetSplinePoints(Points, ESplineCoordinateSpace::World);
	RacingLine->SetClosedLoop(bCircuit);
}

void ABNRaceEvent::SpawnRivals()
{
	const bool bCircuit = Definition->Discipline == EBNRaceDiscipline::Circuit;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const int32 RivalCount = ActiveBossRank > 0 ? 1 : Definition->RivalSkills.Num();

	for (int32 i = 0; i < RivalCount; ++i)
	{
		const float Lane = (i % 2 == 0 ? 1.f : -1.f) * GridLaneOffset;
		const FVector Slot = Definition->Grid.TransformPosition(FVector(-GridRowSpacing * ((i + 1) / 2), Lane, 0.f));
		ABNVehiclePawn* Car = GetWorld()->SpawnActor<ABNVehiclePawn>(ABNVehiclePawn::StaticClass(),
			FTransform(Definition->Grid.GetRotation(), Slot), Params);
		if (!Car)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: could not spawn rival %d"), *GetName(), i);
			continue;
		}
		if (ActiveBossRank > 0)
		{
			Car->ApplyVehicleSpec(ActiveBossCarId, ActiveBossStats);
			Car->ApplyPaintTint(ActiveBossTint);
		}
		else
		{
			// Give free-roam race rivals varied street body styles/paints & engine voices while keeping calibrated racing physics
			static const EBNCarId RivalCars[] = { EBNCarId::ValkenRS4, EBNCarId::Vandal440R, EBNCarId::DracoRST };
			const EBNCarId RivalId = RivalCars[i % UE_ARRAY_COUNT(RivalCars)];
			const int32 StockTiers[FBNUpgradeSpec::NumSlots] = {};
			Car->ApplyVehicleSpec(RivalId, FBNComputedVehicleStats::Compute(EBNCarId::KuroganeMSpec, StockTiers));
		}
		Car->AIControllerClass = ABNRivalController::StaticClass();
		Car->SpawnDefaultController();
		ABNRivalController* Driver = Cast<ABNRivalController>(Car->GetController());
		if (!Driver)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: rival %d has no driver"), *GetName(), i);
			Car->Destroy();
			continue;
		}
		const float Skill = ActiveBossRank > 0 ? ActiveBossSkill : Definition->RivalSkills[i];
		Driver->Setup(this, Skill, Lane);
		Car->SetInputLocked(true);
		FBNRacer& Rival = Rivals.AddDefaulted_GetRef();
		Rival.Car = Car;
		Rival.Progress.Reset(Definition->Checkpoints.Num(), Definition->Laps, bCircuit);
	}
}

void ABNRaceEvent::ClearRivals()
{
	for (const FBNRacer& Rival : Rivals)
	{
		if (ABNVehiclePawn* Car = Rival.Car.Get())
		{
			if (AController* Driver = Car->GetController())
			{
				Driver->Destroy();
			}
			Car->Destroy();
		}
	}
	Rivals.Reset();
}

void ABNRaceEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRivals();
	Super::EndPlay(EndPlayReason);
}

void ABNRaceEvent::ShowGate(int32 Index)
{
	const bool bShow = Definition && Definition->Checkpoints.IsValidIndex(Index);
	for (int32 Side = 0; Side < GatePosts.Num(); ++Side)
	{
		GatePosts[Side]->SetVisibility(bShow);
		if (bShow)
		{
			const float Y = (Side == 0 ? -0.5f : 0.5f) * Definition->GateWidth;
			const float ZOff = bUsingHoloRaceMeshes ? 0.f : PostHeight * 0.5f;
			const FTransform& Cp = Definition->Checkpoints[Index];
			GatePosts[Side]->SetWorldLocation(Cp.TransformPosition(FVector(0.f, Y, ZOff)));
			if (bUsingHoloRaceMeshes)
			{
				GatePosts[Side]->SetWorldRotation(Cp.GetRotation());
			}
		}
	}
}

void ABNRaceEvent::EndRace()
{
	if (State == EBNRaceState::Countdown || State == EBNRaceState::Racing)
	{
		Finish();
	}
}

void ABNRaceEvent::Finish()
{
	const float RaceTime = State == EBNRaceState::Racing ? GetRaceTime() : 0.f;
	const bool bDrift = Definition && Definition->Discipline == EBNRaceDiscipline::Drift;
	const bool bDriftTimedOut = bDrift && State == EBNRaceState::Racing && GetTimeLeft() <= 0.1f;
	DriftScore.Finish();
	if (ABNVehiclePawn* Car = Player.Car.Get())
	{
		Car->SetInputLocked(false);
		Car->GetMesh()->OnComponentHit.RemoveDynamic(this, &ABNRaceEvent::OnCarHit);
	}
	ShowGate(INDEX_NONE);
	State = EBNRaceState::Finished;
	StateStart = GetWorld()->GetTimeSeconds();
	const FBNRaceProgress& Progress = Player.Progress;

	const bool bCompleted = Progress.IsFinished() || bDriftTimedOut;
	const int32 FinishedBossRank = ActiveBossRank;
	const int32 BossDriftTarget = ActiveBossDriftTarget;
	const bool bWonBoss = FinishedBossRank > 0 && bCompleted && (bDrift ? DriftScore.Total() >= BossDriftTarget : GetPlayerPosition() == 1);
	ActiveBossRank = 0;
	ActiveBossDriftTarget = 0;

	if (FinishedBossRank > 0)
	{
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->OnBossRaceFinished(FinishedBossRank, bWonBoss, Player.Car.Get());
		}
	}
	else if (bCompleted)
	{
		if (UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->GrantRaceReward(GetPlayerPosition(), Rivals.Num() + 1, bDrift, DriftScore.Total());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("BN race '%s' over: finished=%d place=%d/%d time=%.2fs laps=%d drift=%d"),
		Definition ? *Definition->DisplayName.ToString() : TEXT("?"), Progress.IsFinished(), GetPlayerPosition(), Rivals.Num() + 1,
		Progress.IsFinished() ? Progress.FinishTime() : RaceTime, Progress.LapTimes().Num(), DriftScore.Total());
}

float ABNRaceEvent::GetRaceTime() const
{
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - StateStart);
	switch (State)
	{
	case EBNRaceState::Countdown:
		return CountdownSeconds - Elapsed;
	case EBNRaceState::Racing:
		return Elapsed;
	case EBNRaceState::Finished:
		return static_cast<float>(Player.Progress.FinishTime());
	default:
		return 0.f;
	}
}

float ABNRaceEvent::GetTimeLeft() const
{
	const bool bTimed = Definition && Definition->Discipline == EBNRaceDiscipline::Drift && State == EBNRaceState::Racing;
	return bTimed ? Definition->TimeLimitSeconds - GetRaceTime() : 0.f;
}

void ABNRaceEvent::OnCarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (State == EBNRaceState::Racing && FMath::Abs(Hit.ImpactNormal.Z) < WallNormalMaxZ && NormalImpulse.Size() > MinImpactImpulse)
	{
		DriftScore.OnImpact();
	}
}
