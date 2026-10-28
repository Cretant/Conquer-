#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatTypes.h"
#include "ConquerGameMode.generated.h"

class AEnemySoldier;
class AConquerCharacterBase;
class USoundBase;
class UMaterialInterface;
class AWrenCharacter;
class ALevelTriggerVolume;
enum class EPickupKind : uint8;

/**
 * Runs the prototype arena: a wave of soldiers, then the gate captain once they are all dead.
 * Also limits how many enemies may attack Wren at once so group fights stay readable.
 */
UCLASS(Config = Game)
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

	/** Shows the hint after any hint already up has had its time. */
	void QueueHint(const FString& Text, float Duration = 6.f);

	/** Queues a hint the first time Key comes up in this level, and never again. */
	void ShowFirstTimeHint(FName Key, const FString& Text, float Duration = 7.f);

	/** An enemy just entered the fight: the first of each kind brings its hint. */
	void NotifyEnemySpawned(AEnemySoldier* Enemy);

	/** Freezes the two fighters (not the world) for a moment so a big hit lands. */
	void HitStop(float Seconds, AActor* One, AActor* Two = nullptr);

	/** Washes the screen with a colour (its alpha is the strength), fading out over Seconds. */
	void Flash(const FLinearColor& Color, float Seconds);

	/** A word in the middle of the screen for a moment: PARRIED, GUARD BROKEN... */
	void ShowCallout(const FString& Text);

	/** Sparks, sounds and camera kicks for a blow's outcome. */
	void PlayCombatEffects(EHitOutcome Outcome, EAttackKind Kind, AConquerCharacterBase* Attacker, AConquerCharacterBase* Defender);

	/** The whoosh of a swing as it comes through. */
	void PlaySwing(AActor* Swinger, EAttackKind Kind);

	/** Sparks at a point, sprayed along a direction. */
	void SpawnSparks(const FVector& Where, const FVector& Direction, float Strength, const FLinearColor& LightColor);

	const FString& GetCallout() const { return Callout; }
	float GetCalloutAlpha() const;
	FLinearColor GetFlash() const;
	const FString& GetBanner() const { return Banner; }
	const FString& GetHint() const { return Hint; }
	float GetBannerAlpha() const { return MessageAlpha(BannerStart, BannerEnd); }
	float GetHintAlpha() const { return MessageAlpha(HintStart, HintEnd); }

	/** The full controls list in the corner; levels that teach controls as they go turn it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowControlsHint = true;

	/** The last blow the player gave or took, and how many so far (for headless tests). */
	EHitOutcome LastPlayerOutcome = EHitOutcome::Ignored;
	int32 PlayerOutcomes = 0;

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

	/** Tools/build_fx.py: synthesised combat sounds and the spark material. */
	UPROPERTY(Config) FSoftObjectPath SwingSoundPath;
	UPROPERTY(Config) FSoftObjectPath HeavySwingSoundPath;
	UPROPERTY(Config) FSoftObjectPath HitSoundPath;
	UPROPERTY(Config) FSoftObjectPath BlockSoundPath;
	UPROPERTY(Config) FSoftObjectPath ParrySoundPath;
	UPROPERTY(Config) FSoftObjectPath ThudSoundPath;
	UPROPERTY(Config) FSoftObjectPath SparkMaterialPath;
	USoundBase* LoadSound(const FSoftObjectPath& Path);
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SparkMaterial;

	float MessageAlpha(float Start, float End) const;
	void ShowNextHint();
	TArray<TPair<FString, float>> PendingHints;
	TSet<FName> HintsShown;
	FTimerHandle HintTimer;
	FTimerHandle HitStopTimer;
	void EndHitStop();
	TMap<TWeakObjectPtr<AActor>, float> Frozen;
	FString Callout;
	float CalloutStart = -100.f;
	FLinearColor FlashColor = FLinearColor::Transparent;
	float FlashStart = -100.f, FlashSeconds = 0.f;
	FString Banner;
	FString Hint;
	float BannerStart = 0.f, BannerEnd = 0.f;
	float HintStart = 0.f, HintEnd = 0.f;

	TMap<TWeakObjectPtr<AActor>, float> AttackTokenExpiry;
	int32 AliveInWave = 0;
};
