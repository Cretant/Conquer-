#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ConquerGameMode.generated.h"

class AEnemySoldier;
class AWrenCharacter;
class ALevelTriggerVolume;
enum class EPickupKind : uint8;

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

	/** Wren died. Return true when the game mode brings her back itself (checkpoints); false reloads the level. */
	virtual bool HandlePlayerDeath(AWrenCharacter* Wren) { return false; }

	/** Wren walked into a checkpoint or the level's exit. */
	virtual void NotifyLevelTrigger(ALevelTriggerVolume* Trigger) {}

	/** Wren picked something up (it has already been applied to her). */
	virtual void NotifyPickup(EPickupKind Kind, float Amount) {}

	// What the HUD draws over the game: a centred banner, a hint line, letterbox bars and a fade to black.
	void ShowBanner(const FString& Text, float Duration = 3.f);
	void ShowHint(const FString& Text, float Duration = 6.f);
	const FString& GetBanner() const { return Banner; }
	const FString& GetHint() const { return Hint; }
	float GetBannerAlpha() const { return MessageAlpha(BannerStart, BannerEnd); }
	float GetHintAlpha() const { return MessageAlpha(HintStart, HintEnd); }

	/** The full controls list in the corner; levels that teach controls as they go turn it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowControlsHint = true;

	/** 0 shows the game, 1 is black. */
	float FadeAlpha = 0.f;

	/** Cinematic bars; the HUD also hides Wren's bars while they're up. */
	bool bLetterbox = false;

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

	float MessageAlpha(float Start, float End) const;
	FString Banner;
	FString Hint;
	float BannerStart = 0.f, BannerEnd = 0.f;
	float HintStart = 0.f, HintEnd = 0.f;

	TMap<TWeakObjectPtr<AActor>, float> AttackTokenExpiry;
	int32 AliveInWave = 0;
};
