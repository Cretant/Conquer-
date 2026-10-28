#include "GarrickBoss.h"
#include "GarrickAIController.h"
#include "BurningPatch.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "Conquer.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGarrickBoss::AGarrickBoss()
{
	AIControllerClass = AGarrickAIController::StaticClass();
	DisplayName = TEXT("Sir Garrick Vane");

	Attributes->MaxHealth = 600.f;
	Combat->bHyperArmor = false;
	Combat->HitStaggerTime = 0.25f;
	Combat->LightAttack.Damage = 14.f;
	Combat->LightAttack.Windup = 0.45f;
	GetCharacterMovement()->MaxWalkSpeed = 260.f;

	EngageRange = 210.f;
	BlockChance = 0.55f;
	AttackCooldownMin = 1.0f;
	AttackCooldownMax = 2.0f;
	AttackDirections = { EGuardDirection::Left, EGuardDirection::Top, EGuardDirection::Right };
	bIgnoresAttackTokens = true;
}

void AGarrickBoss::BeginPlay()
{
	Super::BeginPlay();

	// He doesn't die from damage: at the last point of health he kneels and waits to be executed.
	Attributes->MinHealth = 1.f;
	Attributes->OnDowned.AddDynamic(this, &AGarrickBoss::HandleDowned);

	BashMontage = BashMontagePath.IsValid() ? LoadAnimation<UAnimMontage>(BashMontagePath) : nullptr;
	RoarMontage = RoarMontagePath.IsValid() ? LoadAnimation<UAnimMontage>(RoarMontagePath) : nullptr;
	KneelMontage = KneelMontagePath.IsValid() ? LoadAnimation<UAnimMontage>(KneelMontagePath) : nullptr;
	OverheadMontage = Combat->HeavyAttack.MontageTop;
	SetPhaseOneMoves();
}

bool AGarrickBoss::IsTurtling() const
{
	return Combat->bBlockAllDirections;
}

void AGarrickBoss::StartTurtle(float Seconds)
{
	Combat->bBlockAllDirections = true;
	TurtleTimeLeft = Seconds;
	Combat->PlayReaction(Combat->BlockMontage, 0.7f);
}

void AGarrickBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TurtleTimeLeft > 0.f)
	{
		TurtleTimeLeft -= DeltaSeconds;
		if (TurtleTimeLeft <= 0.f || !Combat->bBlockAllDirections)
		{
			Combat->bBlockAllDirections = false;
			TurtleTimeLeft = 0.f;
		}
	}
}

void AGarrickBoss::SetPhaseOneMoves()
{
	// The heavy is a shield bash: quick to read, can't be blocked, so it must be dodged.
	FAttackSpec& Bash = Combat->HeavyAttack;
	Bash.Windup = 0.7f;
	Bash.Active = 0.12f;
	Bash.Recovery = 0.7f;
	Bash.Damage = 26.f;
	Bash.Range = 150.f;
	Bash.bUnblockable = true;
	if (BashMontage)
	{
		Bash.MontageLeft = Bash.MontageTop = Bash.MontageRight = BashMontage;
	}
}

void AGarrickBoss::SetPhaseTwoMoves()
{
	// Two-handed overheads: heavier and slower, with long recoveries to punish.
	FAttackSpec& Overhead = Combat->HeavyAttack;
	Overhead.Windup = 1.0f;
	Overhead.Active = 0.14f;
	Overhead.Recovery = 1.2f;
	Overhead.Damage = 42.f;
	Overhead.Range = 230.f;
	Overhead.bUnblockable = false;
	if (OverheadMontage)
	{
		Overhead.MontageLeft = Overhead.MontageTop = Overhead.MontageRight = OverheadMontage;
	}

	Combat->LightAttack.Damage = 18.f;
	Combat->LightAttack.Windup = 0.55f;
	Combat->ThrustAttack.Damage = 32.f;
	Combat->ThrustAttack.Recovery = 1.0f;
	Combat->bHyperArmor = true;
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
}

void AGarrickBoss::HandleDamaged(float Amount, AActor* DamageCauser)
{
	Super::HandleDamaged(Amount, DamageCauser);

	if (!bPhaseTwo && !bDowned && Attributes->Health <= PhaseTwoHealth)
	{
		EnterPhaseTwo();
	}
}

void AGarrickBoss::EnterPhaseTwo()
{
	bPhaseTwo = true;
	UE_LOG(LogConquer, Log, TEXT("%s roars and throws down his shield"), *DisplayName);

	Combat->Stagger(1.6f);
	Combat->PlayReaction(RoarMontage);
	DropShield();
	SetPhaseTwoMoves();

	// Stand-ins for the braziers he kicks over: burning patches scattered round him.
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (int32 i = 0; i < BurningPatchCount; ++i)
	{
		const float Angle = (360.f / BurningPatchCount) * i + FMath::FRandRange(-25.f, 25.f);
		const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * FMath::FRandRange(BurningPatchSpread * 0.4f, BurningPatchSpread);
		GetWorld()->SpawnActor<ABurningPatch>(ABurningPatch::StaticClass(), Feet + Offset + FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator);
	}
}

void AGarrickBoss::DropShield()
{
	if (!Shield->GetStaticMesh())
	{
		return;
	}
	// Thrown down beside him, lying flat (the shield's face points along its X axis).
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Shield->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Shield->SetWorldLocationAndRotation(Feet - GetActorRightVector() * 110.f + FVector(0.f, 0.f, 2.f), FRotator(90.f, GetActorRotation().Yaw, 0.f));
}

void AGarrickBoss::HandleDowned(AActor* DamageCauser)
{
	bDowned = true;
	UE_LOG(LogConquer, Log, TEXT("%s drops to one knee"), *DisplayName);

	Combat->bHyperArmor = false;
	Combat->bBlockAllDirections = false;
	Combat->Stagger(9999.f);
	Combat->PlayReaction(KneelMontage);
}
