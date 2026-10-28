#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelTriggerVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class ELevelTrigger : uint8
{
	/** Wren comes back here after dying. */
	Checkpoint,
	/** Walking in ends the level. */
	LevelExit
};

/** A box that tells the game mode when Wren walks into it: a checkpoint or the level's exit. */
UCLASS()
class CONQUER_API ALevelTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	ALevelTriggerVolume();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	ELevelTrigger Kind = ELevelTrigger::Checkpoint;

	/** Checkpoints count up along the level; an earlier one never replaces a later one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FString Label;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Trigger")
	TObjectPtr<UBoxComponent> Box;
};
