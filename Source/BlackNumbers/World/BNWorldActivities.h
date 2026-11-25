#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/BNRoadGraph.h"
#include "BNWorldActivities.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Open-world roadside Speed Trap radar camera (6 placed across Karamara's 6 boroughs).
 * Blowing past above MinTriggerSpeedKmh fires a camera strobe flash, saves Personal Best km/h,
 * and awards Cash + Reputation (and bonus Pursuit Bounty if fleeing police).
 */
UCLASS()
class BLACKNUMBERS_API ABNSpeedTrap : public AActor
{
	GENERATED_BODY()

public:
	ABNSpeedTrap();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activity")
	TObjectPtr<UStaticMeshComponent> PostMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activity")
	TObjectPtr<UStaticMeshComponent> CameraHeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activity")
	TObjectPtr<UPointLightComponent> FlashLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	FString TrapId = TEXT("TRAP_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	FString DisplayName = TEXT("HIZ TUZAĞI");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	EBNDistrict District = EBNDistrict::MerkezBulvar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (ClampMin = "40"))
	float MinTriggerSpeedKmh = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (ClampMin = "200"))
	float TriggerRadiusCm = 750.f;

private:
	float CooldownRemaining = 0.f;
	float FlashRemaining = 0.f;
};

/**
 * Open-world Stunt Jump ramp (4 iconic jumps across Karamara).
 * Measures horizontal airborne jump distance on landing, awards Cash + Rep, and saves Personal Best meters.
 */
UCLASS()
class BLACKNUMBERS_API ABNStuntJump : public AActor
{
	GENERATED_BODY()

public:
	ABNStuntJump();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activity")
	TObjectPtr<UStaticMeshComponent> RampMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activity")
	TObjectPtr<UPointLightComponent> BeaconLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	FString JumpId = TEXT("JUMP_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	FString DisplayName = TEXT("SIÇRAMA RAMPASI");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	EBNDistrict District = EBNDistrict::EskiLiman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (ClampMin = "200"))
	float TriggerRadiusCm = 620.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (ClampMin = "40"))
	float MinLaunchSpeedKmh = 70.f;

private:
	bool bTrackingAirborne = false;
	bool bSawAirborne = false;
	FVector LaunchOrigin = FVector::ZeroVector;
	uint32 LaunchRecoveryCount = 0;
	float TrackingElapsed = 0.f;
	float CooldownRemaining = 0.f;
};
