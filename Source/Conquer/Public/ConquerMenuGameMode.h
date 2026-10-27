#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ConquerMenuGameMode.generated.h"

/** The title screen: no pawn, just the menu HUD and a controller that waits for any key. */
UCLASS()
class CONQUER_API AConquerMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AConquerMenuGameMode();
};
