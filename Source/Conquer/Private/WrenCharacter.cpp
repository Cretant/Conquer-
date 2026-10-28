#include "WrenCharacter.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "ConquerGameMode.h"
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
}

void AWrenCharacter::BeginPlay()
{
	Super::BeginPlay();
	Rage->OnRageStateChanged.AddDynamic(this, &AWrenCharacter::HandleRageChanged);
	UpdateMovementMode();

	SwordMesh = Weapon->GetStaticMesh();
	SwordLight = Combat->LightAttack;
	SwordHeavy = Combat->HeavyAttack;
	SwordGuardBreak = Combat->GuardBreakAttack;
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
			if (FMath::Abs(GuardAccumulator.X) > FMath::Abs(GuardAccumulator.Y))
			{
				Combat->SetGuardDirection(GuardAccumulator.X < 0.f ? EGuardDirection::Left : EGuardDirection::Right);
			}
			else if (GuardAccumulator.Y > 0.f)
			{
				Combat->SetGuardDirection(EGuardDirection::Top);
			}
			GuardAccumulator = FVector2D::ZeroVector;
		}
		return;
	}

	AddControllerYawInput(Look.X);
	AddControllerPitchInput(-Look.Y);
}

void AWrenCharacter::OnLight()      { Attack(EAttackKind::Light); }
void AWrenCharacter::OnGuardBreak() { Attack(EAttackKind::GuardBreak); }
void AWrenCharacter::OnFeint()      { Combat->Feint(); }
void AWrenCharacter::OnRage()       { Rage->TryActivate(); }

void AWrenCharacter::OnHeavy()
{
	if (!TryExecute())
	{
		Attack(EAttackKind::Heavy);
	}
}

void AWrenCharacter::OnDodge()
{
	FVector Dir = FVector::ZeroVector;
	if (Controller && !MoveInput.IsNearlyZero())
	{
		const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
		Dir = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y) * MoveInput.X;
	}
	Combat->Dodge(Dir);
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

bool AWrenCharacter::TryExecute()
{
	AConquerCharacterBase* Target = LockTarget ? LockTarget.Get() : FindBestTarget(ExecuteRange);
	if (!Target || Target->IsDead() || !Target->GetCombat()->IsStaggered()
		|| Target->GetAttributes()->GetHealthFraction() > ExecuteHealthFraction
		|| FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > ExecuteRange)
	{
		return false;
	}

	UE_LOG(LogConquer, Log, TEXT("Wren executes %s"), *Target->GetName());
	Target->Kill(this);
	Attributes->Heal(ExecuteHeal);
	Rage->AddRage(RageOnKill * 1.5f);
	return true;
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
	Combat->bGuardRaised = LockTarget != nullptr;
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
