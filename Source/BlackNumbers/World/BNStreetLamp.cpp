#include "World/BNStreetLamp.h"

#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	constexpr float WarmUpSeconds = 4.f;
	constexpr float CoolDownSeconds = 1.5f;
	/** Photocells differ: each lamp switches up to this long after the cycle says so. */
	constexpr float MaxSwitchDelaySeconds = 2.5f;
	const FName GlowEmissiveParam(TEXT("Emissive"));

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

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // ticks only while warming up or cooling down
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

	GlowMaterial = Glow->CreateAndSetMaterialInstanceDynamic(0);
	if (GlowMaterial)
	{
		GlowMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(GlowEmissiveParam), GlowEmissive);
	}

	// One cycle per level drives every lamp. At level start lamps are already in their final state.
	TActorIterator<ABNDayNightCycle> Cycle(GetWorld());
	if (Cycle)
	{
		Cycle->OnNightChanged.AddDynamic(this, &ABNStreetLamp::SetLit);
		bLit = Cycle->IsNight();
		Brightness = bLit ? 1.f : 0.f;
	}
	ApplyBrightness();
}

void ABNStreetLamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (SwitchDelay > 0.f)
	{
		SwitchDelay -= DeltaSeconds;
		return;
	}
	Brightness = FMath::FInterpConstantTo(Brightness, bLit ? 1.f : 0.f, DeltaSeconds, 1.f / (bLit ? WarmUpSeconds : CoolDownSeconds));
	ApplyBrightness();
	if (Brightness == (bLit ? 1.f : 0.f))
	{
		SetActorTickEnabled(false);
	}
}

void ABNStreetLamp::ApplyBrightness()
{
	const bool bOn = Brightness > 0.f;
	Light->SetVisibility(bOn);
	Light->SetIntensity(LampCandelas * Brightness);
	Glow->SetVisibility(bOn);
	if (GlowMaterial)
	{
		GlowMaterial->SetVectorParameterValue(GlowEmissiveParam, GlowEmissive * Brightness);
	}
}

void ABNStreetLamp::SetLit(bool bInLit)
{
	if (bInLit == bLit)
	{
		return;
	}
	bLit = bInLit;
	SwitchDelay = FMath::FRandRange(0.f, MaxSwitchDelaySeconds);
	SetActorTickEnabled(true);
}
