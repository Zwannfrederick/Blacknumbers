#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNPursuitBreaker.generated.h"

class ABNVehiclePawn;
class UBoxComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
struct FHitResult;

/**
 * Collapsible Karamara Pursuit Breaker (scaffolding arch, dock gantry, highway gantry).
 * Driving through at speed triggers a collapse, disables chasing police in DisableRadiusCm,
 * and adds CollateralCostDollars to the session Sehir Faturasi (Kanon #21).
 */
UCLASS()
class BLACKNUMBERS_API ABNPursuitBreaker : public AActor
{
	GENERATED_BODY()

public:
	ABNPursuitBreaker();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "PursuitBreaker")
	void TriggerCollapse(ABNVehiclePawn* TriggerCar = nullptr);

	UFUNCTION(BlueprintCallable, Category = "PursuitBreaker")
	void ResetStructure();

	UFUNCTION(BlueprintPure, Category = "PursuitBreaker")
	bool IsCollapsed() const { return bCollapsed; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PillarL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PillarR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OverheadBeam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> WarningLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PursuitBreaker")
	FString StructureName = TEXT("Sanayi Iskele Taki");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PursuitBreaker", meta = (ClampMin = "1000"))
	int32 CollateralCostDollars = 25000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PursuitBreaker", meta = (ClampMin = "500", Units = "Centimeters"))
	float DisableRadiusCm = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PursuitBreaker", meta = (ClampMin = "0"))
	float MinTriggerSpeedKmh = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PursuitBreaker", meta = (ClampMin = "5", Units = "Seconds"))
	float ResetCooldownSeconds = 45.f;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyCollapseTransform(float Alpha);

	bool bCollapsed = false;
	float CollapseTimer = 0.f;
	float DebrisTimer = 0.f;
	float ResetTimer = 0.f;
};
