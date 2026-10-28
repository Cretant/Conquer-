#include "ConquerMenuPlayerController.h"
#include "Conquer.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AConquerMenuPlayerController::AConquerMenuPlayerController()
{
	FirstLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Marrowford.Marrowford")));
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
	const TSoftObjectPtr<UWorld>* Level = &FirstLevel;
	if (Key == EKeys::One || Key == EKeys::NumPadOne)
	{
		Level = &ArenaLevel;
	}
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
	{
		Level = &KeepHallLevel;
	}
	UE_LOG(LogConquer, Log, TEXT("Menu: %s pressed, opening %s"), *Key.ToString(), *Level->ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, *Level);
}
