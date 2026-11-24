#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Garage/BNGarageTypes.h"
#include "Race/BNDriftScorer.h"
#include "Race/BNRaceProgress.h"
#include "BNRaceEvent.generated.h"

class ABNVehiclePawn;
class UBNRaceDefinition;
class UPrimitiveComponent;
class USplineComponent;
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

/** One car in a race and its checkpoint progress. */
struct FBNRacer
{
	TWeakObjectPtr<ABNVehiclePawn> Car;
	FBNRaceProgress Progress;
	FVector LastLocation = FVector::ZeroVector;
};

/**
 * A race start in the world: a light beacon the player drives into. Runs one race from a UBNRaceDefinition:
 * grid + countdown with inputs locked, rivals, gate crossings or drift scoring, results. Only the next gate
 * is shown. Rivals are spawned for the race and removed when its results close.
 */
UCLASS()
class BLACKNUMBERS_API ABNRaceEvent : public AActor
{
	GENERATED_BODY()

public:
	ABNRaceEvent();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Puts Car on the grid and starts the countdown. */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace(ABNVehiclePawn* Car);

	/** Puts Car on the grid for a 1v1 BLACK NUMBERS boss showdown (or Drift boss target score). */
	void StartBossRace(ABNVehiclePawn* Car, int32 BossRank, float BossSkill,
		const FBNComputedVehicleStats& BossStats, const FLinearColor& BossTint, int32 DriftTargetScore = 0,
		EBNCarId BossCarId = EBNCarId::KuroganeMSpec);

	UFUNCTION(BlueprintPure, Category = "Race")
	int32 GetActiveBossRank() const { return ActiveBossRank; }

	UFUNCTION(BlueprintPure, Category = "Race")
	int32 GetActiveBossDriftTarget() const { return ActiveBossDriftTarget; }

	/** Ends the race now and shows the results (drift time-out, or the player giving up). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void EndRace();

	/** Cleanly restarts this currently running race (preserving boss showdown settings if active). */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void RestartActiveRace();

	/** Returns any race currently in Countdown or Racing in World (optionally ignoring one event). */
	static ABNRaceEvent* FindActiveRace(const UWorld* World, const ABNRaceEvent* IgnoreEvent = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	TObjectPtr<UBNRaceDefinition> Definition;

	/** Driving within this distance of the beacon starts the race. */
	UPROPERTY(EditAnywhere, Category = "Race", meta = (ClampMin = "100", Units = "Centimeters"))
	float StartRadius = 600.f;

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<UStaticMeshComponent> Beacon;

	/** The line rivals drive, rebuilt from the definition at each start. */
	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<USplineComponent> RacingLine;

	/** Left and right posts of the next gate. */
	UPROPERTY(VisibleAnywhere, Category = "Race")
	TArray<TObjectPtr<UStaticMeshComponent>> GatePosts;

	EBNRaceState GetState() const { return State; }
	const FBNRaceProgress& GetProgress() const { return Player.Progress; }
	const USplineComponent* GetRacingLine() const { return RacingLine; }
	/** True while Car is this race's player on the grid or racing. */
	bool IsRunningFor(const ABNVehiclePawn* Car) const
	{
		return Car && Player.Car.Get() == Car && (State == EBNRaceState::Countdown || State == EBNRaceState::Racing);
	}
	int32 GetNumRivals() const { return Rivals.Num(); }
	const FBNRacer& GetRival(int32 Index) const { return Rivals[Index]; }
	/** Player's place: 1 = leading. Finished racers rank by finish time, the rest by checkpoints passed. */
	int32 GetPlayerPosition() const;
	const FBNDriftScorer& GetDriftScore() const { return DriftScore; }
	/** Seconds into the race (counts down from 3 during the countdown). */
	float GetRaceTime() const;
	float GetTimeLeft() const;

private:
	void BeginRaceInternal(ABNVehiclePawn* Car, int32 BossRank, float BossSkill,
		const FBNComputedVehicleStats* BossStats, const FLinearColor& BossTint, int32 DriftTargetScore,
		EBNCarId BossCarId);
	void TickIdle(ABNVehiclePawn& Car);
	void TickRacing(ABNVehiclePawn& Car, float DeltaSeconds);
	void ShowGate(int32 Index);
	void Finish();
	void BuildRacingLine();
	void SpawnRivals();
	void ClearRivals();
	/** Counts a gate crossing since the last frame. */
	EBNRaceStep AdvanceRacer(FBNRacer& Racer);
	bool IsAhead(const FBNRacer& A, const FBNRacer& B) const;

	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	EBNRaceState State = EBNRaceState::Idle;
	FBNRacer Player;
	TArray<FBNRacer> Rivals;
	FBNDriftScorer DriftScore;
	double StateStart = 0.0;
	/** The beacon only triggers after the car has been outside it, so spawning or finishing on it does not restart. */
	bool bArmed = false;

	int32 ActiveBossRank = 0;
	int32 ActiveBossDriftTarget = 0;
	float ActiveBossSkill = 0.85f;
	EBNCarId ActiveBossCarId = EBNCarId::KuroganeMSpec;
	FBNComputedVehicleStats ActiveBossStats;
	FLinearColor ActiveBossTint = FLinearColor::White;
	int32 LastCountdownDigit = 0;
	bool bUsingHoloRaceMeshes = false;
};
