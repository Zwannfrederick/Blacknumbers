#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Race/BNDriftScorer.h"
#include "Race/BNRaceProgress.h"
#include "BNRaceEvent.generated.h"

class ABNVehiclePawn;
class UBNRaceDefinition;
class UPrimitiveComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EBNRaceState : uint8
{
	Idle,      // beacon waiting for the player to drive in
	Countdown, // car held on the grid
	Racing,
	Finished,  // results on screen
};

/**
 * A race start in the world: a light beacon the player drives into. Runs one race from a UBNRaceDefinition:
 * grid + countdown with inputs locked, gate crossings or drift scoring, results. Only the next gate is shown.
 */
UCLASS()
class BLACKNUMBERS_API ABNRaceEvent : public AActor
{
	GENERATED_BODY()

public:
	ABNRaceEvent();

	virtual void Tick(float DeltaSeconds) override;

	/** Puts Car on the grid and starts the countdown. */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace(ABNVehiclePawn* Car);

	/** Ends the race now and shows the results (drift time-out, or the player giving up). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void EndRace();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	TObjectPtr<UBNRaceDefinition> Definition;

	/** Driving within this distance of the beacon starts the race. */
	UPROPERTY(EditAnywhere, Category = "Race", meta = (ClampMin = "100", Units = "Centimeters"))
	float StartRadius = 600.f;

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<UStaticMeshComponent> Beacon;

	/** Left and right posts of the next gate. */
	UPROPERTY(VisibleAnywhere, Category = "Race")
	TArray<TObjectPtr<UStaticMeshComponent>> GatePosts;

	EBNRaceState GetState() const { return State; }
	const FBNRaceProgress& GetProgress() const { return Progress; }
	const FBNDriftScorer& GetDriftScore() const { return DriftScore; }
	/** Seconds into the race (counts down from 3 during the countdown). */
	float GetRaceTime() const;
	float GetTimeLeft() const;

private:
	void TickIdle(ABNVehiclePawn& Car);
	void TickRacing(ABNVehiclePawn& Car, float DeltaSeconds);
	void ShowGate(int32 Index);
	void Finish();

	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	EBNRaceState State = EBNRaceState::Idle;
	TWeakObjectPtr<ABNVehiclePawn> Racer;
	FBNRaceProgress Progress;
	FBNDriftScorer DriftScore;
	double StateStart = 0.0;
	FVector LastCarLocation = FVector::ZeroVector;
	/** The beacon only triggers after the car has been outside it, so spawning or finishing on it does not restart. */
	bool bArmed = false;
};
