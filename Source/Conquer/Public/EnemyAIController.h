#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

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

private:
	float AttackCooldown = 2.f;
	float ReactTimer = 0.f;
	int32 LastSeenAttackSerial = -1;
	bool bPendingGuardReaction = false;
	float StrafeSign = 1.f;
	float StrafeTimer = 0.f;
};
