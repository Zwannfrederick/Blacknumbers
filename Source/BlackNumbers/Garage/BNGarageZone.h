#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNGarageZone.generated.h"

class ABNVehiclePawn;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * World entry bay for Selin'in Garaji (Yamaç Mahallesi / Doğu Sanayi).
 * Passing through damaged auto-repairs the car for free; pressing [ENTER] or [G] inside opens the garage HUD.
 */
UCLASS()
class BLACKNUMBERS_API ABNGarageZone : public AActor
{
	GENERATED_BODY()

public:
	ABNGarageZone();

	virtual void Tick(float DeltaSeconds) override;

	/** Returns the garage bay currently containing Car (when not in pursuit/race), or nullptr. */
	static ABNGarageZone* FindNearbyGarageZone(const ABNVehiclePawn* Car);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<UStaticMeshComponent> PillarL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<UStaticMeshComponent> PillarR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<UStaticMeshComponent> BeaconBeam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<UPointLightComponent> BayLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Garage")
	FString GarageName = TEXT("Selin'in Garaji");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Garage", meta = (ClampMin = "150", Units = "Centimeters"))
	float TriggerRadius = 620.f;

private:
	bool bRepairArmed = true;
};
