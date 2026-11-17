#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNStreetLamp.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Sodium street lamp that switches on at night. Local +X points over the road.
 * Finds the level's ABNDayNightCycle on BeginPlay; without one it stays off.
 * Materials are assigned per level (Tools/build_test_map.py), the shapes are engine basics.
 */
UCLASS()
class BLACKNUMBERS_API ABNStreetLamp : public AActor
{
	GENERATED_BODY()

public:
	ABNStreetLamp();

	UFUNCTION(BlueprintCallable, Category = "Lamp")
	void SetLit(bool bLit);

	UFUNCTION(BlueprintPure, Category = "Lamp")
	bool IsLit() const;

	UPROPERTY(VisibleAnywhere, Category = "Lamp")
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY(VisibleAnywhere, Category = "Lamp")
	TObjectPtr<UStaticMeshComponent> Arm;

	/** Emissive panel under the head, shown only while lit. */
	UPROPERTY(VisibleAnywhere, Category = "Lamp")
	TObjectPtr<UStaticMeshComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Lamp")
	TObjectPtr<USpotLightComponent> Light;

protected:
	virtual void BeginPlay() override;
};
