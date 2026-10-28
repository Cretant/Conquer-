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
class UWeaponTrailComponent;
class UPhysicalAnimationComponent;

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

	/** Ranged attacks: called on the active frame of a Shot. Archers loose an arrow. */
	virtual void FireProjectile(const FAttackSpec& Spec) {}

	virtual void Tick(float DeltaSeconds) override;

	/** A blow lands: the upper body is knocked along Direction by physics, then pulled back to the animation. */
	void PlayPhysicalHit(const FVector& Direction, float Strength);

	/** If the next blow kills, the body goes limp and is thrown this way (velocity, cm/s) instead of a death animation. */
	FVector DeathImpulse = FVector::ZeroVector;

	/** Something big happened to this character in a fight (the player's camera kicks). */
	virtual void OnCombatImpact(float Strength) {}

	/** Blows pass through her while true (mid-execution). */
	virtual bool IsUntouchable() const { return false; }

	UStaticMeshComponent* GetWeapon() const { return Weapon; }

	/** A one-off montage over whatever she is doing (executions, cutscenes). */
	float PlayMontage(UAnimMontage* Montage, float Rate = 1.f);

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

	/** The blade's trail through each swing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponTrailComponent> Trail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	UPROPERTY(Config) FSoftObjectPath TrailMaterialPath;

	/** Hits knock the body about with physics (from this bone down), fading back to the animation. */
	UPROPERTY(Config) bool bPhysicalHitReactions = true;

	UPROPERTY(Config) FName PhysicalHitBone = TEXT("spine_01");

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

	/** Multiplies the body's colour (the PBR material's "Tint"), so two characters sharing a model read differently. */
	UPROPERTY(Config)
	FLinearColor BodyTint = FLinearColor::White;

	/** Locomotion clips for bodies driven by UConquerAnimInstance instead of an animation blueprint. */
	UPROPERTY(Config) FSoftObjectPath IdleAnimPath;
	UPROPERTY(Config) FSoftObjectPath WalkAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogAnimPath;
	UPROPERTY(Config) FSoftObjectPath WalkBackAnimPath;
	UPROPERTY(Config) FSoftObjectPath WalkLeftAnimPath;
	UPROPERTY(Config) FSoftObjectPath WalkRightAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogBackAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogLeftAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogRightAnimPath;

	/**
	 * The fuller locomotion set (UConquerAnimInstance): diagonals (forward-right, back-right, back-left, forward-left),
	 * starts, stops (forward, right, back, left; each on the left then the right foot), turns on the spot (left 90,
	 * right 90, left 180, right 180) and idle breaks. Local-only clips: missing ones are skipped quietly.
	 */
	UPROPERTY(Config) TArray<FSoftObjectPath> WalkDiagonalAnimPaths;
	UPROPERTY(Config) TArray<FSoftObjectPath> JogDiagonalAnimPaths;
	UPROPERTY(Config) FSoftObjectPath WalkStartAnimPath;
	UPROPERTY(Config) FSoftObjectPath JogStartAnimPath;
	UPROPERTY(Config) TArray<FSoftObjectPath> WalkStopAnimPaths;
	UPROPERTY(Config) TArray<FSoftObjectPath> JogStopAnimPaths;
	UPROPERTY(Config) TArray<FSoftObjectPath> TurnAnimPaths;
	UPROPERTY(Config) TArray<FSoftObjectPath> IdleBreakAnimPaths;

	/** The fuller set's walk and jog speeds (cm/s), used when its diagonals are present. */
	UPROPERTY(Config) float LocomotionWalkSpeed = 0.f;
	UPROPERTY(Config) float LocomotionJogSpeed = 0.f;

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

	UPROPERTY(Config)
	float ShieldScale = 1.f;

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
	UPROPERTY(Config) FSoftObjectPath ThrustMontagePath;
	UPROPERTY(Config) TArray<FSoftObjectPath> DeathMontagePaths;

	/** The fuller moveset (Tools/build_anims.py): combos, parry and recoil, the dodge attack, and per-character extras. */
	UPROPERTY(Config) TArray<FSoftObjectPath> LightComboMontagePaths;
	UPROPERTY(Config) TArray<FSoftObjectPath> HeavyComboMontagePaths;
	UPROPERTY(Config) FSoftObjectPath ParryMontagePath;
	UPROPERTY(Config) FSoftObjectPath ParriedRecoilMontagePath;
	UPROPERTY(Config) FSoftObjectPath DodgeAttackMontagePath;
	UPROPERTY(Config) FSoftObjectPath ShotMontagePath;
	UPROPERTY(Config) FSoftObjectPath ShotReleaseMontagePath;
	UPROPERTY(Config) FSoftObjectPath ThrustLightMontagePath;
	UPROPERTY(Config) FSoftObjectPath ThrustHeavyMontagePath;
	UPROPERTY(Config) FSoftObjectPath GetUpMontagePath;

	/** Attacks slide in to a target this far off (UCombatComponent::LungeRange). */
	UPROPERTY(Config) float LungeRange = 0.f;

	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ShotReleaseMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ThrustLightMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ThrustHeavyMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> GetUpMontage;

	/** "From=>To" path rewrites tried before each montage path; the rewritten montage is used if it exists locally. */
	UPROPERTY(Config) TArray<FString> LocalAnimOverrides;

	/** One is picked at random on death; with none, the body ragdolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conquer")
	float CorpseLifeSpan = 10.f;

	/** Loads an animation, preferring a local-only version (LocalAnimOverrides) when one exists. */
	template <typename T>
	T* LoadAnimation(const FSoftObjectPath& Path) const { return Cast<T>(LoadAnimationObject(Path)); }

private:
	UObject* LoadAnimationObject(const FSoftObjectPath& Path) const;

	/** Loads a local-only clip if it's on this PC; nothing (and no warning) if it isn't. */
	UAnimSequence* LoadOptionalClip(const FSoftObjectPath& Path) const;
	void ApplyPlaceholderBody();
	void ApplyPlaceholderAnimations();
	void UpdatePhysicalHit(float DeltaSeconds);
	float PhysicalHitWeight = 0.f;
};
