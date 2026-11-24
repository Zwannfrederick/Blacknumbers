#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BNTrafficSubsystem.generated.h"

class AActor;
class ABNVehiclePawn;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Dynamic road-graph civilian traffic subsystem for Karamara's 2 km x 2 km city.
 * Stays strictly dormant (0 actors, 0 per-frame work) on non-Karamara maps such as TestTrack.
 *
 * Drives 20 active multi-type vehicles (Sedan, Hatchback, Van, BoxTruck, SUV) on right-hand lanes
 * across UBNRoadGraphSubsystem, recycling distant cars seamlessly around the player (140..560 m),
 * yielding for other vehicles & sirens, lighting brake lamps when slowing, granting Near-Miss NOS,
 * and reacting physically to collisions.
 */
UCLASS()
class BLACKNUMBERS_API UBNTrafficSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	void GetTrafficLocations(TArray<FVector>& OutLocations) const;

	UFUNCTION(BlueprintPure, Category = "Traffic")
	int32 GetActiveTrafficCount() const;

	UFUNCTION(BlueprintCallable, Category = "Traffic")
	void SetTrafficEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool IsTrafficEnabled() const { return bTrafficEnabled; }

	/** Legacy elevation profile helper retained for unit-test compatibility. */
	static float SampleViaductElevationZ(float X);

private:
	enum class ETrafficCarState : uint8
	{
		Cruising,
		Knocked,
		Paused,
	};

	struct FTrafficCar
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UStaticMeshComponent> BodyComp;
		TWeakObjectPtr<UMaterialInstanceDynamic> TaillightMid;
		TWeakObjectPtr<USpotLightComponent> HeadlightBeam;

		int32 EdgeIdx = 0;
		bool bReversed = false;
		float DistanceAlong = 0.f;
		float CruiseSpeedCmPerSec = 1550.f;
		float CurrentSpeedCmPerSec = 1550.f;
		float LateralPullOffset = 0.f;
		float NearMissCooldown = 0.f;

		ETrafficCarState State = ETrafficCarState::Cruising;
		float StateTimer = 0.f;
		FVector KnockVelocity = FVector::ZeroVector;
		float KnockYawRateDeg = 0.f;
	};

	UFUNCTION()
	void HandleNightChanged(bool bInIsNight);

	void LoadTrafficAssets();
	void SpawnAllTraffic();
	void ClearTraffic();
	void ApplyNightLightsToCar(const FTrafficCar& Car, bool bOn) const;
	bool PickEdgeNearPlayer(const FVector& PlayerLoc, float MinDistCm, float MaxDistCm,
		int32 IgnoreCarIdx, int32& OutEdgeIdx, bool& bOutRev, float& OutDistAlong) const;
	void AdvanceToNextEdge(FTrafficCar& Car) const;
	bool ComputeCarPose(const FTrafficCar& Car, FVector& OutLocation, FRotator& OutRotation) const;
	ABNVehiclePawn* FindPlayerCar() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> VehicleMeshes;

	TArray<FTrafficCar> Cars;
	bool bIsKaramaraMap = false;
	bool bWorldBegunPlay = false;
	bool bTrafficEnabled = true;
	bool bNightLightsOn = false;
};
