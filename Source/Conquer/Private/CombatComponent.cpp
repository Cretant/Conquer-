#include "CombatComponent.h"
#include "ConquerCharacterBase.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "ConquerGameMode.h"
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

	ThrustAttack.Windup = 0.45f;
	ThrustAttack.Active = 0.15f;
	ThrustAttack.Recovery = 0.9f;
	ThrustAttack.Damage = 30.f;
	ThrustAttack.StaminaCost = 0.f;
	ThrustAttack.Range = 200.f;
	ThrustAttack.ArcHalfAngle = 35.f;

	DodgeAttack.Windup = 0.12f;
	DodgeAttack.Active = 0.1f;
	DodgeAttack.Recovery = 0.35f;
	DodgeAttack.Damage = 14.f;
	DodgeAttack.StaminaCost = 6.f;
	DodgeAttack.Range = 190.f;
	DodgeAttack.ArcHalfAngle = 60.f;

	ShotAttack.Windup = 1.1f;
	ShotAttack.Active = 0.05f;
	ShotAttack.Recovery = 0.8f;
	ShotAttack.Damage = 14.f;
	ShotAttack.StaminaCost = 0.f;
	ShotAttack.Range = 2600.f;
	ShotAttack.ArcHalfAngle = 180.f;
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
	case EAttackKind::Thrust:      return ThrustAttack;
	case EAttackKind::DodgeAttack: return DodgeAttack;
	case EAttackKind::Shot:        return ShotAttack;
	default:                       return LightAttack;
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

void UCombatComponent::NotifyGuardInput()
{
	LastGuardInputTime = GetWorld()->GetTimeSeconds();
}

EGuardDirection UCombatComponent::SideFrom(const AActor* Defender, const FVector& FromLocation)
{
	const FVector Local = Defender->GetActorTransform().InverseTransformPositionNoScale(FromLocation);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	if (FMath::Abs(Angle) <= 45.f)
	{
		return EGuardDirection::Top;
	}
	return Angle > 0.f ? EGuardDirection::Right : EGuardDirection::Left;
}

bool UCombatComponent::StartAttack(EAttackKind Kind, EGuardDirection Direction)
{
	// A dodge attack comes out of a dodge; everything else waits for the owner to be free.
	const bool bFromDodge = Kind == EAttackKind::DodgeAttack;
	if (bFromDodge ? (!CanDodgeAttack() || !OwnerCharacter || OwnerCharacter->IsDead()) : !CanStartAttack())
	{
		return false;
	}

	const FAttackSpec& Spec = GetSpec(Kind);
	if (!OwnerCharacter->GetAttributes()->TrySpendStamina(Spec.StaminaCost))
	{
		return false;
	}
	if (bFromDodge)
	{
		DodgeTimeLeft = 0.f;
		DodgeAttackWindowLeft = 0.f;
		OwnerCharacter->LaunchCharacter(OwnerCharacter->GetActorForwardVector().GetSafeNormal2D() * DodgeAttackLunge, true, false);
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

bool UCombatComponent::CanDodgeCancel() const
{
	switch (Phase)
	{
	case EAttackPhase::None:
		return true;
	case EAttackPhase::Windup:
		// A heavy can be pulled out of before its commit point; a light is too quick to bother.
		return AttackKind == EAttackKind::Heavy && PhaseElapsed < HeavyAttack.Windup * HeavyCommitFraction;
	case EAttackPhase::Recovery:
		// A light's follow-through can be cut short; a whiffed heavy or guard break is punishable.
		return AttackKind == EAttackKind::Light || AttackKind == EAttackKind::DodgeAttack;
	default:
		return false;
	}
}

bool UCombatComponent::Dodge(const FVector& WorldDirection)
{
	if (IsStaggered() || IsDodging() || !CanDodgeCancel() || !OwnerCharacter || OwnerCharacter->IsDead())
	{
		return false;
	}
	if (!OwnerCharacter->GetAttributes()->TrySpendStamina(DodgeStaminaCost))
	{
		return false;
	}

	CancelAttack();

	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -OwnerCharacter->GetActorForwardVector();
	}
	DodgeTimeLeft = DodgeDuration;
	InvulnerableTimeLeft = DodgeInvulnerableTime;
	DodgeAttackWindowLeft = DodgeDuration + DodgeAttackWindow;
	OwnerCharacter->LaunchCharacter(Dir * DodgeStrength, true, false);
	PlayReaction(DodgeMontage, 1.3f);
	return true;
}

void UCombatComponent::Stagger(float Seconds)
{
	CancelAttack();
	bBlockAllDirections = false;
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
	InvulnerableTimeLeft = FMath::Max(0.f, InvulnerableTimeLeft - DeltaTime);
	DodgeAttackWindowLeft = FMath::Max(0.f, DodgeAttackWindowLeft - DeltaTime);

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
	if (AttackKind == EAttackKind::Shot)
	{
		OwnerCharacter->FireProjectile(Spec);
		return;
	}

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

	if (IsInvulnerable())
	{
		return Report(Attacker, Kind, EHitOutcome::Dodged);
	}

	const URageComponent* MyRage = OwnerCharacter->GetRage();
	const bool bRaging = MyRage && MyRage->IsRageActive();
	const float Now = GetWorld()->GetTimeSeconds();

	if (Kind == EAttackKind::GuardBreak)
	{
		// Countered: the defender's own guard break started after this one and close enough to its landing.
		UCombatComponent* TheirCombat = Attacker ? Attacker->GetCombat() : nullptr;
		if (TheirCombat && AttackKind == EAttackKind::GuardBreak && Phase != EAttackPhase::None
			&& AttackStartTime > TheirCombat->GetAttackStartTime() && Now - AttackStartTime <= GuardBreakCounterWindow)
		{
			CancelAttack();
			TheirCombat->Stagger(TheirCombat->GuardBrokenStaggerTime);
			TheirCombat->PlayReaction(TheirCombat->GuardBrokenMontage);
			return Report(Attacker, Kind, EHitOutcome::Countered);
		}

		// Guard breaks beat anyone standing still or recovering; an attack already swinging wins.
		if (Phase == EAttackPhase::Windup || Phase == EAttackPhase::Active || bRaging)
		{
			return EHitOutcome::Ignored;
		}
		Stagger(GuardBrokenStaggerTime);
		PlayReaction(GuardBrokenMontage);
		return Report(Attacker, Kind, EHitOutcome::GuardBroken);
	}

	const bool bUnblockable = Spec.bUnblockable || bForceUnblockable;
	const bool bFree = !IsStaggered() && (Phase == EAttackPhase::None || Phase == EAttackPhase::Recovery);

	// Perfect parry: guard pressed (or switched) to the blow's side just before it lands.
	if (bCanPerfectParry && !bUnblockable && bFree && GuardDirection == Direction && Now - LastGuardInputTime <= PerfectParryWindow)
	{
		// Arrows are deflected; a sword arm is knocked wide and left open.
		if (Attacker && Kind != EAttackKind::Shot)
		{
			UCombatComponent* TheirCombat = Attacker->GetCombat();
			TheirCombat->Stagger(TheirCombat->ParriedStaggerTime);
			TheirCombat->PlayReaction(TheirCombat->GuardBrokenMontage);
		}
		if (URageComponent* Rage = OwnerCharacter->GetRage())
		{
			Rage->AddRage(ParryRage);
		}
		LastGuardInputTime = -100.f;
		return Report(Attacker, Kind, EHitOutcome::Parried);
	}

	// A shield guard's shield stops anything from in front, whichever side it comes from.
	if (bFrontalShield && !bUnblockable && Attacker && !IsStaggered() && Phase != EAttackPhase::Active)
	{
		const FVector ToAttacker = (Attacker->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
		const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(FrontalShieldHalfAngle));
		if (FVector::DotProduct(OwnerCharacter->GetActorForwardVector().GetSafeNormal2D(), ToAttacker) >= CosHalfAngle)
		{
			OwnerCharacter->GetAttributes()->DrainStamina(BlockStaminaDrain * 0.5f);
			PlayReaction(BlockMontage, 1.5f);
			return Report(Attacker, Kind, EHitOutcome::Blocked);
		}
	}

	// Block: guard raised on the matching side and not mid-attack. Costs stamina, more for a heavy.
	if (!bUnblockable && bGuardRaised && !bRaging && bFree && (GuardDirection == Direction || bBlockAllDirections))
	{
		OwnerCharacter->GetAttributes()->DrainStamina(BlockStaminaDrain * (Kind == EAttackKind::Heavy ? 2.f : 1.f));
		PlayReaction(BlockMontage, 1.5f);
		return Report(Attacker, Kind, EHitOutcome::Blocked);
	}

	const float Damage = Spec.Damage * DamageMultiplier;
	OwnerCharacter->GetAttributes()->ApplyDamage(Damage, Attacker);
	if (!OwnerCharacter->IsDead() && !bHyperArmor && !bRaging)
	{
		Stagger(Kind == EAttackKind::Heavy ? HitStaggerTime * 1.6f : HitStaggerTime);
		PlayReaction(Kind == EAttackKind::Heavy ? HeavyHitReactMontage : HitReactMontage);
	}
	return Report(Attacker, Kind, EHitOutcome::Hit);
}

EHitOutcome UCombatComponent::Report(AConquerCharacterBase* Attacker, EAttackKind Kind, EHitOutcome Outcome)
{
	const bool bPlayerDefends = OwnerCharacter && OwnerCharacter->IsPlayerTeam();
	const bool bPlayerAttacks = Attacker && Attacker->IsPlayerTeam();
	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	if (!GameMode || (!bPlayerDefends && !bPlayerAttacks))
	{
		return Outcome;
	}
	GameMode->LastPlayerOutcome = Outcome;
	++GameMode->PlayerOutcomes;

	UE_LOG(LogConquer, Log, TEXT("Combat: %s's %s on %s: %s"), Attacker ? *Attacker->GetName() : TEXT("someone"),
		*UEnum::GetDisplayValueAsText(Kind).ToString(), *OwnerCharacter->GetName(), *UEnum::GetDisplayValueAsText(Outcome).ToString());

	switch (Outcome)
	{
	case EHitOutcome::Parried:
		GameMode->HitStop(0.14f);
		GameMode->Flash(FLinearColor(1.f, 0.82f, 0.4f, 0.3f), 0.25f);
		GameMode->ShowCallout(Kind == EAttackKind::Shot ? TEXT("DEFLECTED") : TEXT("PARRIED"));
		GameMode->ShowFirstTimeHint(TEXT("Parried"), Kind == EAttackKind::Shot
			? TEXT("Deflected. Close the distance before he draws again")
			: TEXT("Parried: he reels. Punish him now"));
		break;
	case EHitOutcome::Countered:
		GameMode->HitStop(0.12f);
		GameMode->Flash(FLinearColor(0.4f, 0.8f, 1.f, 0.25f), 0.25f);
		GameMode->ShowCallout(TEXT("COUNTERED"));
		GameMode->ShowFirstTimeHint(TEXT("Countered"), TEXT("Countered his guard break: he's open"));
		break;
	case EHitOutcome::GuardBroken:
		GameMode->HitStop(0.08f);
		if (bPlayerAttacks)
		{
			GameMode->ShowCallout(TEXT("GUARD BROKEN"));
			GameMode->ShowFirstTimeHint(TEXT("GuardBroken"), TEXT("His guard is broken: strike before he recovers"));
		}
		else
		{
			GameMode->ShowFirstTimeHint(TEXT("WasGuardBroken"), TEXT("Counter a guard break: press E / X as his grab comes in (it flashes blue)"));
		}
		break;
	case EHitOutcome::Blocked:
		if (bPlayerDefends)
		{
			GameMode->ShowFirstTimeHint(TEXT("Blocked"), TEXT("Blocked, at a cost in stamina. Tap Guard (F / LB) just as a blow lands to parry it"));
		}
		break;
	case EHitOutcome::Dodged:
		if (bPlayerDefends)
		{
			GameMode->ShowFirstTimeHint(TEXT("Dodged"), TEXT("Attack (LMB / RB) straight out of a dodge to lunge"));
		}
		break;
	case EHitOutcome::Hit:
		if (bPlayerAttacks && (Kind == EAttackKind::Heavy || Kind == EAttackKind::DodgeAttack))
		{
			GameMode->HitStop(0.06f);
		}
		break;
	default:
		break;
	}
	return Outcome;
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
