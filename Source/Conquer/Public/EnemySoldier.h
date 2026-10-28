#pragma once

#include "CoreMinimal.h"
#include "ConquerCharacterBase.h"
#include "EnemySoldier.generated.h"

/** A king's soldier. Fodder by default: tune the properties below (or subclass) for tougher archetypes. */
UCLASS(Config = Game)
class CONQUER_API AEnemySoldier : public AConquerCharacterBase
{
	GENERATED_BODY()

public:
	AEnemySoldier();

	/** Distance the soldier tries to keep from its target while fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float EngageRange = 170.f;

	/** Chance to switch guard to the side an incoming attack comes from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ReactionTimeMin = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ReactionTimeMax = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackCooldownMin = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackCooldownMax = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeavyChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardBreakChance = 0.05f;

	/** Sides this enemy attacks from. The gate captain only swings from the top and the right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<EGuardDirection> AttackDirections;

	/** Bosses ignore the squad's attack-token limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bIgnoresAttackTokens = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString DisplayName = TEXT("Soldier");

	/** Shown the first time one of these joins a fight in a level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString FirstSightHint;
};

/**
 * A shield guard: his shield stops every blockable blow from in front, so Wren has to guard break him or get round
 * to his side. He turns slowly to make that possible.
 */
UCLASS(Config = Game)
class CONQUER_API AShieldSoldier : public AEnemySoldier
{
	GENERATED_BODY()

public:
	AShieldSoldier();

	virtual void Tick(float DeltaSeconds) override;

	/** Degrees per second he turns to face Wren. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float TurnRate = 100.f;
};

/** The gate captain, mini-boss of the opening: slow, hard-hitting, readable. */
UCLASS(Config = Game)
class CONQUER_API AEnemyCaptain : public AEnemySoldier
{
	GENERATED_BODY()

public:
	AEnemyCaptain();
};
