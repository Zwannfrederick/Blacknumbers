#include "Garage/BNGarageZone.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float BayHalfWidth = 340.f;
	constexpr float BayPillarHeight = 360.f;
	constexpr float RearmScale = 1.6f;
	const FLinearColor GarageBayColor(1.0f, 0.72f, 0.34f);

	UStaticMeshComponent* MakeBayPart(AActor& Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
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

ABNGarageZone::ABNGarageZone()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Slim frame pillars kept clean (real workshop shutter, tools, floodlights & ground holo ring placed by Karamara builder).
	PillarL = MakeBayPart(*this, RootComponent, TEXT("PillarL"), Cube.Object,
		FVector(0.f, -BayHalfWidth, BayPillarHeight * 0.5f), FVector(0.24f, 0.24f, BayPillarHeight / 100.f));
	PillarR = MakeBayPart(*this, RootComponent, TEXT("PillarR"), Cube.Object,
		FVector(0.f, BayHalfWidth, BayPillarHeight * 0.5f), FVector(0.24f, 0.24f, BayPillarHeight / 100.f));
	BeaconBeam = MakeBayPart(*this, RootComponent, TEXT("BeaconBeam"), Cube.Object,
		FVector(0.f, 0.f, BayPillarHeight + 18.f), FVector(0.32f, (BayHalfWidth * 2.f + 48.f) / 100.f, 0.24f));

	BayLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BayLight"));
	BayLight->SetupAttachment(RootComponent);
	BayLight->SetRelativeLocation(FVector(0.f, 0.f, BayPillarHeight - 20.f));
	BayLight->SetIntensityUnits(ELightUnits::Candelas);
	BayLight->SetIntensity(360.f);
	BayLight->SetLightColor(GarageBayColor);
	BayLight->SetAttenuationRadius(1350.f);
	BayLight->SetCastShadows(false);
}

ABNGarageZone* ABNGarageZone::FindNearbyGarageZone(const ABNVehiclePawn* Car)
{
	const UWorld* World = Car ? Car->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>();
	if (Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		return nullptr;
	}
	if (ABNRaceEvent::FindActiveRace(World) != nullptr)
	{
		return nullptr;
	}

	const FVector CarLoc = Car->GetActorLocation();
	ABNGarageZone* Nearest = nullptr;
	float BestDist = BIG_NUMBER;
	for (TActorIterator<ABNGarageZone> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const float Dist = FVector::Dist2D(CarLoc, It->GetActorLocation());
		if (Dist <= It->TriggerRadius && Dist < BestDist)
		{
			BestDist = Dist;
			Nearest = *It;
		}
	}
	return Nearest;
}

void ABNGarageZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ABNVehiclePawn* Car = PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	UBNProgressionSubsystem* Prog = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	if (!Car || !Prog)
	{
		return;
	}

	const float Distance = FVector::Dist2D(Car->GetActorLocation(), GetActorLocation());
	if (Distance > TriggerRadius * RearmScale)
	{
		bRepairArmed = true;
		return;
	}

	// Non-intrusive pass-through repair: rolling through the workshop bay automatically restores chassis health for free.
	if (bRepairArmed && Distance <= TriggerRadius && (Car->GetVehicleHealth() < 0.99f || Car->IsTotaled()))
	{
		bRepairArmed = false;
		Car->RepairVehicle();
		if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
		{
			Audio->PlaySfx2D(EBNSfxId::GarageRepair, 0.90f);
		}
		Prog->ShowNotificationBanner(FString::Printf(TEXT("ARAÇ TAMİR EDİLDİ (%s — ÜCRETSİZ)"), *GarageName), 3.0f);
	}
}
