#pragma once

#include "CoreMinimal.h"
#include "ConquerCharacterBase.h"
#include "InputActionValue.h"
#include "WrenCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;

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

	UFUNCTION()
	void HandleRageChanged(bool bActive);

	void Attack(EAttackKind Kind);
	bool TryExecute();
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

	UPROPERTY(Transient)
	TObjectPtr<AConquerCharacterBase> LockTarget;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector2D GuardAccumulator = FVector2D::ZeroVector;
	int32 FreeSwingCount = 0;
};
