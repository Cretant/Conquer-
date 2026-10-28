#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ConquerAnimInstance.generated.h"

class UAnimSequence;

/**
 * Native locomotion for characters without an animation blueprint, with montages (attacks, reactions, deaths)
 * played over the top in the default slot. The clips are set per character in DefaultGame.ini.
 *
 * With the basic set (idle, walk and jog each four ways) it blends by speed and direction. Given more it does more:
 * - eight-way walks and jogs (the diagonals), blended an eighth of the way round at a time;
 * - a start clip when she sets off, a stop on whichever foot is planted when she lets go of the stick;
 * - turning on the spot: standing still, the body lags the character's facing and steps round with a turn clip
 *   once it has fallen far enough behind;
 * - idle breaks after standing a while;
 * - foot placement: each foot is traced to the ground, the pelvis drops for the lower one and the other leg bends.
 * Playback speed follows the real ground speed so the feet keep pace with the capsule.
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

	/** Diagonals, forward-right, back-right, back-left, forward-left. With all four the blend goes eight ways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> WalkDiagonals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> JogDiagonals;

	/** Setting off forwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TObjectPtr<UAnimSequence> JogStart;

	/** Stops by direction and planted foot: forward L/R, right L/R, back L/R, left L/R. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> WalkStops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> JogStops;

	/** Turning on the spot: left 90, right 90, left 180, right 180. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> Turns;

	/** Played now and then while standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	TArray<TObjectPtr<UAnimSequence>> IdleBreaks;

	/** Ground speeds (cm/s) the walk and jog clips were made for; playback is scaled to the real speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float WalkSpeed = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float JogSpeed = 450.f;

	/** Standing still, the body steps round once it is this many degrees behind where the character faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float TurnThreshold = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	bool bFootPlacement = true;

	float GetGroundSpeed() const { return GroundSpeed; }

	/** Movement direction relative to where the character faces, in degrees: 0 forward, 90 right, 180 back. */
	float GetMoveDirection() const { return MoveDirection; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FConquerAnimInstanceProxy;

	void UpdateTurnInPlace(float ActorYaw, float DeltaSeconds);
	void UpdateFootPlacement(float DeltaSeconds);

	float GroundSpeed = 0.f;
	float MoveDirection = 0.f;
	bool bAccelerating = false;

	// Turning on the spot: where the feet point, and the turn being stepped.
	float BodyYaw = 0.f;
	float BodyYawOffset = 0.f;
	float TurnFrom = 0.f;
	float TurnTo = 0.f;
	float TurnElapsed = -1.f;
	float TurnLength = 0.f;
	int32 TurnClip = -1;
	bool bTurnStarted = false;

	// Foot placement: how far each foot's ground sits above (+) or below the capsule's floor, smoothed.
	float FootOffsetL = 0.f;
	float FootOffsetR = 0.f;
};
