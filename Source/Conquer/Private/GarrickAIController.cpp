#include "GarrickAIController.h"
#include "GarrickBoss.h"
#include "CombatComponent.h"

void AGarrickAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ThrustCooldown = FMath::Max(0.f, ThrustCooldown - DeltaSeconds);
}

bool AGarrickAIController::TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist)
{
	AGarrickBoss* Boss = Cast<AGarrickBoss>(Me);
	if (!Boss || Boss->IsDowned() || Boss->IsTurtling())
	{
		return false;
	}

	if (!Boss->IsPhaseTwo())
	{
		if (Dist > Me->EngageRange + 60.f)
		{
			return false;
		}

		// A combo is three light swings from alternating sides, each starting as the last one recovers.
		if (ComboHitsLeft > 0)
		{
			static const EGuardDirection Sides[] = { EGuardDirection::Left, EGuardDirection::Right, EGuardDirection::Top };
			--ComboHitsLeft;
			MyCombat->StartAttack(EAttackKind::Light, Sides[ComboIndex++ % 3]);
			AttackCooldown = ComboHitsLeft > 0 ? 0.f : FMath::FRandRange(1.2f, 2.0f);
			return false;
		}

		// Against a raised guard he sometimes grabs for it: the blue tell she can counter.
		if (Target->GetCombat()->bGuardRaised && FMath::FRand() < 0.25f && MyCombat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top))
		{
			AttackCooldown = 0.35f;
			return false;
		}

		const float Roll = FMath::FRand();
		if (Roll < 0.45f)
		{
			ComboHitsLeft = 3;
			ComboIndex = FMath::RandRange(0, 2);
			AttackCooldown = 0.f;
			return false;
		}
		if (Roll < 0.75f)
		{
			return MyCombat->StartAttack(EAttackKind::Heavy, EGuardDirection::Top);
		}
		Boss->StartTurtle(2.5f);
		AttackCooldown = 2.8f;
		return false;
	}

	// Phase two: from across the room he charges in with a thrust.
	if (Dist > 550.f && ThrustCooldown <= 0.f)
	{
		FVector ToTarget = Target->GetActorLocation() - Me->GetActorLocation();
		ToTarget.Z = 0.f;
		Me->SetActorRotation(ToTarget.Rotation());
		if (MyCombat->StartAttack(EAttackKind::Thrust, EGuardDirection::Top))
		{
			Me->LaunchCharacter(ToTarget.GetSafeNormal() * FMath::Min(2200.f, Dist * 3.f), true, false);
			ThrustCooldown = 6.f;
			return true;
		}
	}
	if (Dist > Me->EngageRange + 60.f)
	{
		return false;
	}
	if (Target->GetCombat()->bGuardRaised && FMath::FRand() < 0.15f && MyCombat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top))
	{
		AttackCooldown = 0.3f;
		return false;
	}
	const EGuardDirection Side = Me->AttackDirections[FMath::RandRange(0, Me->AttackDirections.Num() - 1)];
	return MyCombat->StartAttack(FMath::FRand() < 0.6f ? EAttackKind::Heavy : EAttackKind::Light, Side);
}
