#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BNHUD.generated.h"

class ABNVehiclePawn;

/** What the dashboard shows, read straight from the car's physics state. */
struct FBNDashReadout
{
	int32 SpeedKmh = 0;
	FString Gear;
	int32 Rpm = 0;
	float RpmFraction = 0.f;
};

/** Lightweight canvas speedometer: speed, gear, rev bar. No widget assets. */
UCLASS()
class BLACKNUMBERS_API ABNHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	static FBNDashReadout MakeReadout(const ABNVehiclePawn& Car);
	static FString GearLabel(int32 Gear);
};
