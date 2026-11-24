#include "World/BNStreetLamp.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Police/BNPursuitSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehiclePawn.h"
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
	constexpr float KnockDurationSeconds = 0.55f;
	constexpr float KnockMinSpeedKmh = 20.f;
	constexpr float KnockResetSeconds = 25.f;
	constexpr float KnockResetMinDistCm = 7500.f;
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

	// Visual pole stays NoCollision so L-shaped overhead arm never generates a convex launch ramp;
	// slim HitTrigger snaps the lamp down on impact like Most Wanted 2005!
	Pole = MakePart(*this, RootComponent, TEXT("Pole"), Cylinder.Object,
		FVector(0.f, 0.f, PoleHeight * 0.5f), FVector(0.15f, 0.15f, PoleHeight / 100.f));
	Arm = MakePart(*this, RootComponent, TEXT("Arm"), Cube.Object,
		FVector(ArmReach * 0.5f, 0.f, PoleHeight - 10.f), FVector(ArmReach / 100.f + 0.2f, 0.12f, 0.12f));
	Glow = MakePart(*this, RootComponent, TEXT("Glow"), Cube.Object,
		FVector(ArmReach, 0.f, PoleHeight - 18.f), FVector(0.5f, 0.25f, 0.03f));
	Glow->SetCastShadow(false);
	Glow->SetVisibility(false);

	HitTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("HitTrigger"));
	HitTrigger->SetupAttachment(RootComponent);
	HitTrigger->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	HitTrigger->SetBoxExtent(FVector(34.f, 34.f, 150.f));
	HitTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	HitTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABNStreetLamp::OnPoleOverlap);

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
	Light->MaxDrawDistance = 18000.f;
	Light->MaxDistanceFadeRange = 3500.f;
	Light->SetCastShadows(false); // ponytail: ~110 lamps citywide; 180m cull keeps night RenderThread fast
	Light->SetVisibility(false);
}

void ABNStreetLamp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (HasAnyFlags(RF_ClassDefaultObject) || !Pole)
	{
		return;
	}
	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Meshes/SM_BN_StreetLamp")))
	{
		if (UStaticMesh* LampMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/World/Meshes/SM_BN_StreetLamp.SM_BN_StreetLamp")))
		{
			Pole->SetStaticMesh(LampMesh);
			Pole->SetRelativeLocation(FVector::ZeroVector);
			Pole->SetRelativeScale3D(FVector::OneVector);
			if (Arm)
			{
				Arm->SetVisibility(false);
			}
			if (Glow)
			{
				Glow->SetRelativeLocation(FVector(186.f, 0.f, 793.f));
				Glow->SetRelativeScale3D(FVector(0.56f, 0.24f, 0.04f));
			}
			if (Light)
			{
				Light->SetRelativeLocation(FVector(186.f, 0.f, PoleHeight - 25.f));
			}
		}
	}
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

void ABNStreetLamp::OnPoleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bKnockedDown)
	{
		return;
	}
	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(OtherActor);
	if (!Car || FMath::Abs(Car->GetSpeedKmh()) < KnockMinSpeedKmh)
	{
		return;
	}
	FVector Dir = Car->GetVelocity().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = Car->GetActorForwardVector().GetSafeNormal2D();
	}
	KnockDown(Dir, Car);
}

void ABNStreetLamp::KnockDown(const FVector& ImpactDirWorld, ABNVehiclePawn* HitCar)
{
	bKnockedDown = true;
	KnockTimer = 0.f;
	ResetTimer = 0.f;

	// Compute local topple pitch/roll away from car travel direction
	const FVector LocalDir = GetActorTransform().InverseTransformVectorNoScale(ImpactDirWorld).GetSafeNormal2D();
	KnockTargetRot = FRotator(-LocalDir.X * 86.f, 0.f, LocalDir.Y * 86.f);
	ApplyBrightness();
	SetActorTickInterval(0.f);
	SetActorTickEnabled(true);

	if (HitCar)
	{
		HitCar->AddCameraImpulse(0.35f);
		if (UBNVehicleEffectsComponent* Fx = HitCar->FindComponentByClass<UBNVehicleEffectsComponent>())
		{
			Fx->TriggerCollisionSparks(GetActorLocation() + FVector(0.f, 0.f, 110.f), -ImpactDirWorld, 0.85f);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
		{
			Audio->PlaySfxAtLocation(EBNSfxId::BreakMetal, GetActorLocation(), 0.78f);
		}
		if (HitCar && HitCar->IsPlayerControlled())
		{
			if (UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>())
			{
				Pursuit->ReportCollateralDamage(1500, TEXT("Sokak Lambası"));
			}
		}
	}
}

void ABNStreetLamp::ResetLamp()
{
	bKnockedDown = false;
	KnockTimer = 0.f;
	ResetTimer = 0.f;
	if (Pole)
	{
		Pole->SetRelativeRotation(FRotator::ZeroRotator);
	}
	ApplyBrightness();
	if (Brightness == (bLit ? 1.f : 0.f))
	{
		SetActorTickEnabled(false);
	}
}

void ABNStreetLamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bKnockedDown)
	{
		if (KnockTimer < KnockDurationSeconds)
		{
			KnockTimer = FMath::Min(KnockTimer + DeltaSeconds, KnockDurationSeconds);
			const float Alpha = KnockTimer / KnockDurationSeconds;
			const float Eased = Alpha * Alpha;
			if (Pole)
			{
				Pole->SetRelativeRotation(KnockTargetRot * Eased);
			}
		}
		else
		{
			SetActorTickInterval(0.5f);
			ResetTimer += DeltaSeconds;
			if (ResetTimer >= KnockResetSeconds)
			{
				const UWorld* World = GetWorld();
				const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
				const APawn* Player = PC ? PC->GetPawn() : nullptr;
				if (!Player || FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) > KnockResetMinDistCm)
				{
					SetActorTickInterval(0.f);
					ResetLamp();
				}
			}
		}
		return;
	}

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
	const bool bOn = !bKnockedDown && Brightness > 0.f;
	Light->SetVisibility(bOn);
	Light->SetIntensity(LampCandelas * Brightness);
	Glow->SetVisibility(bOn);
	if (GlowMaterial)
	{
		GlowMaterial->SetVectorParameterValue(GlowEmissiveParam, GlowEmissive * (bOn ? Brightness : 0.f));
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
