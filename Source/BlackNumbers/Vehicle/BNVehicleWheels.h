#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "BNVehicleWheels.generated.h"

/** Steered front wheel. Values fit the placeholder sports car; real cars get their own subclasses. */
UCLASS()
class BLACKNUMBERS_API UBNWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UBNWheelFront();
};

/** Driven rear wheel; carries the handbrake so the car can be thrown into slides. */
UCLASS()
class BLACKNUMBERS_API UBNWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UBNWheelRear();
};
