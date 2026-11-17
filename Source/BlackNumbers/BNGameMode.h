#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BNGameMode.generated.h"

/** Spawns the player into the car. */
UCLASS()
class BLACKNUMBERS_API ABNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNGameMode();
};
