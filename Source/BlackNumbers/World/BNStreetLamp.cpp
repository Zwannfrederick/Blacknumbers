#include "World/BNStreetLamp.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "World/BNDayNightCycle.h"

namespace
{
	constexpr float PoleHeight = 800.f;
	constexpr float ArmReach = 160.f;
	/** Warm low-pressure-sodium orange; leaves the cool night grade room to contrast. */
	const FLinearColor SodiumColor(1.f, 0.55f, 0.2f);
	constexpr float LampCandelas = 3000.f;
	constexpr float LampRadius = 2500.f;

	UStaticMeshComponent* MakePart(AActor& Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
		const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Part;
	}
}

ABNStreetLamp::ABNStreetLamp()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Basic shapes are 100 cm with a centred pivot.
	Pole = MakePart(*this, RootComponent, TEXT("Pole"), Cylinder.Object,
		FVector(0.f, 0.f, PoleHeight * 0.5f), FVector(0.15f, 0.15f, PoleHeight / 100.f));
	Pole->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); // cars can hit it
	Arm = MakePart(*this, RootComponent, TEXT("Arm"), Cube.Object,
		FVector(ArmReach * 0.5f, 0.f, PoleHeight - 10.f), FVector(ArmReach / 100.f + 0.2f, 0.12f, 0.12f));
	Glow = MakePart(*this, RootComponent, TEXT("Glow"), Cube.Object,
		FVector(ArmReach, 0.f, PoleHeight - 18.f), FVector(0.5f, 0.25f, 0.03f));
	Glow->SetCastShadow(false);
	Glow->SetVisibility(false);

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(ArmReach, 0.f, PoleHeight - 25.f));
	Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(LampCandelas);
	Light->SetLightColor(SodiumColor);
	Light->SetAttenuationRadius(LampRadius);
	Light->SetInnerConeAngle(40.f);
	Light->SetOuterConeAngle(70.f);
	Light->SetCastShadows(false); // ponytail: ~26 lamps per street; enable on hero lamps if night shadows are wanted
	Light->SetVisibility(false);
}

void ABNStreetLamp::BeginPlay()
{
	Super::BeginPlay();

	// One cycle per level drives every lamp.
	TActorIterator<ABNDayNightCycle> Cycle(GetWorld());
	if (Cycle)
	{
		Cycle->OnNightChanged.AddDynamic(this, &ABNStreetLamp::SetLit);
		SetLit(Cycle->IsNight());
	}
}

void ABNStreetLamp::SetLit(bool bLit)
{
	Light->SetVisibility(bLit);
	Glow->SetVisibility(bLit);
}

bool ABNStreetLamp::IsLit() const
{
	return Light->IsVisible();
}
