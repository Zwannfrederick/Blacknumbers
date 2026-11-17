#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BNGameMode.generated.h"

/** Spawns the player into the car with the dashboard HUD. */
UCLASS()
class BLACKNUMBERS_API ABNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNGameMode();
};
