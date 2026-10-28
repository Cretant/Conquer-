#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ConquerAnimInstance.generated.h"

class UAnimSequence;

/**
 * Native locomotion for characters without an animation blueprint: idle, walk and jog blended by ground speed and
 * by direction (forward, back, left, right, for strafing while locked on), with montages (attacks, reactions,
 * deaths) played over the top in the default slot.
 * The clips are set per character in DefaultGame.ini.
 */
UCLASS()
class CONQUER_API UConquerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Jog;

	/** Directional clips; any left empty falls back to the forward one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> JogBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> JogLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> JogRight;

	/** Ground speeds (cm/s) the walk and jog clips were made for; playback is scaled to the real speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float WalkSpeed = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float JogSpeed = 450.f;

	float GetGroundSpeed() const { return GroundSpeed; }

	/** Movement direction relative to where the character faces, in degrees: 0 forward, 90 right, 180 back. */
	float GetMoveDirection() const { return MoveDirection; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	float GroundSpeed = 0.f;
	float MoveDirection = 0.f;
};
