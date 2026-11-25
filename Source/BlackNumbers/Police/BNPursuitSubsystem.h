#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Police/BNPursuitLogic.h"
#include "BNPursuitSubsystem.generated.h"

class AActor;
class ABNPoliceController;
class ABNPursuitBreaker;
class ABNVehiclePawn;

/**
 * Coordinates Karamara police pursuits (Heat 1-5 default, up to 11 when unlocked),
 * organic free-roam patrols on the road graph, Heavy Rhino SUVs (Heat 5+),
 * road-aligned roadblocks + Heat 5+ spike strips, Cooldown hideout spots,
 * Pursuit Breaker takedowns, and Sehir Faturasi tracking.
 */
UCLASS()
class BLACKNUMBERS_API UBNPursuitSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Police")
	void StartPursuit(int32 InitialHeat = 1);

	/** Always-on Most Wanted-style mid-race police intervention; seamlessly continues into free roam after finish. */
	UFUNCTION(BlueprintCallable, Category = "Police")
	void StartInRacePursuit(int32 InitialHeat = 1);

	void NotifyRaceEndedKeepPursuit();

	UFUNCTION(BlueprintPure, Category = "Police")
	bool IsInRaceIntervention() const { return bInRaceIntervention; }

	UFUNCTION(BlueprintCallable, Category = "Police")
	void EscalateOrStartPursuit();

	UFUNCTION(BlueprintCallable, Category = "Police")
	void StopPursuit();

	UFUNCTION(BlueprintCallable, Category = "Police")
	void SetMaxAllowedHeat(int32 MaxHeat);

	UFUNCTION(BlueprintCallable, Category = "Police")
	void ReportCollateralDamage(int32 Dollars, const FString& Reason = TEXT(""));

	void OnPursuitBreakerTriggered(ABNPursuitBreaker* Breaker, ABNVehiclePawn* PlayerCar);
	void DisablePoliceNear(const FVector& Origin, float RadiusCm);
	void OnPlayerRammedPolice(ABNVehiclePawn* PoliceCar, float PlayerSpeedKmh);
	bool GetNearestRoadblockAhead(const ABNVehiclePawn& Player, float& OutDistanceMeters, bool& bOutHasSpikes, FVector* OutWorldLoc = nullptr) const;

	const FBNPursuitState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Police")
	float GetTakedownBannerSeconds() const { return TakedownBannerSeconds; }

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetActivePoliceCount() const;

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetActivePatrolCount() const;

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetCostToStateDollars() const { return State.CostToStateDollars; }

	UFUNCTION(BlueprintPure, Category = "Police")
	bool IsPlayerTireSpiked() const { return SpikePenaltySeconds > 0.f; }

	UFUNCTION(BlueprintCallable, Category = "Police")
	void SetFreeRoamPatrolsEnabled(bool bEnabled);

private:
	struct FPoliceUnit
	{
		TWeakObjectPtr<ABNVehiclePawn> Car;
		TWeakObjectPtr<ABNPoliceController> Controller;
	};

	struct FSpikeStripHazard
	{
		TWeakObjectPtr<AActor> VisualActor;
		FVector Center = FVector::ZeroVector;
		FVector RightDir = FVector::RightVector;
		float HalfLengthCm = 320.f;
		bool bTriggered = false;
	};

	ABNVehiclePawn* FindPlayerCar() const;
	void SpawnChaser(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec, int32 Slot);
	void SpawnRoadblock(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec);
	void TickFreeRoamPatrols(ABNVehiclePawn* Player, float DeltaTime);
	void EnsureFreeRoamPatrols(const FVector& PlayerLoc);
	void ClearPatrols();
	void ClearSpikeStrips();
	void TickSpikeStrips(ABNVehiclePawn& Player, float DeltaTime);
	void ClearAllUnits();
	void PruneUnits();

	FBNPursuitState State;
	TArray<FPoliceUnit> Units;
	TArray<FPoliceUnit> PatrolUnits;
	TArray<FSpikeStripHazard> SpikeStrips;
	float SpawnCooldown = 0.f;
	float RoadblockCooldown = 0.f;
	float SpikePenaltySeconds = 0.f;
	float TakedownBannerSeconds = 0.f;
	float PatrolSpawnCooldown = 2.f;
	int32 NextSlot = 0;
	int32 LastDispatchSerial = 0;
	bool bFreeRoamPatrolsEnabled = true;
	bool bInRaceIntervention = false;
};
