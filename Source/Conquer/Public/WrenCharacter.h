#pragma once

#include "CoreMinimal.h"
#include "ConquerCharacterBase.h"
#include "InputActionValue.h"
#include "WrenCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UStaticMesh;

/**
 * The player. Out of lock-on she fights crowds hack-and-slash style; locked on, the right stick (or mouse)
 * picks her guard direction and her attacks come from that side, For Honor style.
 *
 * Input actions and key bindings are built in code so the prototype runs without any editor assets.
 */
UCLASS(Config = Game)
class CONQUER_API AWrenCharacter : public AConquerCharacterBase
{
	GENERATED_BODY()

public:
	AWrenCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsPlayerTeam() const override { return true; }
	virtual void OnLandedAttack(AConquerCharacterBase* Target, EHitOutcome Outcome, float DamageDealt) override;

	UFUNCTION(BlueprintPure, Category = "Wren")
	AConquerCharacterBase* GetLockTarget() const { return LockTarget; }

	UFUNCTION(BlueprintPure, Category = "Wren")
	bool IsLockedOn() const { return LockTarget != nullptr; }

	/** Swaps her weapon: the pitchfork she starts Marrowford with, or the sword. */
	void EquipPitchfork(bool bPitchfork);

	bool HasPitchfork() const { return bPitchforkEquipped; }

	/** Multiplies the damage of every attack (the smithy's whetstone). */
	void SetDamageBonus(float Multiplier);

	/** Switches Rage on for good and unleashes it at once (the gate captain fight). */
	void UnlockRage();

	/**
	 * The guard button (F / LB). Pressing it is a guard input: a blow landing on that side within the parry window is
	 * parried. Held, she blocks. Out of lock-on her guard turns to whatever is about to hit her.
	 */
	void SetGuardHeld(bool bHeld);

	/** Starts a dodge as the dodge button does (towards her movement input, or backwards). */
	bool TryDodge();

	virtual void OnCombatImpact(float Strength) override;

	/** Whether a heavy now would execute this enemy (staggered, wounded, close). */
	bool CanExecute(const AConquerCharacterBase* Target) const;

	/** Executes the best target in reach, if any: the paired finisher with its own camera. */
	bool TryExecute();

	bool IsExecuting() const { return ExecutionTarget != nullptr; }

	virtual bool IsUntouchable() const override { return IsExecuting(); }

	/** Shows or hides the weapon in her hand (the opening starts her empty-handed). */
	void ShowWeapon(bool bShow);

	/** The opening: knocked to the ground (holding a fall pose), or getting back up. */
	void SetKnockedDown(bool bDown);

	/** The pitchfork: a farm tool, so it hits a little softer and shorter than the sword. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Weapon")
	float PitchforkDamageScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Weapon")
	float PitchforkRangeScale = 0.9f;

	/** Rage gained per successful hit, per kill and per execution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Rage")
	float RageOnHit = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Rage")
	float RageOnKill = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Execution")
	float ExecuteHealthFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Execution")
	float ExecuteRange = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Execution")
	float ExecuteHeal = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Lock On")
	float LockOnRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Lock On")
	float LockedWalkSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Movement")
	float FreeWalkSpeed = 520.f;

	/** How far the stick or mouse must push before the guard switches side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wren|Lock On")
	float GuardSwitchThreshold = 1.5f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void HandleDeath(AActor* Killer) override;

private:
	void BuildInput();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLight();
	void OnHeavy();
	void OnGuardBreak();
	void OnFeint();
	void OnDodge();
	void OnToggleLock();
	void OnRage();
	void OnGuardPressed() { SetGuardHeld(true); }
	void OnGuardReleased() { SetGuardHeld(false); }
	void UpdateGuard();

	/** The side the blow most about to land on her comes from: an enemy mid-swing nearby, or an arrow in flight. */
	EGuardDirection SideOfNearestThreat() const;

	UFUNCTION()
	void HandleRageChanged(bool bActive);

	void Attack(EAttackKind Kind);
	void StartExecution(AConquerCharacterBase* Target);
	void UpdateExecution(float DeltaSeconds);
	void FinishExecution();
	AConquerCharacterBase* FindBestTarget(float MaxRange) const;
	void SetLockTarget(AConquerCharacterBase* NewTarget);
	void UpdateMovementMode();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HeavyAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GuardBreakAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FeintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LockAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RageAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GuardAction;

	UPROPERTY(Transient)
	TObjectPtr<AConquerCharacterBase> LockTarget;

	void ApplyWeaponStats();

	/** The pitchfork's mesh and grip, set in DefaultGame.ini like the sword's. */
	UPROPERTY(Config) FSoftObjectPath PitchforkMeshPath;
	UPROPERTY(Config) FVector PitchforkLocation = FVector::ZeroVector;
	UPROPERTY(Config) FRotator PitchforkRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SwordMesh;

	FAttackSpec SwordLight, SwordHeavy, SwordGuardBreak;
	TArray<TObjectPtr<UAnimMontage>> SwordLightCombo, SwordHeavyCombo;
	bool bPitchforkEquipped = false;

	// The camera kick: a shake and a punch of the field of view, fading out.
	float CameraKick = 0.f;
	float BaseFieldOfView = 90.f;

	// An execution in progress: who, which finisher, how far in, and its camera.
	UPROPERTY(Transient)
	TObjectPtr<AConquerCharacterBase> ExecutionTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ExecutionCamera;

	int32 ExecutionVariant = 0;
	float ExecutionTime = 0.f;
	float ExecutionKillAt = 1.f;
	float ExecutionEndAt = 1.8f;
	TArray<float> ExecutionBlows;
	int32 ExecutionBlowsDone = 0;
	bool bGuardHeld = false;
	float DamageBonus = 1.f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector2D GuardAccumulator = FVector2D::ZeroVector;
	int32 FreeSwingCount = 0;
};
