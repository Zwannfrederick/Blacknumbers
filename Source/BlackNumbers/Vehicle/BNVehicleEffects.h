#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BNVehicleEffects.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Pooled, low-draw-call gameplay VFX attached to ABNVehiclePawn:
 * - Drift tire smoke puffs rising and expanding behind rear tires
 * - Fading dark asphalt skidmark strips (Z+2 cm above road surface)
 * - Dual 3D tapered exhaust flame cones for NOS (cyan-blue) and backfire (amber-orange)
 * - Impact spark streaks flying off collision normals with gravity
 *
 * Designed for 4 GB VRAM hardware (zero per-frame SpawnActor / NewObject overhead)
 * and 100% safe when Owner/World/Mesh is null or under headless -nullrhi tests.
 */
UCLASS(ClassGroup=(BlackNumbers))
class BLACKNUMBERS_API UBNVehicleEffectsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBNVehicleEffectsComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Driven each tick by ABNVehiclePawn with current kinematics, boost state, and chassis health. */
	void UpdateEffects(float DeltaSeconds, float SpeedKmh, float SlipDegrees, bool bDrifting,
		bool bOnGround, bool bNosActive, bool bBackfireActive, float VehicleHealth = 1.f, bool bEngineBlown = false);

	/** Spawns a pooled burst of bright streaks from WorldLocation along ImpactNormal. */
	void TriggerCollisionSparks(const FVector& WorldLocation, const FVector& ImpactNormal, float Intensity);

	/** Fires a punchy stream of glowing exhaust sparks/embers backward from both tailpipes on NOS ignition or shift pop. */
	void TriggerExhaustSparks(bool bNosCyan);

	UFUNCTION(BlueprintPure, Category = "Effects")
	int32 GetActiveSmokeCount() const { return ActiveSmokeCount; }

	UFUNCTION(BlueprintPure, Category = "Effects")
	int32 GetActiveSkidmarkCount() const { return ActiveSkidmarkCount; }

	UFUNCTION(BlueprintPure, Category = "Effects")
	int32 GetActiveSparkCount() const { return ActiveSparkCount; }

	UFUNCTION(BlueprintPure, Category = "Effects")
	bool IsFlameVisible() const { return bFlameVisible; }

private:
	struct FSmokeState
	{
		bool bActive = false;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FLinearColor Tint = FLinearColor(0.76f, 0.77f, 0.79f, 1.f);
		float Age = 0.f;
		float Lifetime = 0.9f;
		float StartScale = 0.32f;
		float EndScale = 1.45f;
		float BaseOpacity = 0.46f;
		float SpinAngleDeg = 0.f;
		float SpinRateDeg = 0.f;
	};

	struct FSkidmarkState
	{
		bool bActive = false;
		float Age = 0.f;
		float Lifetime = 12.f;
		float BaseOpacity = 0.65f;
	};

	struct FSparkState
	{
		bool bActive = false;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Lifetime = 0.4f;
	};

	bool CanCreateVisuals() const;
	UStaticMeshComponent* CreatePooledVisual(UStaticMesh* Mesh, UMaterialInterface* BaseMat,
		bool bWorldAbsolute, UMaterialInstanceDynamic*& OutMid);
	void EnsureFlameVisuals();
	void EnsureSmokePoolVisuals();
	void EnsureSkidPoolVisuals();
	void EnsureSparkPoolVisuals();
	void CleanupVisuals();

	void GetRearWheelWorldPos(FVector& OutLeft, FVector& OutRight, FVector& OutUp, FVector& OutForward);
	void SpawnSmokePuff(const FVector& SpawnLoc, const FVector& InheritedVel, float SlipIntensity,
		const FLinearColor& CustomTint = FLinearColor(0.76f, 0.77f, 0.79f, 1.f));
	void SpawnSkidStrip(const FVector& PrevLoc, const FVector& CurrLoc, float SlipIntensity);
	void UpdateSmokePool(float DeltaSeconds);
	void UpdateSkidPool(float DeltaSeconds);
	void UpdateSparkPool(float DeltaSeconds);
	void UpdateExhaustFlames(bool bNosActive, bool bBackfireActive, float SpeedKmh);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SmokeBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SkidBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FlameBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SparkBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> UnlitTranslucentMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SmokeMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SmokeMids;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SkidMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SkidMids;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SparkMids;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FlameMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlameMids;

	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerSkelMesh;

	TArray<FSmokeState> SmokeStates;
	TArray<FSkidmarkState> SkidStates;
	TArray<FSparkState> SparkStates;

	int32 NextSmokeIndex = 0;
	float DamageSmokeAccum = 0.f;
	float NosSparkAccum = 0.f;
	int32 NextSkidIndex = 0;
	int32 NextSparkIndex = 0;
	int32 ActiveSmokeCount = 0;
	int32 ActiveSkidmarkCount = 0;
	int32 ActiveSparkCount = 0;

	bool bFlameVisible = false;
	bool bFlameVisualsInitialized = false;
	bool bSmokeVisualsInitialized = false;
	bool bSkidVisualsInitialized = false;
	bool bSparkVisualsInitialized = false;

	float EffectTime = 0.f;
	float SmokeEmitAccum = 0.f;
	int32 SmokeWheelToggle = 0;
	bool bHasLastSkid[2] = { false, false };
	FVector LastSkidPos[2] = { FVector::ZeroVector, FVector::ZeroVector };
};
