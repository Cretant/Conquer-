#pragma once

#include "CoreMinimal.h"
#include "ConquerGameMode.h"
#include "MarrowfordGameMode.generated.h"

class AEncounterVolume;

/**
 * Marrowford, the first level. The opening cutscene plays at the farm, then the fights along the road to the
 * village gate start one by one as Wren walks into their triggers. Dying sends her back to the last checkpoint
 * with that stretch's fight reset. Rage stays locked until the gate captain has her on the ropes, and his sword
 * replaces her pitchfork once he falls.
 *
 * The level (Tools/build_marrowford.py) tags what the opening needs: IntroCam1 to IntroCam5, Family,
 * IntroSoldier (with an IntroTorch each), FarmFire (shown once the farm burns) and PitchforkProp.
 */
UCLASS()
class CONQUER_API AMarrowfordGameMode : public AConquerGameMode
{
	GENERATED_BODY()

public:
	AMarrowfordGameMode();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool HandlePlayerDeath(AWrenCharacter* Wren) override;
	virtual void NotifyLevelTrigger(ALevelTriggerVolume* Trigger) override;
	virtual void NotifyPickup(EPickupKind Kind, float Amount) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marrowford")
	float RespawnDelay = 3.f;

	/** Rage wakes the first time her health drops this low in the gate captain fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marrowford")
	float RageUnlockHealthFraction = 0.35f;

	/** Where the level ends up: back to the title screen for now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marrowford")
	TSoftObjectPtr<UWorld> MenuLevel;

protected:
	virtual void BeginPlay() override;

private:
	// The opening, one step at a time; any key skips to the end of it.
	void StartIntro();
	void RunIntroStep();
	void EndIntro();
	void OnSkipKey(FKey Key);
	void ViewFrom(FName CameraTag);
	void FadeTo(float Target, float Seconds);
	void MoveActor(AActor* Actor, const FVector& To, float Seconds);
	TArray<AActor*> Tagged(FName Tag) const;
	void SetShown(const TArray<AActor*>& Actors, bool bShown);

	// Wren, the fights and the checkpoints.
	AWrenCharacter* GetWren() const;
	void SetUpWren(AWrenCharacter* Wren);
	void Respawn();
	void FinishLevel();

	UFUNCTION()
	void HandleEncounterStarted(AEncounterVolume* Encounter);

	UFUNCTION()
	void HandleEncounterDone(AEncounterVolume* Encounter);

	UFUNCTION()
	void HandleWrenDamaged(float Amount, AActor* DamageCauser);

	// -ConquerMarrowfordTest walks the level headless: each fight started and cleared, a death and respawn, the end.
	void RunTestStep();

	struct FMove
	{
		TWeakObjectPtr<AActor> Actor;
		FVector From, To;
		float Start = 0.f;
		float Duration = 1.f;
	};
	TArray<FMove> Moves;
	float FadeFrom = 0.f, FadeTarget = 0.f, FadeStart = 0.f, FadeDuration = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEncounterVolume>> Encounters;

	TArray<TWeakObjectPtr<AActor>> IntroSoldiers;
	TArray<FVector> IntroSoldierHomes;
	FTimerHandle IntroTimer;
	int32 IntroStep = 0;
	bool bIntroDone = false;
	float IntroStartRealTime = 0.f;

	FTransform CheckpointTransform;
	bool bHasCheckpoint = false;
	int32 CheckpointOrder = -1;

	bool bBossFightActive = false;
	bool bRageUnlocked = false;
	bool bHasSword = false;
	float DamageBonus = 1.f;
	bool bFinished = false;

	bool bTestMode = false;
	FTimerHandle TestTimer;
	int32 TestStep = 0;
	int32 TestTries = 0;
};
