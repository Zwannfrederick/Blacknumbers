#include "World/BNBreakableProp.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Police/BNPursuitSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float HitRadiusCm = 155.f;
	constexpr float SettleHideSeconds = 5.f;
}

ABNBreakableProp::ABNBreakableProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.04f; // 25 Hz proximity backup; OnPropHit handles instant high-speed impacts
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;

	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	RootComponent = PropMesh;
	PropMesh->SetStaticMesh(CubeMesh);
	PropMesh->SetSimulatePhysics(false);
	PropMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PropMesh->SetNotifyRigidBodyCollision(true);
	PropMesh->SetCastShadow(false);
	PropMesh->OnComponentHit.AddDynamic(this, &ABNBreakableProp::OnPropHit);
}

void ABNBreakableProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	switch (PropKind)
	{
	case EBNBreakableKind::Bin:
		if (CylinderMesh)
		{
			PropMesh->SetStaticMesh(CylinderMesh);
		}
		PropMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.95f));
		break;
	case EBNBreakableKind::Crate:
		if (CubeMesh)
		{
			PropMesh->SetStaticMesh(CubeMesh);
		}
		PropMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		break;
	default: // Barrier
		if (CubeMesh)
		{
			PropMesh->SetStaticMesh(CubeMesh);
		}
		PropMesh->SetRelativeScale3D(FVector(1.8f, 0.45f, 0.85f));
		break;
	}
}

void ABNBreakableProp::OnPropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bBroken)
	{
		return;
	}
	const ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(OtherActor);
	if (Car && FMath::Abs(Car->GetSpeedKmh()) >= MinBreakSpeedKmh)
	{
		BreakProp(Car->GetVelocity(), Car->IsPlayerControlled());
	}
}

void ABNBreakableProp::BreakProp(const FVector& ImpactVelocity, bool bByPlayer)
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;
	BrokenSeconds = 0.f;
	SetActorTickInterval(0.25f);

	PropMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	PropMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PropMesh->SetSimulatePhysics(true);
	PropMesh->SetPhysicsLinearVelocity(ImpactVelocity * 1.35f + FVector(0.f, 0.f, 420.f));
	PropMesh->SetPhysicsAngularVelocityInDegrees(FVector(240.f, 190.f, 360.f));

	if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
	{
		const EBNSfxId Sfx = (PropKind == EBNBreakableKind::Crate) ? EBNSfxId::BreakWood : EBNSfxId::BreakMetal;
		Audio->PlaySfxAtLocation(Sfx, GetActorLocation(), 0.85f);
	}

	if (bByPlayer)
	{
		if (UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->ReportCollateralDamage(CollateralCostDollars, TEXT("Şehir Dekoru"));
		}
	}
}

void ABNBreakableProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bBroken)
	{
		BrokenSeconds += DeltaSeconds;
		if (BrokenSeconds >= SettleHideSeconds)
		{
			PropMesh->SetSimulatePhysics(false);
			PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SetActorHiddenInGame(true);
			SetActorTickEnabled(false);
		}
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ABNVehiclePawn* Player = PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	if (Player && FMath::Abs(Player->GetSpeedKmh()) >= MinBreakSpeedKmh)
	{
		if (FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(HitRadiusCm)
			&& FMath::Abs(Player->GetActorLocation().Z - GetActorLocation().Z) < 180.f)
		{
			BreakProp(Player->GetVelocity(), true);
		}
	}
}
