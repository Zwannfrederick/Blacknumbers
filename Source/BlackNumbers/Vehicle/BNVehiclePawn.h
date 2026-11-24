#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Garage/BNGarageTypes.h"
#include "Vehicle/BNVehicleBoost.h"
#include "BNVehiclePawn.generated.h"

class UBNEngineAudioComponent;
class UBNVehicleEffectsComponent;
class UBNVehicleVisualsComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputMappingContext;
class ULocalLightComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FHitResult;
struct FInputActionValue;

/**
 * Arcade (NFS-style) drift. Steering + handbrake at speed starts it; while the wheel is held into the turn the
 * assist holds the slide (angle set by how far the wheel is turned) and the throttle keeps the speed up.
 * Letting go of the wheel or countersteering unwinds it and the car grips up and drives on.
 */
USTRUCT(BlueprintType)
struct FBNDriftTuning
{
	GENERATED_BODY()

	/** The handbrake only starts a drift above this speed; below 2/3 of it a drift ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float MinSpeedKmh = 30.f;

	/** Drift angle with the wheel just into the turn, and at full lock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "5", ClampMax = "60", Units = "Degrees"))
	float MinAngleDegrees = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "5", ClampMax = "70", Units = "Degrees"))
	float MaxAngleDegrees = 36.f;

	/** How fast the car's angle follows the target (1/s). Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.5"))
	float AngleGain = 3.0f;

	/**
	 * The direction of travel swings toward where the car points at this many degrees per second per degree
	 * of drift angle: sets how tight the drift line is. Higher = tighter circle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float LineTightness = 1.15f;

	/** Throttle push along the drift (cm/s^2 at full throttle) against the speed the slide scrubs off, up to the entry speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Thrust = 1100.f;

	/** The drift ends once it has unwound below this angle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", Units = "Degrees"))
	float ExitAngleDegrees = 6.f;

	/** Rear tyre grip during a drift, relative to normal: low so the tyres do not fight the assist. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1"))
	float RearGripScale = 0.45f;

	/** After a drift the rear grip ramps back to full over this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05", Units = "Seconds"))
	float GripRecoverSeconds = 0.6f;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedKmh() const;

	/** Puts the car back on its wheels slightly above where it is, keeping its heading. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ResetVehicle();

	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsDrifting() const { return bDrifting; }

	/**
	 * Any wheel touching the ground. (The movement component's IsMovingOnGround() is the generic pawn one and
	 * is always false for Chaos vehicles.)
	 */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsOnGround() const;

	/** Signed angle between heading and travel, positive when sliding to the right. 0 when nearly stopped. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSlipAngleDegrees() const;

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetNosWanted(bool bWanted) { bNosWanted = bWanted; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetSpeedbreakerWanted(bool bWanted) { bSpeedbreakerWanted = bWanted; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void GrantNearMissBonus();

	/** Triggers a short physical camera jolt + FOV micro-punch on heavy crashes, rams, or Pursuit Breakers. */
	void AddCameraImpulse(float Strength);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetNosFraction() const { return BoostState.NosFraction; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsNosActive() const { return BoostState.bNosActive; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedbreakerFraction() const { return BoostState.SpeedbreakerFraction; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsSpeedbreakerActive() const { return BoostState.bSpeedbreakerActive; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetLaunchRevFraction() const { return BoostState.LaunchRevFraction; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsInLaunchSweetSpot() const { return BoostState.IsInLaunchSweetSpot(Boost); }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetLaunchBannerSeconds() const { return BoostState.LaunchBannerRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetNearMissBannerSeconds() const { return BoostState.NearMissBannerRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetNearMissStreak() const { return NearMissStreak; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetDriftExitBannerSeconds() const { return DriftExitBannerRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsBackfireActive() const { return BoostState.BackfireFlashRemaining > 0.f; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FBNDriftTuning Drift;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FBNBoostTuning Boost;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UBNVehicleVisualsComponent> Visuals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UBNEngineAudioComponent> EngineAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UBNVehicleEffectsComponent> Effects;

	/** Chase distances the camera key (C / gamepad right stick click) cycles between before Hood/Kaput mode. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "200"))
	TArray<float> ChaseDistances = { 650.f, 900.f };

	/** While drifting the camera turns this fraction of the slip angle toward the travel direction. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float DriftCameraBlend = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
	float DriftCameraInterpSpeed = 5.f;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void CycleChaseDistance();

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsHoodCamera() const { return ChaseDistanceIndex >= ChaseDistances.Num(); }

	UFUNCTION(BlueprintPure, Category = "Camera")
	FString GetCameraModeLabel() const;

	/** Cinematic slow-orbit 3/4 hero camera for Title Screen & Selin'in Garaji showcase. */
	void SetShowcaseMode(bool bActive, float LateralOffsetCm = 0.f);
	bool IsShowcaseMode() const { return bShowcaseMode; }

	/** Gives up the race this car is in (Backspace / gamepad View). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void AbandonRace();

	/** Starts or escalates a Karamara police pursuit (P / gamepad D-Pad Up). */
	UFUNCTION(BlueprintCallable, Category = "Police")
	void TogglePursuit();

	/** Opens or closes Selin'in Garaji when free-roaming (G / gamepad D-Pad Down). */
	UFUNCTION(BlueprintCallable, Category = "Garage")
	void ToggleGarage();

	/** Opens or closes the 16-member BLACK NUMBERS Dossier & Evidence Ledger (B / D-Pad Left). */
	UFUNCTION(BlueprintCallable, Category = "Story")
	void ToggleStoryDossier();

	/** Opens main pause/race menu or dismisses active menu/story card (ESC / Start). */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePauseMenu();

	void ApplyVehicleSpec(EBNCarId CarId, const FBNComputedVehicleStats& Stats);
	void ApplyVehicleBuild(const FBNComputedVehicleStats& Stats, const FLinearColor& PaintTint);
	void ApplyPaintTint(const FLinearColor& PaintTint);

	/** Head and tail lights; switched by the level's ABNDayNightCycle (on at night). */
	UFUNCTION(BlueprintCallable, Category = "Lights")
	void SetLightsOn(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Lights")
	bool AreLightsOn() const;

	/** Holds the car still (handbrake on, pedals ignored), e.g. on the grid during a race countdown. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetInputLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsInputLocked() const { return bInputLocked; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights")
	TArray<TObjectPtr<ULocalLightComponent>> CarLights;

	/** Separate from CarLights so World.LightsFollowNight only checks headlights/taillights. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights")
	TArray<TObjectPtr<UPointLightComponent>> ExhaustLights;

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
	void OnNos(const FInputActionValue& Value);
	void OnSpeedbreaker(const FInputActionValue& Value);
	void AdvanceStoryCard();

	void UpdateDrift(float DeltaSeconds);
	void UpdateNormalGripAssist(float DeltaSeconds);
	/** Holds the slide at TargetSlip (signed degrees): turns the car and its direction of travel smoothly. */
	void ApplyDriftAssist(float DeltaSeconds, float TargetSlip, float Throttle);
	void SetRearGrip(float Blend);
	void ApplyPhysicsStats(const FBNComputedVehicleStats& Stats);
	void UpdateBoostAndEffects(float DeltaSeconds, float RealDt);
	void UpdateCamera(float DeltaSeconds, float RealDt, float SpeedKmh, float RealTimeSeconds);
	void UpdateShowcaseCamera(float RealDt);
	void UpdateNearMiss(float SpeedKmh);
	void ResetTimeDilation();

	UFUNCTION()
	void OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	bool bDrifting = false;
	bool bInputLocked = false;
	/** 0 = drift grip, 1 = full rear grip; last value sent to the physics wheels. */
	float RearGripBlend = 1.f;
	bool bRearAbsBypassed = false;
	bool bRearTractionControl = true;
	/** +1 for a drift turning left (sliding right, positive slip), -1 for one turning right. */
	float DriftDirection = 0.f;
	/** Ground speed (cm/s) when the drift started; the throttle keeps the drift up to it. */
	float DriftEntrySpeed = 0.f;
	float CurrentDriftTargetSlip = 0.f;
	int32 ChaseDistanceIndex = 0;

	FBNBoostState BoostState;
	bool bNosWanted = false;
	bool bSpeedbreakerWanted = false;
	bool bOwnsTimeDilation = false;
	float PlayerThrottleInput = 0.f;
	float PlayerSteeringTarget = 0.f;
	float PlayerSteeringSmoothed = 0.f;
	bool bHasPlayerSteeringInput = false;
	int32 LastGear = 0;
	float GearShiftKick = 0.f;
	float CameraImpactShake = 0.f;
	float DriftHoldSeconds = 0.f;
	float DriftExitBoostRemaining = 0.f;
	float DriftExitBannerRemaining = 0.f;
	int32 NearMissStreak = 0;
	float NearMissStreakTimer = 0.f;
	float ActiveTireGripScale = 1.f;
	float LastAppliedTireGripScale = 1.f;
	float CurrentSocketBaseZ = 150.f;
	bool bShowcaseMode = false;
	float ShowcaseYawDeg = 148.f;
	float ShowcaseLateralOffsetCm = 0.f;
	EBNCarId ActiveCarId = EBNCarId::KuroganeMSpec;
	TMap<TWeakObjectPtr<AActor>, double> NearMissCooldowns;

	/** Owns the runtime-created input actions through its key mappings. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputContext;
};
