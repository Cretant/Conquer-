#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ConquerGameMode.generated.h"

class AEnemySoldier;

/**
 * Runs the prototype arena: a wave of soldiers, then the gate captain once they are all dead.
 * Also limits how many enemies may attack Wren at once so group fights stay readable.
 */
UCLASS()
class CONQUER_API AConquerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AConquerGameMode();

	/** Returns true if this enemy may start an attack now. */
	bool TryTakeAttackToken(AActor* Enemy);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	int32 MaxSimultaneousAttackers = 2;

	/** How long an enemy holds its attack token after starting a swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float AttackTokenDuration = 1.2f;

	/** Skip the arena waves when the level already has enemies placed in it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	bool bSpawnArenaWaves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	int32 SoldiersInFirstWave = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float SpawnRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TSubclassOf<AEnemySoldier> SoldierClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TSubclassOf<AEnemySoldier> CaptainClass;

protected:
	virtual void BeginPlay() override;

	AEnemySoldier* SpawnEnemy(TSubclassOf<AEnemySoldier> Class, const FVector& Location);
	FVector GetArenaCentre() const;

	/** Once set, the next enemy death ends the fight instead of bringing on the captain. */
	bool bCaptainSpawned = false;

private:
	void SpawnFirstWave();
	void SpawnCaptain();

	UFUNCTION()
	void HandleEnemyDeath(AActor* Killer);

	// Headless screenshots (-ConquerShots), taken one after another.
	void ScheduleNextShot();
	void TakeShot();
	TArray<float> PendingShotTimes;
	FTimerHandle ShotTimer;

	TMap<TWeakObjectPtr<AActor>, float> AttackTokenExpiry;
	int32 AliveInWave = 0;
};
