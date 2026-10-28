#pragma once

#include "CoreMinimal.h"
#include "EnemyAIController.h"
#include "GarrickAIController.generated.h"

/** Sir Garrick's fighting: combos, bashes and turtling in phase one; overheads and charging thrusts in phase two. */
UCLASS()
class CONQUER_API AGarrickAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual bool TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist) override;

private:
	int32 ComboHitsLeft = 0;
	int32 ComboIndex = 0;
	float ThrustCooldown = 3.f;
};
