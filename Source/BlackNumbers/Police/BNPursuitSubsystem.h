#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Police/BNPursuitLogic.h"
#include "BNPursuitSubsystem.generated.h"

class ABNPoliceController;
class ABNPursuitBreaker;
class ABNVehiclePawn;

/**
 * Coordinates Karamara police pursuits (Heat 1-5 default, up to 11 when unlocked),
 * unit spawning, Heat 3+ roadblocks, Pursuit Breaker takedowns, and Sehir Faturasi tracking.
 * Remains dormant (zero cruisers, zero per-frame work) until triggered.
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

	const FBNPursuitState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetActivePoliceCount() const;

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetCostToStateDollars() const { return State.CostToStateDollars; }

private:
	struct FPoliceUnit
	{
		TWeakObjectPtr<ABNVehiclePawn> Car;
		TWeakObjectPtr<ABNPoliceController> Controller;
	};

	ABNVehiclePawn* FindPlayerCar() const;
	void SpawnChaser(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec, int32 Slot);
	void SpawnRoadblock(ABNVehiclePawn& Player, const FBNHeatTierSpec& Spec);
	void ClearAllUnits();
	void PruneUnits();

	FBNPursuitState State;
	TArray<FPoliceUnit> Units;
	float SpawnCooldown = 0.f;
	float RoadblockCooldown = 0.f;
	int32 NextSlot = 0;
};
