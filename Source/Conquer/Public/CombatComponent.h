#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CombatComponent.generated.h"

class AConquerCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHitResolved, AConquerCharacterBase*, Target, EHitOutcome, Outcome, float, Damage);

/**
 * The duel system: guard direction, attacks that move through windup / active / recovery,
 * blocking, parrying, guard breaks, feints, dodges and staggers.
 *
 * Hits are resolved by direction, not by animation collision: an attack from the left is blocked by a
 * left guard. That keeps the rules readable and frame-exact while the art is still placeholder.
 */
UCLASS(ClassGroup = (Conquer), meta = (BlueprintSpawnableComponent))
class CONQUER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Actions -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetGuardDirection(EGuardDirection NewDirection);

	/** Starts an attack from the given direction. Returns false if busy, staggered or out of stamina. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool StartAttack(EAttackKind Kind, EGuardDirection Direction);

	/** Cancels a heavy attack during its windup. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool Feint();

	/** Dashes in a world direction with brief invulnerability. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool Dodge(const FVector& WorldDirection);

	/** Interrupts whatever the owner is doing and leaves them open for a moment. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Stagger(float Seconds);

	/** Resolves an incoming attack against this character's guard, parry, dodge and stagger state. */
	EHitOutcome ReceiveAttack(AConquerCharacterBase* Attacker, const FAttackSpec& Spec, EAttackKind Kind, EGuardDirection Direction, float DamageMultiplier, bool bForceUnblockable);

	// --- State -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Combat")
	EGuardDirection GetGuardDirection() const { return GuardDirection; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	EAttackPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	EAttackKind GetAttackKind() const { return AttackKind; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	EGuardDirection GetAttackDirection() const { return AttackDirection; }

	/** Increases every time an attack starts, so the AI can react to each attack once. */
	int32 GetAttackSerial() const { return AttackSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsBusy() const { return Phase != EAttackPhase::None || IsStaggered() || IsDodging(); }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsStaggered() const { return StaggerTimeLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDodging() const { return DodgeTimeLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanStartAttack() const;

	/** True while the current attack will ignore guards (unblockable spec, or a heavy during Rage). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsCurrentAttackUnblockable() const;

	const FAttackSpec& GetSpec(EAttackKind Kind) const;

	/** Plays a one-off reaction (flinch, dodge, death) over whatever the owner is doing. */
	void PlayReaction(UAnimMontage* Montage, float PlayRate = 1.f);

	// --- Tuning ------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec LightAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec HeavyAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec GuardBreakAttack;

	/** Whether this character can block at all right now (the player only blocks while locked on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bGuardRaised = true;

	/** A heavy started this soon before an incoming hit, from the same side, parries it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float ParryWindow = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float BlockStaminaDrain = 8.f;

	/** Ignores flinches from normal hits. Set during Rage and on heavily armoured enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bHyperArmor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	float HitStaggerTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	float ParriedStaggerTime = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	float GuardBrokenStaggerTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeStaminaCost = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeInvulnerableTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeStrength = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> HeavyHitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> GuardBrokenMontage;

	/** Played when this character blocks a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> BlockMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnHitResolved OnHitResolved;

protected:
	virtual void BeginPlay() override;

private:
	void EnterPhase(EAttackPhase NewPhase);
	void ResolveActiveFrame();
	void CancelAttack();
	void PlayAttackMontage();
	void StopAttackMontage();

	AConquerCharacterBase* GetOwnerCharacter() const;

	UPROPERTY(Transient)
	TObjectPtr<AConquerCharacterBase> OwnerCharacter;

	EGuardDirection GuardDirection = EGuardDirection::Top;
	EAttackPhase Phase = EAttackPhase::None;
	EAttackKind AttackKind = EAttackKind::Light;
	EGuardDirection AttackDirection = EGuardDirection::Top;
	float PhaseTimeLeft = 0.f;
	float PhaseElapsed = 0.f;
	float AttackStartTime = -100.f;
	int32 AttackSerial = 0;
	float StaggerTimeLeft = 0.f;
	float DodgeTimeLeft = 0.f;
};
