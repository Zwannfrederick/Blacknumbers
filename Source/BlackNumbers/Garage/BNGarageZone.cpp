#include "Garage/BNGarageZone.h"

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
	constexpr float MaxEntrySpeedKmh = 45.f;
	const FLinearColor GarageBayColor(0.2f, 0.95f, 0.55f);

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
	PillarL = MakeBayPart(*this, RootComponent, TEXT("PillarL"), Cube.Object,
		FVector(0.f, -BayHalfWidth, BayPillarHeight * 0.5f), FVector(0.45f, 0.45f, BayPillarHeight / 100.f));
	PillarR = MakeBayPart(*this, RootComponent, TEXT("PillarR"), Cube.Object,
		FVector(0.f, BayHalfWidth, BayPillarHeight * 0.5f), FVector(0.45f, 0.45f, BayPillarHeight / 100.f));
	BeaconBeam = MakeBayPart(*this, RootComponent, TEXT("BeaconBeam"), Cube.Object,
		FVector(0.f, 0.f, BayPillarHeight + 25.f), FVector(0.75f, (BayHalfWidth * 2.f + 90.f) / 100.f, 0.5f));

	BayLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BayLight"));
	BayLight->SetupAttachment(RootComponent);
	BayLight->SetRelativeLocation(FVector(0.f, 0.f, BayPillarHeight - 20.f));
	BayLight->SetIntensityUnits(ELightUnits::Candelas);
	BayLight->SetIntensity(280.f);
	BayLight->SetLightColor(GarageBayColor);
	BayLight->SetAttenuationRadius(1200.f);
	BayLight->SetCastShadows(false);
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
		bArmed = true;
		return;
	}

	if (!bArmed || Prog->IsGarageOpen() || Distance > TriggerRadius || FMath::Abs(Car->GetSpeedKmh()) >= MaxEntrySpeedKmh)
	{
		return;
	}

	const UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>();
	if (Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		return;
	}

	for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
	{
		if (It->IsRunningFor(Car))
		{
			return;
		}
	}

	bArmed = false;
	Prog->OpenGarage(Car);
}
