#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BNRaceDefinition.generated.h"

UENUM(BlueprintType)
enum class EBNRaceDiscipline : uint8
{
	Sprint,  // point to point through the gates
	Circuit, // laps; gate 0 is the start/finish line
	Drift,   // score as many drift points as possible before the time runs out
};

/** One race as data: new races need no code. Created by Tools/build_test_map.py for the test track. */
UCLASS(BlueprintType)
class BLACKNUMBERS_API UBNRaceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race")
	EBNRaceDiscipline Discipline = EBNRaceDiscipline::Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "1", EditCondition = "Discipline == EBNRaceDiscipline::Circuit"))
	int32 Laps = 2;

	/** Drift races end when this runs out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "5", Units = "Seconds", EditCondition = "Discipline == EBNRaceDiscipline::Drift"))
	float TimeLimitSeconds = 60.f;

	/** Where the car waits through the countdown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FTransform Grid;

	/** Gates in order; each gate's forward axis is the driving direction. Unused by drift races. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<FTransform> Checkpoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "200", Units = "Centimeters"))
	float GateWidth = 1600.f;

	/**
	 * World points the rivals drive through, in driving order (a loop for circuits). Empty: grid, then
	 * straight through the gates. A sprint line should run on past the finish so rivals cross it at speed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rivals")
	TArray<FVector> RacingLine;

	/** One entry per rival: 0 = cautious (early braking, slow launch), 1 = on the grip limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rivals", meta = (ClampMin = "0", ClampMax = "1"))
	TArray<float> RivalSkills;
};
