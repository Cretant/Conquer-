#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatTypes.h"
#include "ConquerCharacterBase.generated.h"

class UCombatComponent;
class UAttributesComponent;
class URageComponent;
class UAnimMontage;

/** Shared base for Wren and every enemy: health, stamina, the duel system and death. */
UCLASS(Abstract, Config = Game)
class CONQUER_API AConquerCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AConquerCharacterBase();

	UCombatComponent* GetCombat() const { return Combat; }
	UAttributesComponent* GetAttributes() const { return Attributes; }
	URageComponent* GetRage() const { return Rage; }

	UFUNCTION(BlueprintPure, Category = "Conquer")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Conquer")
	virtual bool IsPlayerTeam() const { return false; }

	float GetCapsuleRadius() const;

	/** Called by the combat component after one of this character's attacks resolves. */
	virtual void OnLandedAttack(AConquerCharacterBase* Target, EHitOutcome Outcome, float DamageDealt) {}

	/** Kills this character outright (used by executions). */
	void Kill(AActor* Killer);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleDamaged(float Amount, AActor* DamageCauser);

	UFUNCTION()
	virtual void HandleDeath(AActor* Killer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAttributesComponent> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<URageComponent> Rage;

	/** Placeholder body, set in DefaultGame.ini so it can change without recompiling. */
	UPROPERTY(Config)
	FSoftObjectPath BodyMeshPath;

	UPROPERTY(Config)
	FSoftClassPath AnimClassPath;

	/** Placeholder combat animations, also set in DefaultGame.ini. Montages set on the components win. */
	UPROPERTY(Config) FSoftObjectPath LightLeftMontagePath;
	UPROPERTY(Config) FSoftObjectPath LightTopMontagePath;
	UPROPERTY(Config) FSoftObjectPath LightRightMontagePath;
	UPROPERTY(Config) FSoftObjectPath HeavyMontagePath;
	UPROPERTY(Config) FSoftObjectPath GuardBreakMontagePath;
	UPROPERTY(Config) FSoftObjectPath DodgeMontagePath;
	UPROPERTY(Config) FSoftObjectPath HitReactMontagePath;
	UPROPERTY(Config) FSoftObjectPath HeavyHitReactMontagePath;
	UPROPERTY(Config) FSoftObjectPath GuardBrokenMontagePath;
	UPROPERTY(Config) TArray<FSoftObjectPath> DeathMontagePaths;

	/** One is picked at random on death; with none, the body ragdolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	float CorpseLifeSpan = 10.f;

private:
	void ApplyPlaceholderBody();
	void ApplyPlaceholderAnimations();
};
