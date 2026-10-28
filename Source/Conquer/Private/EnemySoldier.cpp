#include "EnemySoldier.h"
#include "EnemyAIController.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

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
	// Long enough to see coming (it flashes blue) and to counter with her own.
	Combat->GuardBreakAttack.Windup = 0.4f;

	AttackDirections = { EGuardDirection::Left, EGuardDirection::Top, EGuardDirection::Right };
}

AShieldSoldier::AShieldSoldier()
{
	DisplayName = TEXT("Shield Guard");
	FirstSightHint = TEXT("A shield guard blocks everything from the front: guard break him (E / X) or dodge round to his side");

	Attributes->MaxHealth = 70.f;
	Combat->bFrontalShield = true;
	Combat->LightAttack.Damage = 10.f;
	Combat->LightAttack.Windup = 0.5f;
	Combat->HeavyAttack.Damage = 20.f;
	GetCharacterMovement()->MaxWalkSpeed = 260.f;

	// He turns himself, slowly, instead of snapping to face her.
	bUseControllerRotationYaw = false;
	HeavyChance = 0.2f;
	BlockChance = 0.2f;
	AttackCooldownMin = 1.8f;
	AttackCooldownMax = 3.2f;
}

void AShieldSoldier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || IsDead() || Combat->IsStaggered())
	{
		return;
	}
	const float Desired = (Player->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
	SetActorRotation(FRotator(0.f, FMath::FixedTurn(GetActorRotation().Yaw, Desired, TurnRate * DeltaSeconds), 0.f));
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
