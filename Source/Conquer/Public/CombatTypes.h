#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

class UAnimMontage;

/** The three guard and attack directions of the duel system. */
UENUM(BlueprintType)
enum class EGuardDirection : uint8
{
	Left,
	Top,
	Right
};

UENUM(BlueprintType)
enum class EAttackKind : uint8
{
	Light,
	Heavy,
	GuardBreak,
	/** A charging lunge; only bosses use it. */
	Thrust
};

/** An attack moves through windup (readable, can be parried or feinted), active (deals the hit) and recovery. */
UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
	None,
	Windup,
	Active,
	Recovery
};

UENUM(BlueprintType)
enum class EHitOutcome : uint8
{
	Hit,
	Blocked,
	Parried,
	Dodged,
	GuardBroken,
	Ignored
};

/** Timing and damage for one kind of attack. All tuning lives here so it can be changed without touching code. */
USTRUCT(BlueprintType)
struct FAttackSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Windup = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Active = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Recovery = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float Damage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float StaminaCost = 10.f;

	/** Reach in centimetres, measured from the attacker's capsule edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float Range = 160.f;

	/** Half-angle of the swing arc in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float ArcHalfAngle = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bUnblockable = false;

	/** Optional animation per direction. The montage is time-scaled to fit windup + active + recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Animation")
	TObjectPtr<UAnimMontage> MontageLeft = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Animation")
	TObjectPtr<UAnimMontage> MontageTop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Animation")
	TObjectPtr<UAnimMontage> MontageRight = nullptr;

	float TotalTime() const { return Windup + Active + Recovery; }
};
