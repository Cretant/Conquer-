#include "CombatComponent.h"
#include "ConquerCharacterBase.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "Conquer.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	LightAttack.Windup = 0.25f;
	LightAttack.Active = 0.1f;
	LightAttack.Recovery = 0.3f;
	LightAttack.Damage = 12.f;
	LightAttack.StaminaCost = 8.f;

	HeavyAttack.Windup = 0.6f;
	HeavyAttack.Active = 0.12f;
	HeavyAttack.Recovery = 0.5f;
	HeavyAttack.Damage = 28.f;
	HeavyAttack.StaminaCost = 18.f;
	HeavyAttack.Range = 180.f;

	GuardBreakAttack.Windup = 0.15f;
	GuardBreakAttack.Active = 0.1f;
	GuardBreakAttack.Recovery = 0.45f;
	GuardBreakAttack.Damage = 0.f;
	GuardBreakAttack.StaminaCost = 12.f;
	GuardBreakAttack.Range = 90.f;
	GuardBreakAttack.ArcHalfAngle = 45.f;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AConquerCharacterBase>(GetOwner());
}

AConquerCharacterBase* UCombatComponent::GetOwnerCharacter() const
{
	return OwnerCharacter;
}

const FAttackSpec& UCombatComponent::GetSpec(EAttackKind Kind) const
{
	switch (Kind)
	{
	case EAttackKind::Heavy:      return HeavyAttack;
	case EAttackKind::GuardBreak: return GuardBreakAttack;
	default:                      return LightAttack;
	}
}

bool UCombatComponent::CanStartAttack() const
{
	if (IsStaggered() || IsDodging() || !OwnerCharacter || OwnerCharacter->IsDead())
	{
		return false;
	}
	// Light attacks chain: a new attack may start once the previous one is into its recovery.
	return Phase == EAttackPhase::None || (Phase == EAttackPhase::Recovery && PhaseElapsed > 0.1f);
}

bool UCombatComponent::IsCurrentAttackUnblockable() const
{
	if (Phase == EAttackPhase::None)
	{
		return false;
	}
	if (GetSpec(AttackKind).bUnblockable)
	{
		return true;
	}
	const URageComponent* Rage = OwnerCharacter ? OwnerCharacter->GetRage() : nullptr;
	return AttackKind == EAttackKind::Heavy && Rage && Rage->IsRageActive();
}

void UCombatComponent::SetGuardDirection(EGuardDirection NewDirection)
{
	GuardDirection = NewDirection;
}

bool UCombatComponent::StartAttack(EAttackKind Kind, EGuardDirection Direction)
{
	if (!CanStartAttack())
	{
		return false;
	}

	const FAttackSpec& Spec = GetSpec(Kind);
	if (!OwnerCharacter->GetAttributes()->TrySpendStamina(Spec.StaminaCost))
	{
		return false;
	}

	StopAttackMontage();
	AttackKind = Kind;
	AttackDirection = Direction;
	AttackStartTime = GetWorld()->GetTimeSeconds();
	++AttackSerial;
	EnterPhase(EAttackPhase::Windup);
	PlayAttackMontage();
	return true;
}

bool UCombatComponent::Feint()
{
	if (Phase != EAttackPhase::Windup || AttackKind != EAttackKind::Heavy)
	{
		return false;
	}
	OwnerCharacter->GetAttributes()->DrainStamina(5.f);
	CancelAttack();
	return true;
}

bool UCombatComponent::Dodge(const FVector& WorldDirection)
{
	if (IsStaggered() || IsDodging() || Phase == EAttackPhase::Active || !OwnerCharacter || OwnerCharacter->IsDead())
	{
		return false;
	}
	if (!OwnerCharacter->GetAttributes()->TrySpendStamina(DodgeStaminaCost))
	{
		return false;
	}

	// Dodging out of a windup or recovery is allowed: it is the main way to escape a bad commit.
	CancelAttack();

	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -OwnerCharacter->GetActorForwardVector();
	}
	DodgeTimeLeft = DodgeInvulnerableTime;
	OwnerCharacter->LaunchCharacter(Dir * DodgeStrength, true, false);
	PlayReaction(DodgeMontage, 1.3f);
	return true;
}

void UCombatComponent::Stagger(float Seconds)
{
	CancelAttack();
	StaggerTimeLeft = FMath::Max(StaggerTimeLeft, Seconds);
}

void UCombatComponent::CancelAttack()
{
	if (Phase != EAttackPhase::None)
	{
		StopAttackMontage();
	}
	Phase = EAttackPhase::None;
	PhaseTimeLeft = 0.f;
	PhaseElapsed = 0.f;
}

void UCombatComponent::EnterPhase(EAttackPhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.f;

	const FAttackSpec& Spec = GetSpec(AttackKind);
	switch (NewPhase)
	{
	case EAttackPhase::Windup:   PhaseTimeLeft = Spec.Windup; break;
	case EAttackPhase::Active:   PhaseTimeLeft = Spec.Active; ResolveActiveFrame(); break;
	case EAttackPhase::Recovery: PhaseTimeLeft = Spec.Recovery; break;
	default:                     PhaseTimeLeft = 0.f; break;
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StaggerTimeLeft = FMath::Max(0.f, StaggerTimeLeft - DeltaTime);
	DodgeTimeLeft = FMath::Max(0.f, DodgeTimeLeft - DeltaTime);

	if (Phase != EAttackPhase::None)
	{
		PhaseElapsed += DeltaTime;
		PhaseTimeLeft -= DeltaTime;
		if (PhaseTimeLeft <= 0.f)
		{
			switch (Phase)
			{
			case EAttackPhase::Windup:   EnterPhase(EAttackPhase::Active); break;
			case EAttackPhase::Active:   EnterPhase(EAttackPhase::Recovery); break;
			case EAttackPhase::Recovery: EnterPhase(EAttackPhase::None); break;
			default: break;
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && OwnerCharacter && Phase != EAttackPhase::None && Phase != EAttackPhase::Recovery)
	{
		// Placeholder for attack animations: a line from the side the swing comes from.
		const FVector Origin = OwnerCharacter->GetActorLocation();
		const FVector Fwd = OwnerCharacter->GetActorForwardVector();
		const FVector Right = OwnerCharacter->GetActorRightVector();
		FVector From = Origin + FVector(0, 0, 100.f);
		switch (AttackDirection)
		{
		case EGuardDirection::Left:  From += -Right * 80.f; break;
		case EGuardDirection::Right: From += Right * 80.f; break;
		default:                     From += FVector(0, 0, 60.f); break;
		}
		const FColor Color = Phase == EAttackPhase::Active ? FColor::Red : (IsCurrentAttackUnblockable() ? FColor::Orange : FColor::Yellow);
		const FVector To = Origin + Fwd * GetSpec(AttackKind).Range + FVector(0, 0, 40.f);
		DrawDebugLine(GetWorld(), From, To, Color, false, -1.f, 0, AttackKind == EAttackKind::Heavy ? 6.f : 3.f);
	}
#endif
}

void UCombatComponent::ResolveActiveFrame()
{
	if (!OwnerCharacter)
	{
		return;
	}

	const FAttackSpec& Spec = GetSpec(AttackKind);
	const FVector Origin = OwnerCharacter->GetActorLocation();
	const FVector Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Spec.ArcHalfAngle));

	const URageComponent* Rage = OwnerCharacter->GetRage();
	const bool bRaging = Rage && Rage->IsRageActive();
	const float DamageMultiplier = bRaging ? Rage->DamageMultiplier : 1.f;
	const bool bForceUnblockable = bRaging && AttackKind == EAttackKind::Heavy;

	for (TActorIterator<AConquerCharacterBase> It(GetWorld()); It; ++It)
	{
		AConquerCharacterBase* Target = *It;
		if (Target == OwnerCharacter || Target->IsDead() || Target->IsPlayerTeam() == OwnerCharacter->IsPlayerTeam())
		{
			continue;
		}

		FVector ToTarget = Target->GetActorLocation() - Origin;
		ToTarget.Z = 0.f;
		const float Reach = Spec.Range + OwnerCharacter->GetCapsuleRadius() + Target->GetCapsuleRadius();
		if (ToTarget.Size() > Reach || FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) < CosHalfAngle)
		{
			continue;
		}

		const float HealthBefore = Target->GetAttributes()->Health;
		const EHitOutcome Outcome = Target->GetCombat()->ReceiveAttack(OwnerCharacter, Spec, AttackKind, AttackDirection, DamageMultiplier, bForceUnblockable);
		const float Dealt = HealthBefore - Target->GetAttributes()->Health;
		OnHitResolved.Broadcast(Target, Outcome, Dealt);
		OwnerCharacter->OnLandedAttack(Target, Outcome, Dealt);

		// Light attacks and guard breaks only connect with one target; heavies sweep.
		if (AttackKind != EAttackKind::Heavy)
		{
			break;
		}
	}
}

EHitOutcome UCombatComponent::ReceiveAttack(AConquerCharacterBase* Attacker, const FAttackSpec& Spec, EAttackKind Kind, EGuardDirection Direction, float DamageMultiplier, bool bForceUnblockable)
{
	if (!OwnerCharacter || OwnerCharacter->IsDead())
	{
		return EHitOutcome::Ignored;
	}

	if (IsDodging())
	{
		return EHitOutcome::Dodged;
	}

	const URageComponent* MyRage = OwnerCharacter->GetRage();
	const bool bRaging = MyRage && MyRage->IsRageActive();

	if (Kind == EAttackKind::GuardBreak)
	{
		// Guard breaks beat anyone standing still or recovering; an attack already swinging wins.
		if (Phase == EAttackPhase::Windup || Phase == EAttackPhase::Active || bRaging)
		{
			return EHitOutcome::Ignored;
		}
		Stagger(GuardBrokenStaggerTime);
		PlayReaction(GuardBrokenMontage);
		return EHitOutcome::GuardBroken;
	}

	const bool bUnblockable = Spec.bUnblockable || bForceUnblockable;

	// Parry: a heavy started from the same side just before the hit lands. Rage gives up parrying.
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bRaging && Phase == EAttackPhase::Windup && AttackKind == EAttackKind::Heavy
		&& AttackDirection == Direction && Now - AttackStartTime <= ParryWindow)
	{
		CancelAttack();
		if (Attacker)
		{
			Attacker->GetCombat()->Stagger(ParriedStaggerTime);
			Attacker->GetCombat()->PlayReaction(Attacker->GetCombat()->GuardBrokenMontage);
		}
		return EHitOutcome::Parried;
	}

	// Block: guard raised on the matching side and not mid-attack.
	const bool bCanBlock = bGuardRaised && !bRaging && !IsStaggered()
		&& (Phase == EAttackPhase::None || Phase == EAttackPhase::Recovery);
	if (!bUnblockable && bCanBlock && GuardDirection == Direction)
	{
		OwnerCharacter->GetAttributes()->DrainStamina(BlockStaminaDrain);
		PlayReaction(BlockMontage, 1.5f);
		return EHitOutcome::Blocked;
	}

	const float Damage = Spec.Damage * DamageMultiplier;
	OwnerCharacter->GetAttributes()->ApplyDamage(Damage, Attacker);
	if (!OwnerCharacter->IsDead() && !bHyperArmor && !bRaging)
	{
		Stagger(Kind == EAttackKind::Heavy ? HitStaggerTime * 1.6f : HitStaggerTime);
		PlayReaction(Kind == EAttackKind::Heavy ? HeavyHitReactMontage : HitReactMontage);
	}
	return EHitOutcome::Hit;
}

void UCombatComponent::PlayAttackMontage()
{
	const FAttackSpec& Spec = GetSpec(AttackKind);
	UAnimMontage* Montage = nullptr;
	switch (AttackDirection)
	{
	case EGuardDirection::Left:  Montage = Spec.MontageLeft; break;
	case EGuardDirection::Right: Montage = Spec.MontageRight; break;
	default:                     Montage = Spec.MontageTop; break;
	}

	USkeletalMeshComponent* Mesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (Montage && Anim && Spec.TotalTime() > 0.f)
	{
		Anim->Montage_Play(Montage, Montage->GetPlayLength() / Spec.TotalTime());
	}
}

void UCombatComponent::PlayReaction(UAnimMontage* Montage, float PlayRate)
{
	USkeletalMeshComponent* Mesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (Montage && Anim)
	{
		Anim->Montage_Play(Montage, PlayRate);
	}
}

void UCombatComponent::StopAttackMontage()
{
	USkeletalMeshComponent* Mesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		Anim->Montage_Stop(0.15f);
	}
}
