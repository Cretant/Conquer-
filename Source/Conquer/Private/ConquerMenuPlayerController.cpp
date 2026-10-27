#include "ConquerMenuPlayerController.h"
#include "Conquer.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AConquerMenuPlayerController::AConquerMenuPlayerController()
{
	ArenaLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Arena.Arena")));
	bShowMouseCursor = false;
}

void AConquerMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
}

void AConquerMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AConquerMenuPlayerController::OnAnyKey);
}

void AConquerMenuPlayerController::OnAnyKey(FKey Key)
{
	if (bStarting || GetWorld()->GetRealTimeSeconds() < InputDelay)
	{
		return;
	}

	bStarting = true;
	UE_LOG(LogConquer, Log, TEXT("Menu: %s pressed, opening %s"), *Key.ToString(), *ArenaLevel.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, ArenaLevel);
}
