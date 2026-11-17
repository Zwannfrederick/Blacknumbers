#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "BNVehiclePawn.generated.h"

class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Player car: Chaos wheeled vehicle with a chase camera.
 * Keyboard and gamepad bindings are built in code, so there are no input assets to keep in sync.
 * Uses the engine template's sports car as a placeholder body until BLACK NUMBERS has its own cars.
 */
UCLASS()
class BLACKNUMBERS_API ABNVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ABNVehiclePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedKmh() const;

	/** Puts the car back on its wheels slightly above where it is, keeping its heading. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ResetVehicle();

	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Field of view widens from BaseFov to MaxFov as speed approaches FovMaxSpeedKmh. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float BaseFov = 90.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxFov = 105.f;

	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1"))
	float FovMaxSpeedKmh = 220.f;

private:
	void OnSteering(const FInputActionValue& Value);
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnHandbrake(const FInputActionValue& Value);

	/** Owns the runtime-created input actions through its key mappings. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputContext;
};
