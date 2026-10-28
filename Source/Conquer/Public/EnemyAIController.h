#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class AEnemySoldier;
class AConquerCharacterBase;
class UCombatComponent;

/**
 * Prototype duel AI: close in, keep spacing, read the player's attacks and guard against them,
 * and attack when the squad allows it. Moves without a nav mesh, so any flat test map works.
 */
UCLASS()
class CONQUER_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	/**
	 * Picks and starts an attack once the cooldown is over and the combat component is free.
	 * Returns true if one started, which rolls a new cooldown; overrides may set AttackCooldown themselves instead.
	 */
	virtual bool TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist);

	float AttackCooldown = 2.f;

private:
	float ReactTimer = 0.f;
	int32 LastSeenAttackSerial = -1;
	bool bPendingGuardReaction = false;
	float StrafeSign = 1.f;
	float StrafeTimer = 0.f;
};
