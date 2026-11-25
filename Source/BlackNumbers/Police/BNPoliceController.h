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
 * AI driver and police rig controller for Karamara interceptor, Heavy Rhino SUV,
 * roadblock, and free-roam patrol units. Navigates via UBNRoadGraphSubsystem A*
 * whenever line-of-sight is blocked or target is distant, switching to direct
 * flank / box-in tactics once visual contact is clear.
 */
UCLASS()
class BLACKNUMBERS_API ABNPoliceController : public AController
{
	GENERATED_BODY()

public:
	ABNPoliceController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetupPursuitUnit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec, int32 InFormationSlot,
		bool bInRoadblockUnit = false, bool bInHeavyRhino = false);
	void SetupPatrolUnit(int32 StartEdgeIdx, bool bStartReversed);
	void PromotePatrolToPursuit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec, int32 InFormationSlot);
	void UpdateHeatSpec(const FBNHeatTierSpec& InSpec);
	void SetSearchMode(bool bInSearchMode, const FVector& InSearchPoint);
	void DisableUnit();

	bool IsUnitDisabled() const { return bDisabled; }
	bool IsRoadblockUnit() const { return bRoadblockUnit; }
	bool IsPatrolUnit() const { return bPatrolUnit; }
	bool IsHeavyRhino() const { return bHeavyRhino; }
	bool IsReadyForCleanup() const { return bDisabled && DisabledSeconds >= 8.f; }
	bool HasVisualLightbar() const { return RedLight != nullptr && BlueLight != nullptr; }
	bool IsSirenActive() const;
	bool IsAttemptingPit() const { return bPitActive; }
	/** Called when this unit touches the suspect; spins the suspect if a PIT is in progress. */
	void ApplyPitContact(ABNVehiclePawn& Suspect);

private:
	void AttachPoliceGear(ABNVehiclePawn& Car);
	void CleanupPoliceGear();
	void DriveChase(ABNVehiclePawn& Car, float DeltaSeconds);
	void DrivePatrol(ABNVehiclePawn& Car, float DeltaSeconds);
	void UpdatePit(const ABNVehiclePawn& Target, const FVector& MyLoc, float DeltaSeconds, FVector& InOutAimPoint);
	void SteerTowardPoint(ABNVehiclePawn& Car, const FVector& AimPoint, float DesiredSpeedKmh, float DeltaSeconds) const;
	void ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds) const;
	void UpdateLightsAndSiren();
	void RecoverStuck(ABNVehiclePawn& Car);
	bool HasLineOfSightTo(const FVector& FromLoc, const FVector& ToLoc, const AActor* IgnoreA, const AActor* IgnoreB) const;

	TWeakObjectPtr<ABNVehiclePawn> TargetCar;
	FBNHeatTierSpec HeatSpec;
	int32 FormationSlot = 0;
	bool bRoadblockUnit = false;
	bool bHeavyRhino = false;
	bool bPatrolUnit = false;
	bool bDisabled = false;
	bool bSearchMode = false;
	bool bGearAttached = false;
	FVector SearchPoint = FVector::ZeroVector;
	float DisabledSeconds = 0.f;
	float StuckSeconds = 0.f;
	bool bPitActive = false;
	float PitSide = 1.f;
	float PitTimer = 0.f;
	float PitCooldown = 0.f;

	// A* road-graph navigation cache
	TArray<FVector> NavWaypoints;
	int32 NavWaypointIndex = 0;
	float NavRepathCooldown = 0.f;

	// Free-roam patrol road cursor
	int32 PatrolEdgeIdx = INDEX_NONE;
	bool bPatrolReversed = false;
	float PatrolDistAlong = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LightbarMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PushBarMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LiveryPanels;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> RedLight;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> BlueLight;

	UPROPERTY(Transient)
	TObjectPtr<UBNPoliceSirenComponent> Siren;
};
