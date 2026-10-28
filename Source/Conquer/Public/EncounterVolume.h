#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterVolume.generated.h"

class UBoxComponent;
class AEnemySoldier;
class AEncounterVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEncounterEvent, AEncounterVolume*, Encounter);

/** One group of enemies in an encounter. */
USTRUCT(BlueprintType)
struct FEncounterWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	int32 Count = 1;

	/** Empty means the plain king's soldier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TSubclassOf<AEnemySoldier> EnemyClass;

	/** The wave comes in once no more than this many enemies of the earlier waves are still standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	int32 StartWhenAliveAtMost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	float Delay = 0.8f;
};

UENUM(BlueprintType)
enum class EEncounterState : uint8
{
	Waiting,
	Running,
	Done
};

/**
 * A fight that starts when Wren walks into the box: its waves spawn at the spawn points one after another.
 * A checkpoint respawn resets an unfinished encounter so it can be fought again from the start.
 */
UCLASS()
class CONQUER_API AEncounterVolume : public AActor
{
	GENERATED_BODY()

public:
	AEncounterVolume();

	virtual void Tick(float DeltaSeconds) override;

	/** Clears the fight's enemies and re-arms the trigger. Finished encounters stay finished. */
	void ResetEncounter();

	/** Starts the fight as if Wren had walked in. */
	void Start();

	EEncounterState GetState() const { return State; }

	/** Enemies of this fight still standing. */
	TArray<AEnemySoldier*> GetLivingEnemies() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FString EncounterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FEncounterWave> Waves;

	/** World positions the enemies appear at, used in turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FVector> SpawnPoints;

	/** Shown when the fight starts, e.g. the controls it teaches; several hints separated by | follow each other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FString StartHint;

	/** A boss fight: its name shows as a banner, and the game mode treats it specially. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	bool bBossFight = false;

	/** Blocks the way back once the fight starts: hidden and without collision until then. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<TObjectPtr<AActor>> CloseOnStart;

	/** Raised out of the way once the fight is won, like a portcullis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<TObjectPtr<AActor>> OpenOnDone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	float OpenHeight = 480.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	float OpenSeconds = 3.f;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnEncounterEvent OnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnEncounterEvent OnDone;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void SpawnWave(const FEncounterWave& Wave);
	int32 CountAlive() const;
	void SetBlockersActive(bool bActive);

	UPROPERTY(VisibleAnywhere, Category = "Encounter")
	TObjectPtr<UBoxComponent> Box;

	TArray<TWeakObjectPtr<AEnemySoldier>> Enemies;
	EEncounterState State = EEncounterState::Waiting;
	int32 NextWave = 0;
	float NextWaveTime = -1.f;
	int32 NextSpawnPoint = 0;

	float OpenElapsed = -1.f;
	TArray<FVector> OpenFrom;
};
