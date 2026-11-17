#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "BNVehiclePawn.generated.h"

class UBNEngineAudioComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputMappingContext;
class ULocalLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** Arcade drift assist on top of Chaos tyre physics: the slide itself stays physical. */
USTRUCT(BlueprintType)
struct FBNDriftTuning
{
	GENERATED_BODY()

	/** The handbrake only starts a drift above this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float MinSpeedKmh = 30.f;

	/** Rear tyre grip while the handbrake is held at speed, relative to normal. Lower = easier slides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1"))
	float RearGripScale = 0.45f;

	/**
	 * After releasing the handbrake the rear grip ramps back to full over this long, unless the driver is
	 * countersteering (which holds the drift). Straightening up and flooring it pulls the car out of the slide.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05", Units = "Seconds"))
	float GripRecoverSeconds = 1.f;
};

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
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedKmh() const;

	/** Puts the car back on its wheels slightly above where it is, keeping its heading. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ResetVehicle();

	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsDrifting() const { return bDrifting; }

	/** Signed angle between heading and travel, positive when sliding to the right. 0 when nearly stopped. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSlipAngleDegrees() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FBNDriftTuning Drift;

	/** Auto-reverse would otherwise run the reverse gear to ~90 km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "5"))
	float MaxReverseSpeedKmh = 35.f;

	/**
	 * Visible body. The skeletal mesh is only an invisible wheel rig for Chaos and the wheel animation;
	 * body, glass and wheels are static meshes riding on it, as in the engine's vehicle template.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UStaticMeshComponent> Chassis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UStaticMeshComponent> ChassisGlass;

	/** FL, FR, BL, BR, each attached to its Phys_Wheel_* bone so it follows suspension and spin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UBNEngineAudioComponent> EngineAudio;

	/** Chase distances the camera key (C / gamepad right stick click) cycles between. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "200"))
	TArray<float> ChaseDistances = { 650.f, 900.f };

	/** While drifting the camera turns this fraction of the slip angle toward the travel direction. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float DriftCameraBlend = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
	float DriftCameraInterpSpeed = 2.f;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void CycleChaseDistance();

	/** Head and tail lights; switched by the level's ABNDayNightCycle (on at night). */
	UFUNCTION(BlueprintCallable, Category = "Lights")
	void SetLightsOn(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Lights")
	bool AreLightsOn() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights")
	TArray<TObjectPtr<ULocalLightComponent>> CarLights;

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

	void UpdateDrift(float DeltaSeconds);

	bool bDrifting = false;
	/** 0 = drift grip, 1 = full rear grip; last value sent to the physics wheels. */
	float RearGripBlend = 1.f;
	bool bRearAbsBypassed = false;
	int32 ChaseDistanceIndex = 0;

	/** Owns the runtime-created input actions through its key mappings. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputContext;
};
