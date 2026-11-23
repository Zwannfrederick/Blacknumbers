#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BNTrafficSubsystem.generated.h"

class AActor;
class ABNVehiclePawn;
class UMaterialInterface;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Hardware-friendly kinematic civilian traffic subsystem for Karamara maps.
 * Stays completely dormant (0 actors, 0 per-frame work) on non-Karamara maps such as /Game/Maps/TestTrack.
 *
 * Spawns 14 lightweight static-mesh civilian cars on right-hand lanes across Karamara's boulevards
 * and elevated Kuzey Viyadugu, yielding for other vehicles and nearby police sirens, granting
 * Near-Miss NOS bonuses on clean passes, and reacting smoothly to player collisions.
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

	/** Writes world-space locations of all active civilian traffic cars (used by minimap radar). */
	void GetTrafficLocations(TArray<FVector>& OutLocations) const;

	UFUNCTION(BlueprintPure, Category = "Traffic")
	int32 GetActiveTrafficCount() const;

	UFUNCTION(BlueprintCallable, Category = "Traffic")
	void SetTrafficEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool IsTrafficEnabled() const { return bTrafficEnabled; }

	/** Elevation profile (cm) along Kuzey Viyadugu (Y = -24000) matching Tools/build_karamara_map.py. */
	static float SampleViaductElevationZ(float X);

private:
	enum class ETrafficCarState : uint8
	{
		Cruising,
		Knocked,
		Paused,
	};

	struct FTrafficRoute
	{
		TArray<FVector> Points;
		TArray<float> CumulativeDist;
		float TotalLength = 0.f;
		bool bClosed = true;
	};

	struct FTrafficCar
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Wheels;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> LightLenses;
		TWeakObjectPtr<USpotLightComponent> HeadlightBeam;

		int32 RouteIndex = 0;
		float DistanceAlong = 0.f;
		float CruiseSpeedCmPerSec = 1500.f;
		float CurrentSpeedCmPerSec = 1500.f;
		float LateralPullOffset = 0.f;
		float WheelSpinDeg = 0.f;
		float NearMissCooldown = 0.f;

		ETrafficCarState State = ETrafficCarState::Cruising;
		float StateTimer = 0.f;
		FVector KnockVelocity = FVector::ZeroVector;
		float KnockYawRateDeg = 0.f;
	};

	UFUNCTION()
	void HandleNightChanged(bool bInIsNight);

	void BuildKaramaraRoutes();
	void LoadTrafficAssets();
	void SpawnAllTraffic();
	void ClearTraffic();
	void ApplyNightLightsToCar(const FTrafficCar& Car, bool bOn) const;
	void SampleRoute(int32 RouteIndex, float Distance, FVector& OutLocation, FRotator& OutRotation) const;
	float FindSafeRespawnDistance(int32 RouteIndex, const FVector& PlayerLoc, int32 IgnoreCarIndex) const;
	ABNVehiclePawn* FindPlayerCar() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CarBodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CarGlassMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CarWheelMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LightLensMaterial;

	FVector WheelLocalOffsets[4] = {
		FVector(145.f, -92.f, 39.f),
		FVector(145.f,  92.f, 39.f),
		FVector(-140.f, -92.f, 40.f),
		FVector(-140.f,  92.f, 40.f)
	};

	TArray<FTrafficRoute> Routes;
	TArray<FTrafficCar> Cars;

	bool bIsKaramaraMap = false;
	bool bWorldBegunPlay = false;
	bool bTrafficEnabled = true;
	bool bNightLightsOn = false;
};
