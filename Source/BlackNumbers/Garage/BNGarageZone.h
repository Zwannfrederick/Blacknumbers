#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNGarageZone.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * World entry bay for Selin'in Garaji (Yamaç Mahallesi / Doğu Sanayi).
 * Driving slowly into the glowing green/amber bay opens the garage upgrade & car selection HUD.
 */
UCLASS()
class BLACKNUMBERS_API ABNGarageZone : public AActor
{
	GENERATED_BODY()

public:
	ABNGarageZone();

	virtual void Tick(float DeltaSeconds) override;

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
	float TriggerRadius = 550.f;

private:
	bool bArmed = false;
};
