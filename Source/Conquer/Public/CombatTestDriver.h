#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "CombatTestDriver.generated.h"

class AEnemySoldier;
class AWrenCharacter;

/**
 * -ConquerCombatTest: runs the duel mechanics headless in the arena and logs PASS or FAIL for each. Perfect parry,
 * late block, guard break and its counter, dodge cancels, i-frames and the dodge attack, the shield guard, and an
 * archer's arrows deflected, dodged and taken. The enemies are puppets (no AI) except the archer.
 *
 * -ConquerMoveTest instead walks Wren through her locomotion (setting off, running, stopping, a diagonal, turning on
 * the spot) in front of a fixed side camera, saving a frame every tenth of a second for a preview video.
 */
UCLASS(NotPlaceable)
class CONQUER_API ACombatTestDriver : public AActor
{
	GENERATED_BODY()

public:
	ACombatTestDriver();

	virtual void Tick(float DeltaSeconds) override;

private:
	AEnemySoldier* Spawn(TSubclassOf<AEnemySoldier> Class, float Distance, bool bKeepAI = false);
	void Reset();
	void Next();
	void Check(const TCHAR* Name, bool bPass, const FString& Detail = FString());
	void TickMoveTest(float DeltaSeconds);
	void Face(AActor* Actor, const AActor* Target) const;
	void Shot(const TCHAR* Name);
	EHitOutcome LastOutcome() const;
	bool NewOutcomeSince(int32 Count) const;

	UPROPERTY(Transient)
	TObjectPtr<AWrenCharacter> Wren;

	UPROPERTY(Transient)
	TObjectPtr<AEnemySoldier> Dummy;

	UPROPERTY(Transient)
	TObjectPtr<AEnemySoldier> Lineup;

	FVector Start = FVector::ZeroVector;
	int32 Step = 0;
	float StepTime = -1.f;
	int32 OutcomesBefore = 0;
	float Before = 0.f;
	int32 ArrowPhase = 0;
	int32 Passed = 0;
	int32 Failed = 0;

	bool bMoveTest = false;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FollowCamera;
	float MoveTime = 0.f;
	float NextFrame = 0.f;
	int32 Frames = 0;
};
