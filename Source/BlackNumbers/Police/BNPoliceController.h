#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "Police/BNPursuitLogic.h"
#include "BNPoliceController.generated.h"

class ABNVehiclePawn;
class UBNPoliceSirenComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * AI driver and police rig controller for Karamara interceptor units.
 * Attaches roof lightbar, alternating red/blue strobes, and 3D procedural siren onto the possessed pawn
 * without touching ABNVehiclePawn::CarLights.
 */
UCLASS()
class BLACKNUMBERS_API ABNPoliceController : public AController
{
	GENERATED_BODY()

public:
	ABNPoliceController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetupPursuitUnit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec, int32 InFormationSlot, bool bInRoadblockUnit = false);
	void UpdateHeatSpec(const FBNHeatTierSpec& InSpec);
	void SetSearchMode(bool bInSearchMode, const FVector& InSearchPoint);
	void DisableUnit();

	bool IsUnitDisabled() const { return bDisabled; }
	bool IsRoadblockUnit() const { return bRoadblockUnit; }
	bool IsReadyForCleanup() const { return bDisabled && DisabledSeconds >= 8.f; }
	bool HasVisualLightbar() const { return RedLight != nullptr && BlueLight != nullptr; }
	bool IsSirenActive() const;

private:
	void AttachPoliceGear(ABNVehiclePawn& Car);
	void CleanupPoliceGear();
	void DriveChase(ABNVehiclePawn& Car, float DeltaSeconds);
	void ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds) const;
	void UpdateLightsAndSiren();
	void RecoverStuck(ABNVehiclePawn& Car);

	TWeakObjectPtr<ABNVehiclePawn> TargetCar;
	FBNHeatTierSpec HeatSpec;
	int32 FormationSlot = 0;
	bool bRoadblockUnit = false;
	bool bDisabled = false;
	bool bSearchMode = false;
	bool bGearAttached = false;
	FVector SearchPoint = FVector::ZeroVector;
	float DisabledSeconds = 0.f;
	float StuckSeconds = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LightbarMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> RedLight;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> BlueLight;

	UPROPERTY(Transient)
	TObjectPtr<UBNPoliceSirenComponent> Siren;
};
