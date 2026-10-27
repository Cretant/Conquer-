#include "ConquerMenuGameMode.h"
#include "ConquerMenuHUD.h"
#include "ConquerMenuPlayerController.h"

AConquerMenuGameMode::AConquerMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AConquerMenuHUD::StaticClass();
	PlayerControllerClass = AConquerMenuPlayerController::StaticClass();
}
