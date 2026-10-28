#include "EnemySoldier.h"
#include "EnemyAIController.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemySoldier::AEnemySoldier()
{
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Soldiers always face their target so their guard reads clearly.
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 330.f;

	Attributes->MaxHealth = 45.f;
	Combat->bGuardRaised = true;
	Combat->LightAttack.Damage = 9.f;
	Combat->HeavyAttack.Damage = 18.f;
	// Slower windups than Wren's so they are readable.
	Combat->LightAttack.Windup = 0.45f;
	Combat->HeavyAttack.Windup = 0.8f;

	AttackDirections = { EGuardDirection::Left, EGuardDirection::Top, EGuardDirection::Right };
}

AEnemyCaptain::AEnemyCaptain()
{
	DisplayName = TEXT("Gate Captain");

	Attributes->MaxHealth = 220.f;
	Combat->bHyperArmor = true;
	Combat->HeavyAttack.Damage = 35.f;
	Combat->HeavyAttack.Windup = 0.9f;
	Combat->HeavyAttack.Range = 220.f;
	Combat->LightAttack.Damage = 14.f;
	Combat->LightAttack.Windup = 0.5f;
	Combat->HitStaggerTime = 0.15f;
	GetCharacterMovement()->MaxWalkSpeed = 280.f;

	EngageRange = 200.f;
	BlockChance = 0.6f;
	HeavyChance = 0.55f;
	GuardBreakChance = 0.1f;
	AttackCooldownMin = 1.2f;
	AttackCooldownMax = 2.4f;
	AttackDirections = { EGuardDirection::Top, EGuardDirection::Right };
	bIgnoresAttackTokens = true;
}
