#pragma once

#include "CoreMinimal.h"
#include "EnemySoldier.h"
#include "GarrickBoss.generated.h"

class UAnimMontage;

/**
 * Sir Garrick Vane, the first boss. Phase one: three-hit combos, an unblockable shield bash and turtling behind
 * his shield (a guard break opens him). Phase two, after a roar: the shield is thrown down, two-handed overheads
 * with long recoveries, a charging thrust, and burning patches on the floor. At zero health he kneels and waits
 * for Wren's execution.
 */
UCLASS(Config = Game)
class CONQUER_API AGarrickBoss : public AEnemySoldier
{
	GENERATED_BODY()

public:
	AGarrickBoss();

	virtual void Tick(float DeltaSeconds) override;

	bool IsPhaseTwo() const { return bPhaseTwo; }
	bool IsDowned() const { return bDowned; }
	bool IsTurtling() const;

	/** Blocks from every side for a while; a guard break or any stagger ends it early. */
	void StartTurtle(float Seconds);

	/** Health at which phase two starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	float PhaseTwoHealth = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	int32 BurningPatchCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	float BurningPatchSpread = 800.f;

	/** Boss-only animations, set in DefaultGame.ini. */
	UPROPERTY(Config) FSoftObjectPath BashMontagePath;
	UPROPERTY(Config) FSoftObjectPath RoarMontagePath;
	UPROPERTY(Config) FSoftObjectPath KneelMontagePath;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDamaged(float Amount, AActor* DamageCauser) override;

private:
	UFUNCTION()
	void HandleDowned(AActor* DamageCauser);

	void EnterPhaseTwo();
	void DropShield();
	void SetPhaseOneMoves();
	void SetPhaseTwoMoves();

	UPROPERTY(Transient) TObjectPtr<UAnimMontage> BashMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> RoarMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> KneelMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> OverheadMontage;

	bool bPhaseTwo = false;
	bool bDowned = false;
	float TurtleTimeLeft = 0.f;
};
