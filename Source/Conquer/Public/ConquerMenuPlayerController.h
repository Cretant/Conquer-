#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ConquerMenuPlayerController.generated.h"

/** Title screen controller: any key, mouse button or gamepad button starts a new game in Marrowford. */
UCLASS()
class CONQUER_API AConquerMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AConquerMenuPlayerController();

	/** The first level: a new game. Every key but the test ones opens it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> FirstLevel;

	/** Test fights: 1 opens the arena, 2 the Keep Hall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> ArenaLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> KeepHallLevel;

	/** Ignores keys for this long after the menu opens, so a key still held from before doesn't skip it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	float InputDelay = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnAnyKey(FKey Key);

	bool bStarting = false;
};
