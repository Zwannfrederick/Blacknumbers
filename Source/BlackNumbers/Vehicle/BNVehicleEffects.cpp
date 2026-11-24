#include "Vehicle/BNVehicleEffects.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 SmokePoolSize = 28;
	constexpr int32 SkidPoolSize = 56;
	constexpr int32 SparkPoolSize = 20;

	constexpr float TireVfxMinSpeedKmh = 25.f;
	constexpr float TireVfxMinSlipDeg = 14.f;
	constexpr float SmokeEmitIntervalSeconds = 0.055f;
	constexpr float SkidMinSegmentCm = 70.f;
	constexpr float SkidMaxSegmentCm = 900.f;
	constexpr float SkidWidthScaleY = 0.28f;
	/** Rear wheel radius is 40 cm (UBNWheelRear); subtracting 38 cm places strips at Z + 2 cm above asphalt. */
	constexpr float RearWheelGroundOffsetCm = 38.f;
	constexpr float SparkGravityCmPerSec2 = 1400.f;

	const FName ColorParam(TEXT("Color"));
	const FName WheelSocketBL(TEXT("Phys_Wheel_BL"));
	const FName WheelSocketBR(TEXT("Phys_Wheel_BR"));

	const FLinearColor SmokeTint(0.76f, 0.77f, 0.79f, 1.f);
	const FLinearColor DamageSmokeTint(0.14f, 0.14f, 0.16f, 1.f);
	const FLinearColor SkidTint(0.015f, 0.015f, 0.018f, 1.f);
	const FLinearColor NosOuterTint(0.10f, 0.95f, 3.20f, 0.58f);
	const FLinearColor NosCoreTint(1.80f, 3.00f, 4.60f, 0.88f);
	const FLinearColor BackfireOuterTint(3.80f, 1.05f, 0.14f, 0.72f);
	const FLinearColor BackfireCoreTint(5.20f, 3.10f, 0.95f, 0.90f);
	const FLinearColor SparkHotTint(3.2f, 2.0f, 0.55f, 0.96f);
	const FLinearColor SparkCoolTint(1.65f, 0.42f, 0.06f, 0.0f);
}

UBNVehicleEffectsComponent::UBNVehicleEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitTransMat(TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTransMat(TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FallbackMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	SphereMesh = SphereAsset.Object;
	PlaneMesh = PlaneAsset.Object;
	ConeMesh = ConeAsset.Object;
	CubeMesh = CubeAsset.Object;

	UMaterialInterface* DefaultFallback = FallbackMat.Object.Get();
	UnlitTranslucentMaterial = UnlitTransMat.Object ? UnlitTransMat.Object.Get() : DefaultFallback;
	SmokeBaseMaterial = LitTransMat.Object ? LitTransMat.Object.Get() : UnlitTranslucentMaterial.Get();
	SkidBaseMaterial = UnlitTranslucentMaterial;
	FlameBaseMaterial = UnlitTranslucentMaterial;
	SparkBaseMaterial = UnlitTranslucentMaterial;

	SmokeStates.SetNum(SmokePoolSize);
	SkidStates.SetNum(SkidPoolSize);
	SparkStates.SetNum(SparkPoolSize);
}

void UBNVehicleEffectsComponent::BeginPlay()
{
	Super::BeginPlay();
	// Load project VFX materials at runtime (never inside CDO constructor so Editor rebuilds can edit them).
	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_VFX_Smoke")))
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_VFX_Smoke.MI_VFX_Smoke")))
		{
			SmokeBaseMaterial = M;
		}
	}
	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_VFX_Skid")))
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_VFX_Skid.MI_VFX_Skid")))
		{
			SkidBaseMaterial = M;
		}
	}
	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_RaceHolo_Start")))
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_RaceHolo_Start.MI_RaceHolo_Start")))
		{
			FlameBaseMaterial = M;
		}
	}
	else if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_VFX_Flame")))
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_VFX_Flame.MI_VFX_Flame")))
		{
			FlameBaseMaterial = M;
		}
	}
	if (FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_VFX_Spark")))
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/World/Materials/MI_VFX_Spark.MI_VFX_Spark")))
		{
			SparkBaseMaterial = M;
		}
	}
	if (AActor* Owner = GetOwner())
	{
		CachedOwnerSkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	EnsureFlameVisuals();
}

void UBNVehicleEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupVisuals();
	Super::EndPlay(EndPlayReason);
}

void UBNVehicleEffectsComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	CleanupVisuals();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UBNVehicleEffectsComponent::CanCreateVisuals() const
{
	if (IsTemplate() || HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	return Owner != nullptr && World != nullptr && World->IsGameWorld() && Owner->GetRootComponent() != nullptr;
}

UStaticMeshComponent* UBNVehicleEffectsComponent::CreatePooledVisual(UStaticMesh* Mesh, UMaterialInterface* BaseMat,
	bool bWorldAbsolute, UMaterialInstanceDynamic*& OutMid)
{
	OutMid = nullptr;
	if (!CanCreateVisuals() || !Mesh)
	{
		return nullptr;
	}

	AActor* Owner = GetOwner();
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	if (!Comp)
	{
		return nullptr;
	}
	Comp->SetStaticMesh(Mesh);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCastShadow(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->bReceivesDecals = false;
	if (bWorldAbsolute)
	{
		Comp->SetAbsolute(true, true, true);
	}
	Comp->SetupAttachment(this);
	if (BaseMat)
	{
		OutMid = Comp->CreateDynamicMaterialInstance(0, BaseMat);
	}
	Comp->SetVisibility(false, true);
	Comp->RegisterComponent();
	return Comp;
}

void UBNVehicleEffectsComponent::EnsureFlameVisuals()
{
	if (bFlameVisualsInitialized || !CanCreateVisuals() || !PlaneMesh)
	{
		return;
	}
	bFlameVisualsInitialized = true;
	// 2 sleek volumetric 3D jet cones per tailpipe (4 total: slim outer flame + tight bright inner core, no round flare blob).
	UMaterialInterface* ConeMat = FlameBaseMaterial ? FlameBaseMaterial.Get() : UnlitTranslucentMaterial.Get();
	UStaticMesh* MeshToUse = ConeMesh ? ConeMesh.Get() : PlaneMesh.Get();
	FlameMeshes.SetNum(4);
	FlameMids.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		UStaticMeshComponent* Comp = CreatePooledVisual(MeshToUse, ConeMat, false, Mid);
		if (Comp)
		{
			// +90 deg Pitch maps local +Z (Cone apex) straight backward along -X behind the tailpipes!
			Comp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		}
		FlameMeshes[i] = Comp;
		FlameMids[i] = Mid;
	}
}

void UBNVehicleEffectsComponent::EnsureSmokePoolVisuals()
{
	if (bSmokeVisualsInitialized || !CanCreateVisuals() || !PlaneMesh)
	{
		return;
	}
	bSmokeVisualsInitialized = true;
	SmokeMeshes.SetNum(SmokePoolSize);
	SmokeMids.SetNum(SmokePoolSize);
	for (int32 i = 0; i < SmokePoolSize; ++i)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		SmokeMeshes[i] = CreatePooledVisual(PlaneMesh, SmokeBaseMaterial, true, Mid);
		SmokeMids[i] = Mid;
	}
}

void UBNVehicleEffectsComponent::EnsureSkidPoolVisuals()
{
	if (bSkidVisualsInitialized || !CanCreateVisuals() || !PlaneMesh)
	{
		return;
	}
	bSkidVisualsInitialized = true;
	SkidMeshes.SetNum(SkidPoolSize);
	SkidMids.SetNum(SkidPoolSize);
	for (int32 i = 0; i < SkidPoolSize; ++i)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		SkidMeshes[i] = CreatePooledVisual(PlaneMesh, SkidBaseMaterial, true, Mid);
		SkidMids[i] = Mid;
	}
}

void UBNVehicleEffectsComponent::EnsureSparkPoolVisuals()
{
	if (bSparkVisualsInitialized || !CanCreateVisuals() || !PlaneMesh)
	{
		return;
	}
	bSparkVisualsInitialized = true;
	SparkMeshes.SetNum(SparkPoolSize);
	SparkMids.SetNum(SparkPoolSize);
	for (int32 i = 0; i < SparkPoolSize; ++i)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		SparkMeshes[i] = CreatePooledVisual(PlaneMesh, SparkBaseMaterial, true, Mid);
		SparkMids[i] = Mid;
	}
}

void UBNVehicleEffectsComponent::CleanupVisuals()
{
	auto DestroyArray = [](TArray<TObjectPtr<UStaticMeshComponent>>& Array)
	{
		for (TObjectPtr<UStaticMeshComponent>& Comp : Array)
		{
			if (IsValid(Comp))
			{
				Comp->DestroyComponent();
			}
		}
		Array.Reset();
	};

	DestroyArray(SmokeMeshes);
	DestroyArray(SkidMeshes);
	DestroyArray(SparkMeshes);
	DestroyArray(FlameMeshes);

	SmokeMids.Reset();
	SkidMids.Reset();
	SparkMids.Reset();
	FlameMids.Reset();

	bFlameVisualsInitialized = false;
	bSmokeVisualsInitialized = false;
	bSkidVisualsInitialized = false;
	bSparkVisualsInitialized = false;
	bFlameVisible = false;
	ActiveSmokeCount = 0;
	ActiveSkidmarkCount = 0;
	ActiveSparkCount = 0;

	for (FSmokeState& S : SmokeStates) { S.bActive = false; }
	for (FSkidmarkState& S : SkidStates) { S.bActive = false; }
	for (FSparkState& S : SparkStates) { S.bActive = false; }
	bHasLastSkid[0] = false;
	bHasLastSkid[1] = false;
}

void UBNVehicleEffectsComponent::GetRearWheelWorldPos(FVector& OutLeft, FVector& OutRight,
	FVector& OutUp, FVector& OutForward)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutUp = FVector::UpVector;
		OutForward = FVector::ForwardVector;
		OutLeft = FVector(-140.f, -92.f, 40.f);
		OutRight = FVector(-140.f, 92.f, 40.f);
		return;
	}

	OutUp = Owner->GetActorUpVector();
	OutForward = Owner->GetActorForwardVector();
	if (!CachedOwnerSkelMesh.IsValid())
	{
		CachedOwnerSkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (const USkeletalMeshComponent* Skel = CachedOwnerSkelMesh.Get())
	{
		if (Skel->DoesSocketExist(WheelSocketBL) && Skel->DoesSocketExist(WheelSocketBR))
		{
			OutLeft = Skel->GetSocketLocation(WheelSocketBL);
			OutRight = Skel->GetSocketLocation(WheelSocketBR);
			return;
		}
	}

	const FTransform Xform = GetComponentTransform();
	OutLeft = Xform.TransformPosition(FVector(-140.f, -92.f, 40.f));
	OutRight = Xform.TransformPosition(FVector(-140.f, 92.f, 40.f));
}

void UBNVehicleEffectsComponent::UpdateEffects(float DeltaSeconds, float SpeedKmh, float SlipDegrees,
	bool bDrifting, bool bOnGround, bool bNosActive, bool bBackfireActive, float VehicleHealth, bool bEngineBlown)
{
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Clamp(DeltaSeconds, 0.f, 0.25f) : 0.f;
	EffectTime += Dt;

	const float AbsSpeed = FMath::IsFinite(SpeedKmh) ? FMath::Abs(SpeedKmh) : 0.f;
	const float AbsSlip = FMath::IsFinite(SlipDegrees) ? FMath::Abs(SlipDegrees) : 0.f;
	const bool bEmitTireVfx = bOnGround && (bDrifting || AbsSlip > TireVfxMinSlipDeg) && AbsSpeed > TireVfxMinSpeedKmh;

	if (bEmitTireVfx)
	{
		FVector WheelL, WheelR, Up, Forward;
		GetRearWheelWorldPos(WheelL, WheelR, Up, Forward);

		const float SlipIntensity = FMath::Clamp((AbsSlip - 10.f) / 26.f, 0.35f, 1.f);
		const FVector OwnerVel = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;

		SmokeEmitAccum += Dt;
		if (SmokeEmitAccum >= SmokeEmitIntervalSeconds)
		{
			SmokeEmitAccum = FMath::Fmod(SmokeEmitAccum, SmokeEmitIntervalSeconds);
			const FVector WheelPos = (SmokeWheelToggle++ % 2 == 0) ? WheelL : WheelR;
			const FVector PuffOrigin = WheelPos - Up * 22.f - Forward * 20.f;
			SpawnSmokePuff(PuffOrigin, OwnerVel * 0.18f - Forward * 90.f, SlipIntensity, SmokeTint);
		}

		const FVector Patches[2] = {
			WheelL - Up * RearWheelGroundOffsetCm,
			WheelR - Up * RearWheelGroundOffsetCm
		};
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (!bHasLastSkid[Side])
			{
				LastSkidPos[Side] = Patches[Side];
				bHasLastSkid[Side] = true;
				continue;
			}
			const float Dist = FVector::Dist(LastSkidPos[Side], Patches[Side]);
			if (Dist >= SkidMaxSegmentCm)
			{
				LastSkidPos[Side] = Patches[Side];
			}
			else if (Dist >= SkidMinSegmentCm)
			{
				SpawnSkidStrip(LastSkidPos[Side], Patches[Side], SlipIntensity);
				LastSkidPos[Side] = Patches[Side];
			}
		}
	}
	else
	{
		SmokeEmitAccum = 0.f;
		bHasLastSkid[0] = false;
		bHasLastSkid[1] = false;
	}

	// Damaged engine bay smoke (<35% health or blown engine) reusing the pooled smoke cards (0 extra draw overhead).
	if (bEngineBlown || VehicleHealth < 0.35f)
	{
		const float Interval = bEngineBlown ? 0.055f : FMath::Lerp(0.08f, 0.22f, FMath::Clamp(VehicleHealth / 0.35f, 0.f, 1.f));
		DamageSmokeAccum += Dt;
		if (DamageSmokeAccum >= Interval && GetOwner())
		{
			DamageSmokeAccum = FMath::Fmod(DamageSmokeAccum, Interval);
			const FTransform Xf = GetComponentTransform();
			const FVector HoodPos = Xf.TransformPosition(FVector(155.f, FMath::FRandRange(-28.f, 28.f), 84.f));
			const FVector OwnerVel = GetOwner()->GetVelocity() * 0.35f + FVector(0.f, 0.f, bEngineBlown ? 210.f : 140.f);
			SpawnSmokePuff(HoodPos, OwnerVel, bEngineBlown ? 1.0f : 0.72f, DamageSmokeTint);
		}
	}
	else
	{
		DamageSmokeAccum = 0.f;
	}

	// Continuous high-energy exhaust sparks trailing behind tailpipes during active NOS boost!
	if (bNosActive)
	{
		NosSparkAccum += Dt;
		if (NosSparkAccum >= 0.075f)
		{
			NosSparkAccum = FMath::Fmod(NosSparkAccum, 0.075f);
			TriggerExhaustSparks(true);
		}
	}
	else
	{
		NosSparkAccum = 0.f;
	}

	UpdateSmokePool(Dt);
	UpdateSkidPool(Dt);
	UpdateSparkPool(Dt);
	UpdateExhaustFlames(bNosActive, bBackfireActive, AbsSpeed);
}

void UBNVehicleEffectsComponent::SpawnSmokePuff(const FVector& SpawnLoc, const FVector& InheritedVel, float SlipIntensity,
	const FLinearColor& CustomTint)
{
	EnsureSmokePoolVisuals();

	const int32 Idx = NextSmokeIndex;
	NextSmokeIndex = (NextSmokeIndex + 1) % SmokePoolSize;

	FSmokeState& State = SmokeStates[Idx];
	if (!State.bActive)
	{
		State.bActive = true;
		++ActiveSmokeCount;
	}
	State.Age = 0.f;
	State.Lifetime = FMath::FRandRange(0.82f, 1.05f);
	State.Location = SpawnLoc + FVector(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-12.f, 12.f), 8.f);
	State.Velocity = InheritedVel + FVector(FMath::FRandRange(-55.f, 55.f), FMath::FRandRange(-55.f, 55.f),
		FMath::FRandRange(95.f, 145.f));
	State.Tint = CustomTint;
	State.StartScale = FMath::FRandRange(0.45f, 0.62f);
	State.EndScale = FMath::FRandRange(1.75f, 2.35f) * (0.85f + 0.25f * SlipIntensity);
	State.BaseOpacity = FMath::Lerp(0.32f, 0.56f, SlipIntensity);
	State.SpinAngleDeg = FMath::FRandRange(0.f, 360.f);
	State.SpinRateDeg = FMath::FRandRange(-75.f, 75.f);

	if (SmokeMeshes.IsValidIndex(Idx) && IsValid(SmokeMeshes[Idx]))
	{
		SmokeMeshes[Idx]->SetWorldLocation(State.Location);
		SmokeMeshes[Idx]->SetWorldScale3D(FVector(State.StartScale, State.StartScale, 1.f));
		SmokeMeshes[Idx]->SetVisibility(true, true);
		if (SmokeMids.IsValidIndex(Idx) && IsValid(SmokeMids[Idx]))
		{
			SmokeMids[Idx]->SetVectorParameterValue(ColorParam,
				FLinearColor(State.Tint.R, State.Tint.G, State.Tint.B, State.BaseOpacity * 0.4f));
		}
	}
}

void UBNVehicleEffectsComponent::SpawnSkidStrip(const FVector& PrevLoc, const FVector& CurrLoc, float SlipIntensity)
{
	EnsureSkidPoolVisuals();

	const int32 Idx = NextSkidIndex;
	NextSkidIndex = (NextSkidIndex + 1) % SkidPoolSize;

	FSkidmarkState& State = SkidStates[Idx];
	if (!State.bActive)
	{
		State.bActive = true;
		++ActiveSkidmarkCount;
	}
	State.Age = 0.f;
	State.Lifetime = 12.f;
	State.BaseOpacity = FMath::Lerp(0.55f, 0.78f, SlipIntensity);

	if (SkidMeshes.IsValidIndex(Idx) && IsValid(SkidMeshes[Idx]))
	{
		const FVector Delta = CurrLoc - PrevLoc;
		const float Length = Delta.Size();
		FVector Midpoint = 0.5f * (PrevLoc + CurrLoc);
		FVector SurfaceNormal = FVector::UpVector;

		if (const UWorld* World = GetWorld())
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BNSkidTrace), false, GetOwner());
			if (World->LineTraceSingleByChannel(Hit, Midpoint + FVector(0.f, 0.f, 55.f),
				Midpoint - FVector(0.f, 0.f, 90.f), ECC_WorldStatic, Params))
			{
				SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
				Midpoint = Hit.ImpactPoint + SurfaceNormal * 2.5f;
			}
		}

		const FVector Forward = FVector::VectorPlaneProject(Delta, SurfaceNormal).GetSafeNormal();
		const FRotator Rot = Forward.IsNearlyZero()
			? Delta.Rotation()
			: FRotationMatrix::MakeFromXZ(Forward, SurfaceNormal).Rotator();

		SkidMeshes[Idx]->SetWorldLocationAndRotation(Midpoint, Rot);
		SkidMeshes[Idx]->SetWorldScale3D(FVector((Length + 12.f) / 100.f, SkidWidthScaleY, 1.f));
		SkidMeshes[Idx]->SetVisibility(true, true);
		if (SkidMids.IsValidIndex(Idx) && IsValid(SkidMids[Idx]))
		{
			SkidMids[Idx]->SetVectorParameterValue(ColorParam,
				FLinearColor(SkidTint.R, SkidTint.G, SkidTint.B, State.BaseOpacity));
		}
	}
}

void UBNVehicleEffectsComponent::TriggerCollisionSparks(const FVector& WorldLocation,
	const FVector& ImpactNormal, float Intensity)
{
	if (!FMath::IsFinite(Intensity) || Intensity <= 0.f)
	{
		return;
	}
	EnsureSparkPoolVisuals();

	// Accept either normalized [0..1.5] intensity or raw Chaos impulse magnitudes (>80000).
	const float NormIntensity = Intensity > 10.f
		? FMath::Clamp(Intensity / 250000.f, 0.3f, 1.4f)
		: FMath::Clamp(Intensity, 0.25f, 1.4f);

	const FVector BaseDir = ImpactNormal.IsNearlyZero() ? FVector::UpVector : ImpactNormal.GetSafeNormal();
	const int32 BurstCount = FMath::Clamp(FMath::RoundToInt(8.f + 4.f * NormIntensity), 8, SparkPoolSize);

	for (int32 i = 0; i < BurstCount; ++i)
	{
		const int32 Idx = NextSparkIndex;
		NextSparkIndex = (NextSparkIndex + 1) % SparkPoolSize;

		FSparkState& State = SparkStates[Idx];
		if (!State.bActive)
		{
			State.bActive = true;
			++ActiveSparkCount;
		}
		const FVector ConeDir = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(52.f));
		const float Speed = FMath::FRandRange(480.f, 1080.f) * (0.72f + 0.35f * NormIntensity);

		State.Age = 0.f;
		State.Lifetime = FMath::FRandRange(0.28f, 0.52f);
		State.Location = WorldLocation + BaseDir * 12.f;
		State.Velocity = ConeDir * Speed + FVector(0.f, 0.f, FMath::FRandRange(120.f, 320.f));

		if (SparkMeshes.IsValidIndex(Idx) && IsValid(SparkMeshes[Idx]))
		{
			SparkMeshes[Idx]->SetWorldLocationAndRotation(State.Location, State.Velocity.Rotation());
			SparkMeshes[Idx]->SetWorldScale3D(FVector(0.36f, 0.025f, 0.025f));
			SparkMeshes[Idx]->SetVisibility(true, true);
			if (SparkMids.IsValidIndex(Idx) && IsValid(SparkMids[Idx]))
			{
				SparkMids[Idx]->SetVectorParameterValue(ColorParam, SparkHotTint);
			}
		}
	}
}

void UBNVehicleEffectsComponent::UpdateSmokePool(float DeltaSeconds)
{
	if (ActiveSmokeCount <= 0)
	{
		return;
	}
	const float DragFactor = FMath::Exp(-1.35f * DeltaSeconds);
	FVector CamLoc = GetComponentLocation() + FVector(0.f, 0.f, 200.f);
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				CamLoc = CamMgr->GetCameraLocation();
			}
		}
	}

	for (int32 i = 0; i < SmokePoolSize; ++i)
	{
		FSmokeState& State = SmokeStates[i];
		if (!State.bActive)
		{
			continue;
		}
		State.Age += DeltaSeconds;
		if (State.Age >= State.Lifetime)
		{
			State.bActive = false;
			ActiveSmokeCount = FMath::Max(0, ActiveSmokeCount - 1);
			if (SmokeMeshes.IsValidIndex(i) && IsValid(SmokeMeshes[i]))
			{
				SmokeMeshes[i]->SetVisibility(false, true);
			}
			continue;
		}

		const float Frac = State.Age / FMath::Max(State.Lifetime, KINDA_SMALL_NUMBER);
		State.Velocity.X *= DragFactor;
		State.Velocity.Y *= DragFactor;
		State.Location += State.Velocity * DeltaSeconds;
		State.SpinAngleDeg += State.SpinRateDeg * DeltaSeconds;

		if (SmokeMeshes.IsValidIndex(i) && IsValid(SmokeMeshes[i]))
		{
			const float Scale = FMath::Lerp(State.StartScale, State.EndScale, FMath::Pow(Frac, 0.68f));
			const float FadeIn = FMath::Clamp(Frac / 0.14f, 0.f, 1.f);
			const float FadeOut = FMath::Square(1.f - Frac);
			const float Opacity = State.BaseOpacity * FadeIn * FadeOut;

			// Billboard Plane's +Z normal directly toward active camera with gentle roll spin.
			const FVector ToCam = (CamLoc - State.Location).GetSafeNormal();
			const FQuat BaseRot = FRotationMatrix::MakeFromZ(ToCam.IsNearlyZero() ? FVector::UpVector : ToCam).ToQuat();
			const FQuat SpinRot(ToCam.IsNearlyZero() ? FVector::UpVector : ToCam, FMath::DegreesToRadians(State.SpinAngleDeg));
			SmokeMeshes[i]->SetWorldLocationAndRotation(State.Location, (SpinRot * BaseRot).Rotator());
			SmokeMeshes[i]->SetWorldScale3D(FVector(Scale, Scale, 1.f));
			if (SmokeMids.IsValidIndex(i) && IsValid(SmokeMids[i]))
			{
				SmokeMids[i]->SetVectorParameterValue(ColorParam,
					FLinearColor(State.Tint.R, State.Tint.G, State.Tint.B, Opacity));
			}
		}
	}
}

void UBNVehicleEffectsComponent::UpdateSkidPool(float DeltaSeconds)
{
	if (ActiveSkidmarkCount <= 0)
	{
		return;
	}
	for (int32 i = 0; i < SkidPoolSize; ++i)
	{
		FSkidmarkState& State = SkidStates[i];
		if (!State.bActive)
		{
			continue;
		}
		State.Age += DeltaSeconds;
		if (State.Age >= State.Lifetime)
		{
			State.bActive = false;
			ActiveSkidmarkCount = FMath::Max(0, ActiveSkidmarkCount - 1);
			if (SkidMeshes.IsValidIndex(i) && IsValid(SkidMeshes[i]))
			{
				SkidMeshes[i]->SetVisibility(false, true);
			}
			continue;
		}

		// Stay crisp for first half of lifetime, then fade smoothly over the last 6 seconds.
		if (SkidMids.IsValidIndex(i) && IsValid(SkidMids[i]))
		{
			const float Remaining = State.Lifetime - State.Age;
			const float Fade = FMath::Clamp(Remaining / (State.Lifetime * 0.5f), 0.f, 1.f);
			SkidMids[i]->SetVectorParameterValue(ColorParam,
				FLinearColor(SkidTint.R, SkidTint.G, SkidTint.B, State.BaseOpacity * Fade));
		}
	}
}

void UBNVehicleEffectsComponent::UpdateSparkPool(float DeltaSeconds)
{
	if (ActiveSparkCount <= 0)
	{
		return;
	}
	for (int32 i = 0; i < SparkPoolSize; ++i)
	{
		FSparkState& State = SparkStates[i];
		if (!State.bActive)
		{
			continue;
		}
		State.Age += DeltaSeconds;
		if (State.Age >= State.Lifetime)
		{
			State.bActive = false;
			ActiveSparkCount = FMath::Max(0, ActiveSparkCount - 1);
			if (SparkMeshes.IsValidIndex(i) && IsValid(SparkMeshes[i]))
			{
				SparkMeshes[i]->SetVisibility(false, true);
			}
			continue;
		}

		State.Velocity.Z -= SparkGravityCmPerSec2 * DeltaSeconds;
		State.Location += State.Velocity * DeltaSeconds;

		if (SparkMeshes.IsValidIndex(i) && IsValid(SparkMeshes[i]))
		{
			const float Frac = State.Age / FMath::Max(State.Lifetime, KINDA_SMALL_NUMBER);
			const float SpeedStretch = FMath::Clamp(State.Velocity.Size() / 900.f, 0.2f, 1.1f);
			SparkMeshes[i]->SetWorldLocationAndRotation(State.Location, State.Velocity.Rotation());
			SparkMeshes[i]->SetWorldScale3D(FVector(0.38f * SpeedStretch * (1.f - 0.4f * Frac), 0.024f, 0.024f));
			if (SparkMids.IsValidIndex(i) && IsValid(SparkMids[i]))
			{
				const FLinearColor Color = FMath::Lerp(SparkHotTint, SparkCoolTint, Frac);
				SparkMids[i]->SetVectorParameterValue(ColorParam, Color);
			}
		}
	}
}

void UBNVehicleEffectsComponent::TriggerExhaustSparks(bool bNosCyan)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	EnsureSparkPoolVisuals();
	const FTransform Xf = GetComponentTransform();
	const FVector BackDir = -Owner->GetActorForwardVector();
	const FVector OwnerVel = Owner->GetVelocity();
	const int32 Count = bNosCyan ? 4 : 6;

	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Idx = NextSparkIndex;
		NextSparkIndex = (NextSparkIndex + 1) % SparkPoolSize;

		FSparkState& State = SparkStates[Idx];
		if (!State.bActive)
		{
			State.bActive = true;
			++ActiveSparkCount;
		}
		const float Side = (i % 2 == 0) ? -62.f : 62.f;
		const FVector Nozzle = Xf.TransformPosition(FVector(-262.f, Side, 48.f));
		const FVector ConeDir = FMath::VRandCone(BackDir, FMath::DegreesToRadians(bNosCyan ? 14.f : 24.f));

		State.Age = 0.f;
		State.Lifetime = FMath::FRandRange(0.16f, 0.30f);
		State.Location = Nozzle;
		State.Velocity = OwnerVel * 0.25f + ConeDir * FMath::FRandRange(750.f, 1450.f) + FVector(0.f, 0.f, FMath::FRandRange(20.f, 110.f));

		if (SparkMeshes.IsValidIndex(Idx) && IsValid(SparkMeshes[Idx]))
		{
			SparkMeshes[Idx]->SetWorldLocationAndRotation(State.Location, State.Velocity.Rotation());
			SparkMeshes[Idx]->SetWorldScale3D(FVector(0.45f, 0.028f, 0.028f));
			SparkMeshes[Idx]->SetVisibility(true, true);
			if (SparkMids.IsValidIndex(Idx) && IsValid(SparkMids[Idx]))
			{
				SparkMids[Idx]->SetVectorParameterValue(ColorParam,
					bNosCyan ? FLinearColor(0.85f, 2.8f, 5.2f, 0.98f) : SparkHotTint);
			}
		}
	}
}

void UBNVehicleEffectsComponent::UpdateExhaustFlames(bool bNosActive, bool bBackfireActive, float SpeedKmh)
{
	const bool bShow = bNosActive || bBackfireActive;
	bFlameVisible = bShow;

	if (!bShow)
	{
		for (TObjectPtr<UStaticMeshComponent>& Cone : FlameMeshes)
		{
			if (IsValid(Cone) && Cone->IsVisible())
			{
				Cone->SetVisibility(false, true);
			}
		}
		return;
	}

	EnsureFlameVisuals();
	const FLinearColor OuterCol = bNosActive ? NosOuterTint : BackfireOuterTint;
	const FLinearColor CoreCol = bNosActive ? NosCoreTint : BackfireCoreTint;
	const float SpeedStretch = FMath::GetMappedRangeValueClamped(FVector2f(40.f, 260.f), FVector2f(0.90f, 1.22f), SpeedKmh);

	for (int32 i = 0; i < FlameMeshes.Num(); ++i)
	{
		UStaticMeshComponent* Comp = FlameMeshes[i];
		if (!IsValid(Comp))
		{
			continue;
		}
		const float Side = (i < 2) ? -1.f : 1.f;
		const bool bCore = (i % 2) == 1;
		const float Flicker = 0.86f
			+ 0.14f * FMath::Sin(EffectTime * 78.f + Side * 1.9f + (bCore ? 0.7f : 0.f))
			+ 0.08f * FMath::Sin(EffectTime * 155.f + i * 0.8f);

		const float LenZ = (bNosActive ? (bCore ? 0.44f : 0.78f) * SpeedStretch : (bCore ? 0.34f : 0.58f)) * Flicker;
		const float RadXY = (bNosActive ? (bCore ? 0.075f : 0.145f) : (bCore ? 0.095f : 0.18f)) * (0.92f + 0.10f * Flicker);
		// Cone spans -50..+50 cm along local Z; pitched +90 deg maps base (-Z) flush inside exhaust tip (-252 cm) and tip (+Z) backward (-X).
		Comp->SetRelativeLocation(FVector(-252.f - 46.f * LenZ, Side * 62.f, 48.f));
		Comp->SetRelativeScale3D(FVector(RadXY, RadXY, LenZ));
		Comp->SetVisibility(true, true);

		if (FlameMids.IsValidIndex(i) && IsValid(FlameMids[i]))
		{
			const FLinearColor& Picked = bCore ? CoreCol : OuterCol;
			FLinearColor Color = Picked * FMath::Clamp(Flicker, 0.86f, 1.20f);
			Color.A = Picked.A;
			FlameMids[i]->SetVectorParameterValue(ColorParam, Color);
		}
	}
}
