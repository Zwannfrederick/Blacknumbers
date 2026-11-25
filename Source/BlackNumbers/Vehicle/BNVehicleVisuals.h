#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Garage/BNGarageTypes.h"
#include "BNVehicleVisuals.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Manages vehicle body style meshes (StreetCoupe / RallyOffroad / HyperConcept),
 * procedural 3D aero bodykits, and ClearCoat/Metallic/Carbon paint material instances
 * without altering the Chaos wheel rig or wheel alignment.
 */
UCLASS(ClassGroup = (BlackNumbers), meta = (BlueprintSpawnableComponent))
class BLACKNUMBERS_API UBNVehicleVisualsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBNVehicleVisualsComponent();

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Swaps body mesh (with safe fallback), builds aero kit parts, and applies ClearCoat paint. */
	void ApplyCarVisuals(const FBNVehicleSpec& Spec, UStaticMeshComponent* ChassisComp,
		UStaticMeshComponent* GlassComp, const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes);

	/** Applies paint color on the current chassis while keeping current metallic/clearcoat values. */
	void ApplyPaintTint(const FLinearColor& PaintTint, UStaticMeshComponent* ChassisComp);

	/** Applies a named garage finish preset (Gloss/Metallic/Satin/MatteCarbon) keeping current paint tint. */
	void ApplyPaintFinishPreset(EBNPaintFinish Finish, UStaticMeshComponent* ChassisComp);

	/** Applies full paint tint + metallic + clearcoat finish to the current chassis paint slot. */
	void ApplyPaintFinish(const FLinearColor& PaintTint, float Metallic, float ClearCoat, UStaticMeshComponent* ChassisComp);

	/** Tints the alloy rim slot (never rubber tires or wheel transforms) across all 4 wheel meshes. */
	void ApplyRimPaint(const FLinearColor& RimTint, const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes);

	/** Updates pulsing cyan NOS or amber backfire exhaust point lights. */
	static void UpdateExhaustLights(const TArray<TObjectPtr<UPointLightComponent>>& ExhaustLights,
		bool bNosActive, bool bBackfireActive, float RealTimeSeconds);

	/** Updates emissive front DRL/headlight and dynamic rear brake/taillight strips. */
	void UpdateLightStrips(bool bHeadlightsOn, bool bBraking);

	EBNBodyStyle GetAppliedBodyStyle() const { return AppliedBodyStyle; }
	EBNAeroKit GetAppliedAeroKit() const { return AppliedAeroKit; }
	int32 GetAeroPartCount() const { return AeroParts.Num(); }
	bool IsUsingCustomBodyMesh() const { return bUsingCustomBodyMesh; }

private:
	UStaticMesh* ResolveBodyMesh(EBNCarId CarId, EBNBodyStyle Style, bool& bOutIsCustom);
	UStaticMesh* LoadOptionalMesh(const TCHAR* PackagePath, const TCHAR* AssetPath);
	void ConfigureChassisTransform(UStaticMeshComponent* ChassisComp, UStaticMesh* Mesh, EBNBodyStyle Style,
		bool bIsCustom, UStaticMeshComponent* GlassComp, const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes,
		float& OutBodyTopZ);
	void EnsureAeroAssets(const FBNVehicleSpec& Spec);
	void BindBodyLightsSlot(UStaticMeshComponent* ChassisComp);
	void RebuildAeroKit(const FBNVehicleSpec& Spec, float BodyTopZ);
	void ClearAeroParts();
	UStaticMeshComponent* AddAeroPart(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc,
		const FRotator& Rot, const FVector& Scale, UMaterialInterface* Mat, bool bCastShadow = true);

	EBNBodyStyle AppliedBodyStyle = EBNBodyStyle::StreetCoupe;
	EBNAeroKit AppliedAeroKit = EBNAeroKit::StreetLip;
	bool bHasBuiltAeroKit = false;
	bool bUsingCustomBodyMesh = false;
	FLinearColor LastPaintTint = FLinearColor(0.22f, 0.24f, 0.27f, 1.f);
	float LastMetallic = 0.72f;
	float LastClearCoat = 0.85f;
	float LastRoughnessOverride = -1.f;
	float LastBodyTopZ = 128.f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultSportsCarMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AeroBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CarbonBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RimBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyPaintMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyLightsMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AeroCarbonMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AeroAccentMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExhaustChromeMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeadlightStripMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrakeLightStripMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WheelRimMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> AeroParts;
};
