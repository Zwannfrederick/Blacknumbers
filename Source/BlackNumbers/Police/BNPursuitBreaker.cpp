#include "Police/BNPursuitBreaker.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "UObject/ConstructorHelpers.h"
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

	if (UWorld* World = GetWorld())
	{
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
	WarningLight->SetLightColor(WarningColor);
	WarningLight->SetIntensity(WarningNormalCandelas);
	WarningLight->SetVisibility(true);
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
		if (UBNPursuitSubsystem* Pursuit = World ? World->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
		{
			Pursuit->DisablePoliceNear(GetActorLocation(), DisableRadiusCm);
		}
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
