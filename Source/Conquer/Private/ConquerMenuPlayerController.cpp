#include "ConquerMenuPlayerController.h"
#include "Conquer.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AConquerMenuPlayerController::AConquerMenuPlayerController()
{
	ArenaLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Arena.Arena")));
	KeepHallLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/KeepHall.KeepHall")));
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
	const bool bKeepHall = Key == EKeys::Two || Key == EKeys::NumPadTwo;
	const TSoftObjectPtr<UWorld>& Level = bKeepHall ? KeepHallLevel : ArenaLevel;
	UE_LOG(LogConquer, Log, TEXT("Menu: %s pressed, opening %s"), *Key.ToString(), *Level.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
}
