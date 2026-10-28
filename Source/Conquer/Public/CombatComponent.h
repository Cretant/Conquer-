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

	/** Dashes in a world direction with brief invulnerability. Cancels a light's recovery or an uncommitted heavy. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool Dodge(const FVector& WorldDirection);

	/** Whether a dodge may interrupt what the owner is doing now. */
	bool CanDodgeCancel() const;

	/** A light pressed now becomes a dodge attack. */
	bool CanDodgeAttack() const { return DodgeAttackWindowLeft > 0.f && !IsStaggered(); }

	/** The defender just pressed guard or switched guard side: a hit landing in the next moment is parried. */
	void NotifyGuardInput();

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

	/** The first part of a dodge, when hits pass through. */
	bool IsInvulnerable() const { return InvulnerableTimeLeft > 0.f; }

	float GetAttackStartTime() const { return AttackStartTime; }

	/** A frontal shield is up (shield guards): blocks from the front until a guard break or stagger. */
	bool IsShieldUp() const { return bFrontalShield && !IsStaggered(); }

	/** Which of the defender's sides a blow or arrow from this location arrives on. */
	static EGuardDirection SideFrom(const AActor* Defender, const FVector& FromLocation);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec ThrustAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec DodgeAttack;

	/** Archers: Range is how far they shoot; the arrow does the damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attacks")
	FAttackSpec ShotAttack;

	/** Whether this character can block at all right now (the player only blocks while locked on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bGuardRaised = true;

	/** Turtling: blocks blockable hits from every side, until a guard break or any stagger opens it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bBlockAllDirections = false;

	/** Only the player parries: guard input on the right side this soon before a hit lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bCanPerfectParry = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float PerfectParryWindow = 0.15f;

	/** Rage gained for a parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float ParryRage = 25.f;

	/** Starting your own guard break after theirs started (and no more than this before yours lands) counters it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float GuardBreakCounterWindow = 0.4f;

	/** Stamina a blocked light costs; heavies cost twice as much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float BlockStaminaDrain = 8.f;

	/** Shield guards: blocks every blockable hit from in front, whatever the side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	bool bFrontalShield = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defence")
	float FrontalShieldHalfAngle = 70.f;

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

	/** How long a dodge lasts; the i-frames are its first part. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeInvulnerableTime = 0.25f;

	/** A heavy can be dodged out of until this much of its windup has passed (its commit point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float HeavyCommitFraction = 0.6f;

	/** A light pressed during a dodge or this soon after becomes a dodge attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeAttackWindow = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Dodge")
	float DodgeAttackLunge = 900.f;

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

	/** Hit-stop, flash, callout and first-time hints for blows the player gives or takes. */
	EHitOutcome Report(AConquerCharacterBase* Attacker, EAttackKind Kind, EHitOutcome Outcome);

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
	float InvulnerableTimeLeft = 0.f;
	float DodgeAttackWindowLeft = 0.f;
	float LastGuardInputTime = -100.f;
};
