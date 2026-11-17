#include "Race/BNRaceEvent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Race/BNRaceDefinition.h"
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
	Beacon = MakePillar(*this, TEXT("Beacon"), Cylinder.Object, 150.f, BeaconHeight);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, BeaconHeight * 0.5f));
	for (const TCHAR* Name : { TEXT("GatePostL"), TEXT("GatePostR") })
	{
		UStaticMeshComponent* Post = MakePillar(*this, Name, Cylinder.Object, 40.f, PostHeight);
		Post->SetUsingAbsoluteLocation(true);
		Post->SetVisibility(false);
		GatePosts.Add(Post);
	}
}

void ABNRaceEvent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABNVehiclePawn* Car = State == EBNRaceState::Idle ? PlayerCar(GetWorld()) : Racer.Get();
	if (!Car)
	{
		if (State != EBNRaceState::Idle)
		{
			Finish(); // racer gone (respawned or level change)
		}
		return;
	}

	const double Elapsed = GetWorld()->GetTimeSeconds() - StateStart;
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
			LastCarLocation = Car->GetActorLocation();
			Car->SetInputLocked(false);
		}
		break;
	case EBNRaceState::Racing:
		TickRacing(*Car, DeltaSeconds);
		break;
	case EBNRaceState::Finished:
		if (Elapsed >= ResultsSeconds)
		{
			State = EBNRaceState::Idle;
			Beacon->SetVisibility(true);
		}
		break;
	}
}

void ABNRaceEvent::TickIdle(ABNVehiclePawn& Car)
{
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
	if (!Car || !Definition || State == EBNRaceState::Countdown || State == EBNRaceState::Racing)
	{
		return;
	}
	Racer = Car;
	bArmed = false;

	Car->SetActorTransform(Definition->Grid, false, nullptr, ETeleportType::TeleportPhysics);
	Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Car->SetInputLocked(true);
	Car->GetMesh()->OnComponentHit.AddUniqueDynamic(this, &ABNRaceEvent::OnCarHit);

	const bool bCircuit = Definition->Discipline == EBNRaceDiscipline::Circuit;
	Progress.Reset(Definition->Checkpoints.Num(), Definition->Laps, bCircuit);
	DriftScore.Reset();
	Beacon->SetVisibility(false);
	ShowGate(Definition->Discipline == EBNRaceDiscipline::Drift ? INDEX_NONE : Progress.NextCheckpoint());

	State = EBNRaceState::Countdown;
	StateStart = GetWorld()->GetTimeSeconds();
}

void ABNRaceEvent::TickRacing(ABNVehiclePawn& Car, float DeltaSeconds)
{
	const FVector Location = Car.GetActorLocation();
	if (Definition->Discipline == EBNRaceDiscipline::Drift)
	{
		DriftScore.Update(DeltaSeconds, Car.GetSlipAngleDegrees(), Car.GetSpeedKmh());
		if (GetTimeLeft() <= 0.f)
		{
			Finish();
		}
	}
	else if (Definition->Checkpoints.IsValidIndex(Progress.NextCheckpoint()))
	{
		// Crossing = the car went from behind the gate plane to in front of it, between the posts.
		const FTransform& Gate = Definition->Checkpoints[Progress.NextCheckpoint()];
		const FVector Before = Gate.InverseTransformPosition(LastCarLocation);
		const FVector After = Gate.InverseTransformPosition(Location);
		if (Before.X < 0.f && After.X >= 0.f && FMath::Abs(After.Y) <= Definition->GateWidth * 0.5f)
		{
			const EBNRaceStep Step = Progress.Pass(Progress.NextCheckpoint(), GetRaceTime());
			if (Step == EBNRaceStep::Finished)
			{
				Finish();
			}
			else
			{
				ShowGate(Progress.NextCheckpoint());
			}
		}
	}
	LastCarLocation = Location;
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
			GatePosts[Side]->SetWorldLocation(Definition->Checkpoints[Index].TransformPosition(FVector(0.f, Y, PostHeight * 0.5f)));
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
	DriftScore.Finish();
	if (ABNVehiclePawn* Car = Racer.Get())
	{
		Car->SetInputLocked(false);
		Car->GetMesh()->OnComponentHit.RemoveDynamic(this, &ABNRaceEvent::OnCarHit);
	}
	ShowGate(INDEX_NONE);
	State = EBNRaceState::Finished;
	StateStart = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("BN race '%s' over: finished=%d time=%.2fs laps=%d drift=%d"),
		Definition ? *Definition->DisplayName.ToString() : TEXT("?"), Progress.IsFinished(),
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
		return static_cast<float>(Progress.FinishTime());
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
