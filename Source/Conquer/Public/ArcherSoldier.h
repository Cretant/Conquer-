#pragma once

#include "CoreMinimal.h"
#include "EnemySoldier.h"
#include "EnemyAIController.h"
#include "ArcherSoldier.generated.h"

class UPointLightComponent;
class AArrowProjectile;

/**
 * An archer: keeps his distance and looses burning arrows. The long draw is the tell (his bow glows and the
 * guard indicator shows the shot coming); Wren deflects an arrow with a timed guard, or dodges it.
 */
UCLASS(Config = Game)
class CONQUER_API AArcherSoldier : public AEnemySoldier
{
	GENERATED_BODY()

public:
	AArcherSoldier();

	virtual void Tick(float DeltaSeconds) override;
	virtual void FireProjectile(const FAttackSpec& Spec) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer")
	TSubclassOf<AArrowProjectile> ArrowClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer")
	float ArrowSpeed = 2600.f;

	/** How much of her movement he leads his shot by (0 aims at where she is). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer")
	float LeadFraction = 0.5f;

private:
	/** The arrow being lit while he draws. */
	UPROPERTY(VisibleAnywhere, Category = "Archer")
	TObjectPtr<UPointLightComponent> DrawGlow;
};

/** Holds the archer at range: shoots when he can see her, swings his bow if she gets close. */
UCLASS()
class CONQUER_API AArcherAIController : public AEnemyAIController
{
	GENERATED_BODY()

protected:
	virtual bool TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist) override;
};
