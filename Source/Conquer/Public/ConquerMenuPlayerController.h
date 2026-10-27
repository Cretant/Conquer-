#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ConquerMenuPlayerController.generated.h"

/** Title screen controller: any key, mouse button or gamepad button starts the arena. */
UCLASS()
class CONQUER_API AConquerMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AConquerMenuPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> ArenaLevel;

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
