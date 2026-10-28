#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatTypes.h"
#include "ConquerCharacterBase.generated.h"

class UCombatComponent;
class UAttributesComponent;
class URageComponent;
class UAnimMontage;
class UStaticMeshComponent;

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

	/** The sword in the right hand. Purely visual: hits are resolved by the combat component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Weapon;

	/** The shield on the left arm, for characters that carry one. Also purely visual. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Shield;

	/** Placeholder body, set in DefaultGame.ini so it can change without recompiling. */
	UPROPERTY(Config)
	FSoftObjectPath BodyMeshPath;

	UPROPERTY(Config)
	FSoftClassPath AnimClassPath;

	/** Capsule and body placement for the body above (defaults fit the mannequins). */
	UPROPERTY(Config)
	float CapsuleRadius = 42.f;

	UPROPERTY(Config)
	float CapsuleHalfHeight = 96.f;

	UPROPERTY(Config)
	FVector MeshLocation = FVector(0.f, 0.f, -96.f);

	UPROPERTY(Config)
	FRotator MeshRotation = FRotator(0.f, -90.f, 0.f);

	/** Locomotion clips for bodies driven by UConquerAnimInstance instead of an animation blueprint. */
	UPROPERTY(Config) FSoftObjectPath IdleAnimPath;
	UPROPERTY(Config) FSoftObjectPath WalkAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogAnimPath;

	/** Placeholder weapon and where it sits relative to the hand bone. */
	UPROPERTY(Config)
	FSoftObjectPath WeaponMeshPath;

	UPROPERTY(Config)
	FName WeaponSocket = TEXT("hand_r");

	UPROPERTY(Config)
	FVector WeaponLocation = FVector::ZeroVector;

	UPROPERTY(Config)
	FRotator WeaponRotation = FRotator::ZeroRotator;

	/** Optional shield, placed like the weapon. No mesh, no shield. */
	UPROPERTY(Config)
	FSoftObjectPath ShieldMeshPath;

	UPROPERTY(Config)
	FName ShieldSocket = TEXT("hand_l");

	UPROPERTY(Config)
	FVector ShieldLocation = FVector::ZeroVector;

	UPROPERTY(Config)
	FRotator ShieldRotation = FRotator::ZeroRotator;

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
	UPROPERTY(Config) FSoftObjectPath BlockMontagePath;
	UPROPERTY(Config) TArray<FSoftObjectPath> DeathMontagePaths;

	/** "From=>To" path rewrites tried before each montage path; the rewritten montage is used if it exists locally. */
	UPROPERTY(Config) TArray<FString> LocalAnimOverrides;

	/** One is picked at random on death; with none, the body ragdolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	float CorpseLifeSpan = 10.f;

private:
	void ApplyPlaceholderBody();
	void ApplyPlaceholderAnimations();
};
