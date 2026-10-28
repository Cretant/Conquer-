#include "WrenCharacter.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "ConquerGameMode.h"
#include "ArrowProjectile.h"
#include "GarrickBoss.h"
#include "ConquerHitNotify.h"
#include "Camera/CameraActor.h"
#include "Conquer.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AWrenCharacter::AWrenCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 340.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Rage->bEnabled = true;
	Attributes->MaxHealth = 150.f;
	Combat->bGuardRaised = false;
	Combat->bCanPerfectParry = true;
}

void AWrenCharacter::BeginPlay()
{
	Super::BeginPlay();
	Rage->OnRageStateChanged.AddDynamic(this, &AWrenCharacter::HandleRageChanged);
	UpdateMovementMode();

	SwordMesh = Weapon->GetStaticMesh();
	BaseFieldOfView = FollowCamera->FieldOfView;
	SwordLight = Combat->LightAttack;
	SwordHeavy = Combat->HeavyAttack;
	SwordGuardBreak = Combat->GuardBreakAttack;
	SwordLightCombo = Combat->LightCombo;
	SwordHeavyCombo = Combat->HeavyCombo;
	UpdateGuard();
}

void AWrenCharacter::EquipPitchfork(bool bPitchfork)
{
	UStaticMesh* Pitchfork = PitchforkMeshPath.IsValid() ? Cast<UStaticMesh>(PitchforkMeshPath.TryLoad()) : nullptr;
	if (bPitchfork && !Pitchfork)
	{
		UE_LOG(LogConquer, Warning, TEXT("Pitchfork mesh %s not found. Run Tools/build_marrowford.py."), *PitchforkMeshPath.ToString());
	}
	bPitchforkEquipped = bPitchfork;
	Weapon->SetStaticMesh(bPitchfork && Pitchfork ? Pitchfork : SwordMesh.Get());
	Weapon->SetRelativeLocationAndRotation(bPitchfork ? PitchforkLocation : WeaponLocation, bPitchfork ? PitchforkRotation : WeaponRotation);

	// The pitchfork jabs: thrusts in place of the sword's swings, and no sword combos.
	auto SetMontages = [](FAttackSpec& Spec, const FAttackSpec& Sword, UAnimMontage* Thrust)
	{
		Spec.MontageLeft = Thrust ? Thrust : Sword.MontageLeft.Get();
		Spec.MontageTop = Thrust ? Thrust : Sword.MontageTop.Get();
		Spec.MontageRight = Thrust ? Thrust : Sword.MontageRight.Get();
	};
	SetMontages(Combat->LightAttack, SwordLight, bPitchfork ? ThrustLightMontage.Get() : nullptr);
	SetMontages(Combat->HeavyAttack, SwordHeavy, bPitchfork ? ThrustHeavyMontage.Get() : nullptr);
	Combat->LightCombo = bPitchfork && ThrustLightMontage ? TArray<TObjectPtr<UAnimMontage>>() : SwordLightCombo;
	Combat->HeavyCombo = bPitchfork && ThrustHeavyMontage ? TArray<TObjectPtr<UAnimMontage>>() : SwordHeavyCombo;
	ApplyWeaponStats();
}

void AWrenCharacter::SetDamageBonus(float Multiplier)
{
	DamageBonus = Multiplier;
	ApplyWeaponStats();
}

void AWrenCharacter::ApplyWeaponStats()
{
	// Montages stay as they are; only damage and reach change with the weapon.
	auto Apply = [this](FAttackSpec& Spec, const FAttackSpec& Sword)
	{
		Spec.Damage = Sword.Damage * DamageBonus * (bPitchforkEquipped ? PitchforkDamageScale : 1.f);
		Spec.Range = Sword.Range * (bPitchforkEquipped ? PitchforkRangeScale : 1.f);
	};
	Apply(Combat->LightAttack, SwordLight);
	Apply(Combat->HeavyAttack, SwordHeavy);
	Apply(Combat->GuardBreakAttack, SwordGuardBreak);
}

void AWrenCharacter::ShowWeapon(bool bShow)
{
	Weapon->SetVisibility(bShow);
}

void AWrenCharacter::SetKnockedDown(bool bDown)
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	UAnimMontage* Fall = DeathMontages.Num() > 0 ? DeathMontages[0].Get() : nullptr;
	if (!Anim || !Fall)
	{
		return;
	}
	if (bDown)
	{
		// Straight to the end of the fall: the cut hides the blow itself.
		Anim->Montage_Play(Fall);
		Anim->Montage_SetPosition(Fall, Fall->GetPlayLength() - 0.05f);
	}
	else if (GetUpMontage)
	{
		Anim->Montage_Stop(0.f, Fall);
		Anim->Montage_Play(GetUpMontage);
	}
	else
	{
		Anim->Montage_Stop(0.8f, Fall);
	}
}

void AWrenCharacter::UnlockRage()
{
	if (Rage->bEnabled)
	{
		return;
	}
	Rage->bEnabled = true;
	Rage->AddRage(Rage->MaxRage);
	Rage->TryActivate();
}

// --- Input ---------------------------------------------------------------

void AWrenCharacter::BuildInput()
{
	if (MappingContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction       = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction       = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	LightAction      = MakeAction(TEXT("IA_Light"), EInputActionValueType::Boolean);
	HeavyAction      = MakeAction(TEXT("IA_Heavy"), EInputActionValueType::Boolean);
	GuardBreakAction = MakeAction(TEXT("IA_GuardBreak"), EInputActionValueType::Boolean);
	FeintAction      = MakeAction(TEXT("IA_Feint"), EInputActionValueType::Boolean);
	DodgeAction      = MakeAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	LockAction       = MakeAction(TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	RageAction       = MakeAction(TEXT("IA_Rage"), EInputActionValueType::Boolean);
	GuardAction      = MakeAction(TEXT("IA_Guard"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Wren"));

	// WASD into a 2D axis: W/S feed Y (swizzled), A/D feed X, negated for S and A.
	MappingContext->MapKey(MoveAction, EKeys::D);
	MappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(this));
	MappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	{
		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	MappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);

	// Controller layout follows For Honor: RB light, RT heavy, X guard break, B feint, A dodge, LT lock.
	MappingContext->MapKey(LightAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(LightAction, EKeys::Gamepad_RightShoulder);
	MappingContext->MapKey(HeavyAction, EKeys::RightMouseButton);
	MappingContext->MapKey(HeavyAction, EKeys::Gamepad_RightTrigger);
	MappingContext->MapKey(GuardBreakAction, EKeys::E);
	MappingContext->MapKey(GuardBreakAction, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(FeintAction, EKeys::Q);
	MappingContext->MapKey(FeintAction, EKeys::Gamepad_FaceButton_Right);
	MappingContext->MapKey(DodgeAction, EKeys::SpaceBar);
	MappingContext->MapKey(DodgeAction, EKeys::Gamepad_FaceButton_Bottom);
	MappingContext->MapKey(LockAction, EKeys::Tab);
	MappingContext->MapKey(LockAction, EKeys::MiddleMouseButton);
	MappingContext->MapKey(LockAction, EKeys::Gamepad_LeftTrigger);
	MappingContext->MapKey(RageAction, EKeys::R);
	MappingContext->MapKey(RageAction, EKeys::Gamepad_FaceButton_Top);
	MappingContext->MapKey(GuardAction, EKeys::F);
	MappingContext->MapKey(GuardAction, EKeys::Gamepad_LeftShoulder);
}

void AWrenCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogConquer, Error, TEXT("Enhanced Input is not the default input component. Check Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWrenCharacter::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AWrenCharacter::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWrenCharacter::OnLook);
	Input->BindAction(LightAction, ETriggerEvent::Started, this, &AWrenCharacter::OnLight);
	Input->BindAction(HeavyAction, ETriggerEvent::Started, this, &AWrenCharacter::OnHeavy);
	Input->BindAction(GuardBreakAction, ETriggerEvent::Started, this, &AWrenCharacter::OnGuardBreak);
	Input->BindAction(FeintAction, ETriggerEvent::Started, this, &AWrenCharacter::OnFeint);
	Input->BindAction(DodgeAction, ETriggerEvent::Started, this, &AWrenCharacter::OnDodge);
	Input->BindAction(LockAction, ETriggerEvent::Started, this, &AWrenCharacter::OnToggleLock);
	Input->BindAction(RageAction, ETriggerEvent::Started, this, &AWrenCharacter::OnRage);
	Input->BindAction(GuardAction, ETriggerEvent::Started, this, &AWrenCharacter::OnGuardPressed);
	Input->BindAction(GuardAction, ETriggerEvent::Completed, this, &AWrenCharacter::OnGuardReleased);
}

void AWrenCharacter::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
	if (!Controller || IsDead() || MoveInput.IsNearlyZero())
	{
		return;
	}

	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const float Scale = Combat->IsBusy() ? 0.35f : 1.f;
	AddMovementInput(Forward, MoveInput.Y * Scale);
	AddMovementInput(Right, MoveInput.X * Scale);
}

void AWrenCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();

	if (LockTarget)
	{
		// Locked on, the look input becomes the guard stance instead of the camera.
		GuardAccumulator += Look;
		if (GuardAccumulator.Size() >= GuardSwitchThreshold)
		{
			const EGuardDirection Before = Combat->GetGuardDirection();
			if (FMath::Abs(GuardAccumulator.X) > FMath::Abs(GuardAccumulator.Y))
			{
				Combat->SetGuardDirection(GuardAccumulator.X < 0.f ? EGuardDirection::Left : EGuardDirection::Right);
			}
			else if (GuardAccumulator.Y > 0.f)
			{
				Combat->SetGuardDirection(EGuardDirection::Top);
			}
			// Switching to a side is a guard input, so a well-timed flick parries.
			if (Combat->GetGuardDirection() != Before)
			{
				Combat->NotifyGuardInput();
			}
			GuardAccumulator = FVector2D::ZeroVector;
		}
		return;
	}

	AddControllerYawInput(Look.X);
	AddControllerPitchInput(-Look.Y);
}

void AWrenCharacter::OnLight()      { if (!IsExecuting()) { Attack(Combat->CanDodgeAttack() ? EAttackKind::DodgeAttack : EAttackKind::Light); } }
void AWrenCharacter::OnGuardBreak() { Attack(EAttackKind::GuardBreak); }
void AWrenCharacter::OnFeint()      { Combat->Feint(); }
void AWrenCharacter::OnRage()       { Rage->TryActivate(); }

void AWrenCharacter::OnHeavy()
{
	if (IsExecuting())
	{
		return;
	}
	if (!TryExecute())
	{
		Attack(EAttackKind::Heavy);
	}
}

void AWrenCharacter::OnDodge()
{
	TryDodge();
}

bool AWrenCharacter::TryDodge()
{
	if (IsExecuting())
	{
		return false;
	}
	FVector Dir = FVector::ZeroVector;
	if (Controller && !MoveInput.IsNearlyZero())
	{
		const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
		Dir = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y) * MoveInput.X;
	}
	const bool bWasAttacking = Combat->GetPhase() != EAttackPhase::None;
	if (!Combat->Dodge(Dir))
	{
		return false;
	}
	if (bWasAttacking)
	{
		if (AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>())
		{
			GameMode->ShowFirstTimeHint(TEXT("DodgeCancel"), TEXT("Dodge cuts a light attack's follow-through short, and pulls her out of a heavy before it commits"));
		}
	}
	return true;
}

void AWrenCharacter::SetGuardHeld(bool bHeld)
{
	bGuardHeld = bHeld;
	if (bHeld)
	{
		if (!LockTarget)
		{
			Combat->SetGuardDirection(SideOfNearestThreat());
		}
		Combat->NotifyGuardInput();
	}
	UpdateGuard();
}

void AWrenCharacter::UpdateGuard()
{
	Combat->bGuardRaised = LockTarget != nullptr || bGuardHeld;
	// Locked on, her attacks come from her guard side; free, they flow as combos.
	Combat->bUseCombos = LockTarget == nullptr;
}

EGuardDirection AWrenCharacter::SideOfNearestThreat() const
{
	EGuardDirection Side = Combat->GetGuardDirection();
	float Nearest = BIG_NUMBER;

	for (TActorIterator<AConquerCharacterBase> It(GetWorld()); It; ++It)
	{
		const AConquerCharacterBase* Enemy = *It;
		const EAttackPhase Phase = Enemy->GetCombat()->GetPhase();
		if (Enemy->IsPlayerTeam() || Enemy->IsDead() || (Phase != EAttackPhase::Windup && Phase != EAttackPhase::Active)
			|| Enemy->GetCombat()->GetAttackKind() == EAttackKind::Shot)
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Enemy->GetActorLocation(), GetActorLocation());
		if (Dist < 600.f && Dist < Nearest)
		{
			Nearest = Dist;
			Side = Enemy->GetCombat()->GetAttackDirection();
		}
	}
	for (TActorIterator<AArrowProjectile> It(GetWorld()); It; ++It)
	{
		const FVector ToMe = GetActorLocation() - It->GetActorLocation();
		const float Dist = ToMe.Size2D();
		if (Dist < 1500.f && FVector::DotProduct(It->GetVelocity().GetSafeNormal2D(), ToMe.GetSafeNormal2D()) > 0.8f && Dist * 0.5f < Nearest)
		{
			Nearest = Dist * 0.5f;
			Side = UCombatComponent::SideFrom(this, It->GetActorLocation());
		}
	}
	return Side;
}

void AWrenCharacter::OnToggleLock()
{
	SetLockTarget(LockTarget ? nullptr : FindBestTarget(LockOnRange));
}

// --- Combat --------------------------------------------------------------

void AWrenCharacter::Attack(EAttackKind Kind)
{
	if (IsDead())
	{
		return;
	}

	// Out of lock-on she swings at whoever is closest in front of her, hack-and-slash style.
	if (!LockTarget)
	{
		if (AConquerCharacterBase* Target = FindBestTarget(400.f))
		{
			const FVector To = Target->GetActorLocation() - GetActorLocation();
			SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}

	// Free-flowing attacks alternate sides; locked-on attacks come from her guard side.
	EGuardDirection Direction = Combat->GetGuardDirection();
	if (!LockTarget)
	{
		Direction = (FreeSwingCount++ % 2 == 0) ? EGuardDirection::Right : EGuardDirection::Left;
		if (Kind == EAttackKind::Heavy)
		{
			Direction = EGuardDirection::Top;
		}
	}
	Combat->StartAttack(Kind, Direction);
}

bool AWrenCharacter::CanExecute(const AConquerCharacterBase* Target) const
{
	return Target && !Target->IsDead() && Target->GetCombat()->IsStaggered()
		&& Target->GetAttributes()->GetHealthFraction() <= ExecuteHealthFraction
		&& FVector::Dist(Target->GetActorLocation(), GetActorLocation()) <= ExecuteRange;
}

bool AWrenCharacter::TryExecute()
{
	AConquerCharacterBase* Target = LockTarget ? LockTarget.Get() : FindBestTarget(ExecuteRange);
	if (IsExecuting() || IsDead() || !CanExecute(Target))
	{
		return false;
	}
	StartExecution(Target);
	return true;
}

void AWrenCharacter::StartExecution(AConquerCharacterBase* Target)
{
	ExecutionTarget = Target;
	ExecutionTime = 0.f;
	ExecutionBlowsDone = 0;
	ExecutionBlows.Reset();
	Combat->Stagger(0.f);
	Target->GetCombat()->Stagger(9999.f);

	// Square up: the victim set in front of her, facing her.
	const FVector Forward = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(Forward.Rotation());
	const FVector Spot = GetActorLocation() + Forward * 135.f;
	Target->SetActorLocation(FVector(Spot.X, Spot.Y, Target->GetActorLocation().Z), true);
	Target->SetActorRotation((-Forward).Rotation());

	// Which finisher: Sir Garrick kneels for an overhead blow; anyone else gets one of three.
	const bool bBoss = Target->IsA<AGarrickBoss>();
	ExecutionVariant = bBoss ? 3 : FMath::RandRange(0, 2);
	UAnimMontage* Montage = nullptr;
	float Rate = 1.f;
	switch (ExecutionVariant)
	{
	case 0:
		// A flurry: two cuts that rock him, then the last.
		Montage = Combat->HeavyCombo.Num() > 0 ? Combat->HeavyCombo.Last().Get() : Combat->HeavyAttack.MontageTop.Get();
		Rate = 1.1f;
		break;
	case 1:
		// A kick to the gut, then a heavy cut.
		Montage = Combat->GuardBreakAttack.MontageTop;
		Rate = 1.2f;
		break;
	case 2:
		// A lunging thrust straight through.
		Montage = Combat->DodgeAttack.MontageTop;
		Rate = 0.9f;
		break;
	default:
		Montage = Combat->HeavyAttack.MontageTop;
		Rate = 0.8f;
		break;
	}
	const float Hit = UConquerHitNotify::FindHitTime(Montage);
	PlayMontage(Montage, Rate);
	ExecutionKillAt = (Hit > 0.f ? Hit : 0.6f) / Rate;
	if (ExecutionVariant == 0)
	{
		ExecutionBlows = { ExecutionKillAt * 0.45f, ExecutionKillAt * 0.75f };
	}
	else if (ExecutionVariant == 1)
	{
		// The kick lands, then the heavy follows.
		ExecutionBlows = { ExecutionKillAt };
		ExecutionKillAt += 0.75f;
	}
	ExecutionEndAt = ExecutionKillAt + (bBoss ? 1.4f : 0.9f);

	// A low camera from her sword side, both of them in frame.
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		const FVector Mid = (GetActorLocation() + Target->GetActorLocation()) * 0.5f;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
		const FVector Eye = Mid + Side * 300.f - Forward * 60.f + FVector(0.f, 0.f, bBoss ? 10.f : 30.f);
		ExecutionCamera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Mid + FVector(0.f, 0.f, 20.f) - Eye).Rotation());
		PC->SetViewTargetWithBlend(ExecutionCamera, 0.25f, VTBlend_EaseInOut, 2.f);
	}
	if (AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>())
	{
		GameMode->bLetterbox = true;
	}
	UE_LOG(LogConquer, Log, TEXT("Wren executes %s (finisher %d)"), *Target->GetName(), ExecutionVariant);
}

void AWrenCharacter::UpdateExecution(float DeltaSeconds)
{
	AConquerCharacterBase* Target = ExecutionTarget;
	if (!Target)
	{
		return;
	}
	ExecutionTime += DeltaSeconds;
	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	// The blows before the last rock him: a flinch, sparks of contact, the camera kicks.
	while (ExecutionBlowsDone < ExecutionBlows.Num() && ExecutionTime >= ExecutionBlows[ExecutionBlowsDone])
	{
		++ExecutionBlowsDone;
		Target->GetCombat()->PlayReaction(ExecutionVariant == 1 ? Target->GetCombat()->GuardBrokenMontage.Get() : Target->GetCombat()->HitReactMontage.Get());
		Target->PlayPhysicalHit(Forward, 300.f);
		if (ExecutionVariant == 1)
		{
			// After the kick, the heavy cut.
			PlayMontage(Combat->HeavyCombo.Num() > 0 ? Combat->HeavyCombo[0].Get() : Combat->HeavyAttack.MontageTop.Get(), 1.3f);
		}
		OnCombatImpact(0.5f);
		if (GameMode)
		{
			GameMode->HitStop(0.06f, this, Target);
		}
	}

	if (!Target->IsDead() && ExecutionTime >= ExecutionKillAt)
	{
		// The last blow: he's thrown back, the world stops a moment, she's healed and enraged.
		Target->DeathImpulse = Forward * (ExecutionVariant == 3 ? 300.f : 850.f) + FVector(0.f, 0.f, 300.f);
		Target->Kill(this);
		Attributes->Heal(ExecuteHeal);
		Rage->AddRage(RageOnKill * 1.5f);
		OnCombatImpact(1.f);
		if (GameMode)
		{
			GameMode->HitStop(0.18f, this, Target);
			GameMode->Flash(FLinearColor(0.6f, 0.05f, 0.02f, 0.25f), 0.35f);
			GameMode->ShowCallout(TEXT("EXECUTED"));
		}
	}

	if (ExecutionTime >= ExecutionEndAt)
	{
		FinishExecution();
	}
}

void AWrenCharacter::FinishExecution()
{
	ExecutionTarget = nullptr;
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->SetViewTargetWithBlend(this, 0.35f, VTBlend_EaseInOut, 2.f);
	}
	if (ExecutionCamera)
	{
		ExecutionCamera->SetLifeSpan(0.5f);
		ExecutionCamera = nullptr;
	}
	if (AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>())
	{
		GameMode->bLetterbox = false;
	}
}

void AWrenCharacter::OnCombatImpact(float Strength)
{
	CameraKick = FMath::Max(CameraKick, FMath::Clamp(Strength, 0.f, 1.f));
}

void AWrenCharacter::OnLandedAttack(AConquerCharacterBase* Target, EHitOutcome Outcome, float DamageDealt)
{
	if (Outcome == EHitOutcome::Hit)
	{
		Rage->AddRage(RageOnHit);
		if (Target->IsDead())
		{
			Rage->AddRage(RageOnKill);
		}
	}
}

AConquerCharacterBase* AWrenCharacter::FindBestTarget(float MaxRange) const
{
	// Prefer enemies near the centre of the camera, then the nearest.
	const FVector ViewDir = Controller ? Controller->GetControlRotation().Vector().GetSafeNormal2D() : GetActorForwardVector();
	AConquerCharacterBase* Best = nullptr;
	float BestScore = -BIG_NUMBER;

	for (TActorIterator<AConquerCharacterBase> It(GetWorld()); It; ++It)
	{
		AConquerCharacterBase* Other = *It;
		if (Other == this || Other->IsDead() || Other->IsPlayerTeam())
		{
			continue;
		}
		const FVector To = Other->GetActorLocation() - GetActorLocation();
		const float Dist = To.Size2D();
		if (Dist > MaxRange)
		{
			continue;
		}
		const float Score = FVector::DotProduct(ViewDir, To.GetSafeNormal2D()) * 2.f - Dist / MaxRange;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Other;
		}
	}
	return Best;
}

void AWrenCharacter::SetLockTarget(AConquerCharacterBase* NewTarget)
{
	LockTarget = NewTarget;
	UpdateGuard();
	GuardAccumulator = FVector2D::ZeroVector;
	UpdateMovementMode();
}

void AWrenCharacter::UpdateMovementMode()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float RageScale = Rage->IsRageActive() ? Rage->SpeedMultiplier : 1.f;
	const bool bLocked = LockTarget != nullptr;

	Move->bOrientRotationToMovement = !bLocked;
	bUseControllerRotationYaw = bLocked;
	Move->MaxWalkSpeed = (bLocked ? LockedWalkSpeed : FreeWalkSpeed) * RageScale;
}

void AWrenCharacter::HandleRageChanged(bool bActive)
{
	Combat->bHyperArmor = bActive;
	CustomTimeDilation = bActive ? 1.15f : 1.f;
	UpdateMovementMode();
}

void AWrenCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	GuardAccumulator *= 0.85f;
	UpdateExecution(DeltaSeconds);

	// The camera kick: a quick shake of the camera and a punch out of the field of view, fading over half a second.
	if (CameraKick > 0.f)
	{
		CameraKick = FMath::Max(0.f, CameraKick - DeltaSeconds * 2.2f);
		const float Time = GetWorld()->GetTimeSeconds() * 40.f;
		const float Shake = CameraKick * CameraKick * 9.f;
		FollowCamera->SetRelativeLocation(FVector(0.f, FMath::Sin(Time * 1.3f) * Shake, FMath::Cos(Time) * Shake));
		FollowCamera->SetFieldOfView(BaseFieldOfView + CameraKick * 7.f);
	}
	else if (!FollowCamera->GetRelativeLocation().IsZero())
	{
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
		FollowCamera->SetFieldOfView(BaseFieldOfView);
	}

	// Holding guard out of lock-on, she keeps turning it to meet the next blow.
	if (bGuardHeld && !LockTarget)
	{
		Combat->SetGuardDirection(SideOfNearestThreat());
	}

	if (LockTarget && (LockTarget->IsDead() || FVector::Dist(LockTarget->GetActorLocation(), GetActorLocation()) > LockOnRange * 1.2f))
	{
		// Hop to the next enemy so a fight against a group keeps flowing.
		SetLockTarget(FindBestTarget(LockOnRange));
	}

	if (LockTarget && Controller)
	{
		const FVector To = LockTarget->GetActorLocation() - GetActorLocation();
		FRotator Desired = To.Rotation();
		Desired.Pitch = -12.f;
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaSeconds, 10.f));
	}
}

void AWrenCharacter::HandleDeath(AActor* Killer)
{
	SetLockTarget(nullptr);
	Super::HandleDeath(Killer);

	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	if (GameMode && GameMode->HandlePlayerDeath(this))
	{
		return;
	}

	// Levels without checkpoints restart after a moment.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
	}), 3.f, false);
}
