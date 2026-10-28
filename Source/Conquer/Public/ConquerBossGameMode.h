#pragma once

#include "CoreMinimal.h"
#include "ConquerGameMode.h"
#include "ConquerBossGameMode.generated.h"

/** A boss room: no waves, just the boss, who walks in a moment after the level starts. */
UCLASS()
class CONQUER_API AConquerBossGameMode : public AConquerGameMode
{
	GENERATED_BODY()

public:
	AConquerBossGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	TSubclassOf<AEnemySoldier> BossClass;

	/** How far in front of the player the boss appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	float BossDistance = 1000.f;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnBoss();
	void AdvanceBossTest();

	TWeakObjectPtr<AEnemySoldier> Boss;
	FTimerHandle TestTimer;
	int32 TestStep = 0;
};
