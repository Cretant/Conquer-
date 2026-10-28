#include "EnemyAIController.h"
#include "EnemySoldier.h"
#include "CombatComponent.h"
#include "ConquerGameMode.h"
#include "Kismet/GameplayStatics.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AttackCooldown = FMath::FRandRange(1.f, 2.5f);
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AEnemySoldier* Me = Cast<AEnemySoldier>(GetPawn());
	AConquerCharacterBase* Target = Cast<AConquerCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Me || Me->IsDead() || !Target || Target->IsDead())
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	SetFocus(Target);
	UCombatComponent* MyCombat = Me->GetCombat();
	UCombatComponent* TheirCombat = Target->GetCombat();

	FVector ToTarget = Target->GetActorLocation() - Me->GetActorLocation();
	ToTarget.Z = 0.f;
	const float Dist = ToTarget.Size();
	const FVector Dir = ToTarget.GetSafeNormal();

	// Movement: close the gap, back off if crowding, circle a little at fighting range.
	if (!MyCombat->IsBusy())
	{
		if (Dist > Me->EngageRange + 40.f)
		{
			Me->AddMovementInput(Dir, 1.f);
		}
		else if (Dist < Me->EngageRange * 0.6f)
		{
			Me->AddMovementInput(-Dir, 0.6f);
		}
		else
		{
			StrafeTimer -= DeltaSeconds;
			if (StrafeTimer <= 0.f)
			{
				StrafeSign = -StrafeSign;
				StrafeTimer = FMath::FRandRange(1.f, 2.5f);
			}
			Me->AddMovementInput(FVector::CrossProduct(Dir, FVector::UpVector) * StrafeSign, 0.25f);
		}
	}

	// Defence: notice each new player attack once, then react after a human-ish delay.
	if (TheirCombat->GetPhase() == EAttackPhase::Windup && TheirCombat->GetAttackSerial() != LastSeenAttackSerial)
	{
		LastSeenAttackSerial = TheirCombat->GetAttackSerial();
		bPendingGuardReaction = FMath::FRand() < Me->BlockChance;
		ReactTimer = FMath::FRandRange(Me->ReactionTimeMin, Me->ReactionTimeMax);
	}
	if (bPendingGuardReaction)
	{
		ReactTimer -= DeltaSeconds;
		if (ReactTimer <= 0.f)
		{
			MyCombat->SetGuardDirection(TheirCombat->GetAttackDirection());
			bPendingGuardReaction = false;
		}
	}

	// Offence: attack when off cooldown and free to swing.
	AttackCooldown -= DeltaSeconds;
	if (AttackCooldown > 0.f || !MyCombat->CanStartAttack())
	{
		return;
	}
	if (TryAttack(Me, Target, MyCombat, Dist))
	{
		AttackCooldown = FMath::FRandRange(Me->AttackCooldownMin, Me->AttackCooldownMax);
	}
}

bool AEnemyAIController::TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist)
{
	// Soldiers attack when in range and the squad has a free attack token.
	if (Dist > Me->EngageRange + 60.f)
	{
		return false;
	}

	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	if (!Me->bIgnoresAttackTokens && GameMode && !GameMode->TryTakeAttackToken(Me))
	{
		AttackCooldown = 0.4f;
		return false;
	}

	EAttackKind Kind = EAttackKind::Light;
	const float Roll = FMath::FRand();
	if (Roll < Me->GuardBreakChance)
	{
		Kind = EAttackKind::GuardBreak;
	}
	else if (Roll < Me->GuardBreakChance + Me->HeavyChance)
	{
		Kind = EAttackKind::Heavy;
	}

	const EGuardDirection Side = Me->AttackDirections.Num() > 0
		? Me->AttackDirections[FMath::RandRange(0, Me->AttackDirections.Num() - 1)]
		: EGuardDirection::Top;

	return MyCombat->StartAttack(Kind, Side);
}
