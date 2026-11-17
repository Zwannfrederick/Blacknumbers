#include "BNGameMode.h"

#include "UI/BNHUD.h"
#include "Vehicle/BNVehiclePawn.h"

ABNGameMode::ABNGameMode()
{
	DefaultPawnClass = ABNVehiclePawn::StaticClass();
	HUDClass = ABNHUD::StaticClass();
}
