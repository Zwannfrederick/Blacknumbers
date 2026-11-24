#include "Police/BNPursuitBreaker.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float PillarOffsetY = 520.f;
	constexpr float PillarBaseZ = 250.f;
	constexpr float BeamBaseZ = 520.f;
	constexpr float BeamCollapsedZ = 65.f;

	constexpr float CollapseDurationSeconds = 0.85f;
	constexpr float DebrisWindowSeconds = 1.8f;
	constexpr float ResetMinPlayerDistCm = 6000.f;
	constexpr float WarningNormalCandelas = 180.f;
	constexpr float WarningFlashCandelas = 350.f;
	const FLinearColor WarningColor(1.f, 0.35f, 0.05f);
	const FLinearColor CollapseFlashColor(1.f, 0.15f, 0.05f);

	bool CanTriggerBreakerFor(UWorld* World, const ABNVehiclePawn* Car)
	{
		if (!World || !Car)
		{
			return false;
		}
		const UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>();
		if (!Pursuit || !Pursuit->GetState().IsPursuitRunning())
		{
			return false;
		}
		for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
		{
			if (It->IsRunningFor(Car))
			{
				return false;
			}
		}
		return true;
	}

	UStaticMeshComponent* MakeBeamPart(AActor& Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
		const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		return Part;
	}
}

ABNPursuitBreaker::ABNPursuitBreaker()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PillarL = MakeBeamPart(*this, RootComponent, TEXT("PillarL"), Cube.Object,
		FVector(0.f, -PillarOffsetY, PillarBaseZ), FVector(0.7f, 0.7f, 5.f));
	PillarR = MakeBeamPart(*this, RootComponent, TEXT("PillarR"), Cube.Object,
		FVector(0.f, PillarOffsetY, PillarBaseZ), FVector(0.7f, 0.7f, 5.f));
	OverheadBeam = MakeBeamPart(*this, RootComponent, TEXT("OverheadBeam"), Cube.Object,
		FVector(0.f, 0.f, BeamBaseZ), FVector(1.2f, 12.f, 1.1f));

	WarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarningLight"));
	WarningLight->SetupAttachment(RootComponent);
	WarningLight->SetRelativeLocation(FVector(0.f, 0.f, 560.f));
	WarningLight->SetIntensityUnits(ELightUnits::Candelas);
	WarningLight->SetIntensity(WarningNormalCandelas);
	WarningLight->SetLightColor(WarningColor);
	WarningLight->SetAttenuationRadius(2200.f);
	WarningLight->SetCastShadows(false);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	TriggerBox->SetBoxExtent(FVector(320.f, 520.f, 220.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABNPursuitBreaker::OnTriggerOverlap);
}

void ABNPursuitBreaker::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bCollapsed)
	{
		return;
	}
	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(OtherActor);
	if (Car && Car->IsPlayerControlled() && FMath::Abs(Car->GetSpeedKmh()) >= MinTriggerSpeedKmh && CanTriggerBreakerFor(GetWorld(), Car))
	{
		TriggerCollapse(Car);
	}
}

void ABNPursuitBreaker::TriggerCollapse(ABNVehiclePawn* TriggerCar)
{
	if (bCollapsed || !CanTriggerBreakerFor(GetWorld(), TriggerCar))
	{
		return;
	}
	bCollapsed = true;
	CollapseTimer = 0.f;
	DebrisTimer = 0.f;
	ResetTimer = 0.f;
	LaunchDebrisChunks();

	if (TriggerCar)
	{
		if (UBNVehicleEffectsComponent* Fx = TriggerCar->FindComponentByClass<UBNVehicleEffectsComponent>())
		{
			Fx->TriggerCollisionSparks(GetActorLocation() + FVector(0.f, 0.f, 220.f), FVector::UpVector, 1.3f);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
		{
			Audio->PlaySfxAtLocation(EBNSfxId::BreakerCollapse, GetActorLocation(), 1.0f);
		}
		if (UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>())
		{
			Pursuit->OnPursuitBreakerTriggered(this, TriggerCar);
		}
	}
}

void ABNPursuitBreaker::ResetStructure()
{
	bCollapsed = false;
	CollapseTimer = 0.f;
	DebrisTimer = 0.f;
	ResetTimer = 0.f;
	ApplyCollapseTransform(0.f);
	HideDebrisChunks();
	WarningLight->SetLightColor(WarningColor);
	WarningLight->SetIntensity(WarningNormalCandelas);
	WarningLight->SetVisibility(true);
}

void ABNPursuitBreaker::EnsureDebrisChunks()
{
	if (DebrisMeshes.Num() > 0 || !OverheadBeam)
	{
		return;
	}
	UStaticMesh* ChunkMesh = OverheadBeam->GetStaticMesh();
	UMaterialInterface* ChunkMat = OverheadBeam->GetMaterial(0);
	if (!ChunkMesh)
	{
		return;
	}
	constexpr int32 NumChunks = 8;
	DebrisMeshes.Reserve(NumChunks);
	DebrisPhysics.SetNum(NumChunks);
	for (int32 i = 0; i < NumChunks; ++i)
	{
		UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(this);
		Chunk->SetStaticMesh(ChunkMesh);
		if (ChunkMat) { Chunk->SetMaterial(0, ChunkMat); }
		Chunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chunk->SetCastShadow(false);
		Chunk->SetupAttachment(RootComponent);
		Chunk->SetRelativeScale3D(FVector(FMath::FRandRange(0.7f, 1.5f), FMath::FRandRange(0.7f, 1.8f), FMath::FRandRange(0.4f, 0.9f)));
		Chunk->SetVisibility(false, true);
		Chunk->RegisterComponent();
		DebrisMeshes.Add(Chunk);
	}
}

void ABNPursuitBreaker::LaunchDebrisChunks()
{
	EnsureDebrisChunks();
	for (int32 i = 0; i < DebrisMeshes.Num(); ++i)
	{
		UStaticMeshComponent* Chunk = DebrisMeshes[i];
		if (!IsValid(Chunk)) { continue; }
		const float OffsetY = FMath::Lerp(-420.f, 420.f, static_cast<float>(i) / FMath::Max(DebrisMeshes.Num() - 1, 1));
		Chunk->SetRelativeLocation(FVector(FMath::FRandRange(-90.f, 90.f), OffsetY, BeamBaseZ));
		Chunk->SetRelativeRotation(FRotator(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-30.f, 30.f)));
		Chunk->SetVisibility(true, true);

		DebrisPhysics[i].Velocity = FVector(FMath::FRandRange(-520.f, 520.f), OffsetY * 0.85f + FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(220.f, 580.f));
		DebrisPhysics[i].SpinRate = FRotator(FMath::FRandRange(-280.f, 280.f), FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(-280.f, 280.f));
	}
}

void ABNPursuitBreaker::TickDebrisChunks(float DeltaSeconds)
{
	for (int32 i = 0; i < DebrisMeshes.Num(); ++i)
	{
		UStaticMeshComponent* Chunk = DebrisMeshes[i];
		if (!IsValid(Chunk) || !Chunk->IsVisible()) { continue; }
		FDebrisChunk& Phys = DebrisPhysics[i];
		Phys.Velocity.Z -= 1450.f * DeltaSeconds;
		FVector Loc = Chunk->GetRelativeLocation() + Phys.Velocity * DeltaSeconds;
		if (Loc.Z < 38.f)
		{
			Loc.Z = 38.f;
			Phys.Velocity.Z = FMath::Abs(Phys.Velocity.Z) * 0.32f;
			Phys.Velocity.X *= 0.65f;
			Phys.Velocity.Y *= 0.65f;
			Phys.SpinRate *= 0.55f;
		}
		Chunk->SetRelativeLocation(Loc);
		Chunk->AddRelativeRotation(Phys.SpinRate * DeltaSeconds);
	}
}

void ABNPursuitBreaker::HideDebrisChunks()
{
	for (UStaticMeshComponent* Chunk : DebrisMeshes)
	{
		if (IsValid(Chunk))
		{
			Chunk->SetVisibility(false, true);
		}
	}
}

void ABNPursuitBreaker::ApplyCollapseTransform(float Alpha)
{
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Eased = A * A;

	PillarL->SetRelativeLocation(FVector(0.f, -PillarOffsetY - 140.f * Eased, PillarBaseZ - 180.f * Eased));
	PillarL->SetRelativeRotation(FRotator(0.f, 0.f, -78.f * Eased));

	PillarR->SetRelativeLocation(FVector(0.f, PillarOffsetY + 140.f * Eased, PillarBaseZ - 180.f * Eased));
	PillarR->SetRelativeRotation(FRotator(0.f, 0.f, 78.f * Eased));

	OverheadBeam->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(BeamBaseZ, BeamCollapsedZ, Eased)));
	OverheadBeam->SetRelativeRotation(FRotator(0.f, 0.f, 8.f * Eased));
}

void ABNPursuitBreaker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ABNVehiclePawn* Player = PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;

	if (!bCollapsed)
	{
		const bool bArmedForPursuit = CanTriggerBreakerFor(GetWorld(), Player);
		WarningLight->SetVisibility(bArmedForPursuit);
		if (!bArmedForPursuit)
		{
			return;
		}
		const bool bBeaconPulse = World && FMath::Fmod(World->GetTimeSeconds() * 2.5, 1.0) < 0.65;
		WarningLight->SetIntensity(bBeaconPulse ? WarningNormalCandelas : 80.f);

		if (Player && FMath::Abs(Player->GetSpeedKmh()) >= MinTriggerSpeedKmh)
		{
			const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
			if (FMath::Abs(Local.X) <= 340.f && FMath::Abs(Local.Y) <= 540.f && FMath::Abs(Local.Z - 200.f) <= 280.f)
			{
				TriggerCollapse(Player);
			}
		}
		return;
	}

	if (CollapseTimer < CollapseDurationSeconds)
	{
		CollapseTimer = FMath::Min(CollapseTimer + DeltaSeconds, CollapseDurationSeconds);
		ApplyCollapseTransform(CollapseTimer / CollapseDurationSeconds);
		WarningLight->SetLightColor(CollapseFlashColor);
		WarningLight->SetIntensity(WarningFlashCandelas);
		WarningLight->SetVisibility(true);
	}
	else
	{
		WarningLight->SetVisibility(false);
	}

	if (DebrisTimer < DebrisWindowSeconds)
	{
		DebrisTimer += DeltaSeconds;
		TickDebrisChunks(DeltaSeconds);
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->DisablePoliceNear(GetActorLocation(), DisableRadiusCm);
		}
	}
	else if (DebrisTimer < DebrisWindowSeconds + 3.0f)
	{
		DebrisTimer += DeltaSeconds;
		TickDebrisChunks(DeltaSeconds);
	}

	ResetTimer += DeltaSeconds;
	if (ResetTimer >= ResetCooldownSeconds)
	{
		const float PlayerDist = Player ? FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) : ResetMinPlayerDistCm + 100.f;
		if (PlayerDist > ResetMinPlayerDistCm)
		{
			ResetStructure();
		}
	}
}
