#include "BNGameMode.h"

#include "Vehicle/BNVehiclePawn.h"

ABNGameMode::ABNGameMode()
{
	DefaultPawnClass = ABNVehiclePawn::StaticClass();
}
