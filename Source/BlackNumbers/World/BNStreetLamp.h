#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNStreetLamp.generated.h"

class UMaterialInstanceDynamic;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Sodium street lamp that switches on at night, warming up over a few seconds (each lamp after its own short
 * delay, so a street lights up gradually instead of all at once). Local +X points over the road.
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

	/** Whether the lamp is switched on (it may still be warming up). */
	UFUNCTION(BlueprintPure, Category = "Lamp")
	bool IsLit() const { return bLit; }

	/** 0 = dark, 1 = fully warmed up. */
	UFUNCTION(BlueprintPure, Category = "Lamp")
	float GetBrightness() const { return Brightness; }

	virtual void Tick(float DeltaSeconds) override;

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

private:
	void ApplyBrightness();

	bool bLit = false;
	float Brightness = 0.f;
	/** Seconds left before this lamp starts to change after being switched. */
	float SwitchDelay = 0.f;
	FLinearColor GlowEmissive = FLinearColor::Black;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;
};
