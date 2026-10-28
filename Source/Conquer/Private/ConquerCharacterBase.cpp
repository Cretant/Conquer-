#include "ConquerCharacterBase.h"
#include "ConquerAnimInstance.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "WeaponTrailComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Materials/MaterialInterface.h"
#include "Conquer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInstanceDynamic.h"

AConquerCharacterBase::AConquerCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Attributes = CreateDefaultSubobject<UAttributesComponent>(TEXT("Attributes"));
	Rage = CreateDefaultSubobject<URageComponent>(TEXT("Rage"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(GetMesh());
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetGenerateOverlapEvents(false);
	Weapon->SetCanEverAffectNavigation(false);

	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(GetMesh());
	Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shield->SetGenerateOverlapEvents(false);
	Shield->SetCanEverAffectNavigation(false);

	Trail = CreateDefaultSubobject<UWeaponTrailComponent>(TEXT("Trail"));
	Trail->SetupAttachment(RootComponent);

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = 500.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GroundFriction = 8.f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void AConquerCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyPlaceholderBody();
	ApplyPlaceholderAnimations();
	Trail->Setup(Weapon, Combat, TrailMaterialPath.IsValid() ? Cast<UMaterialInterface>(TrailMaterialPath.TryLoad()) : nullptr);
	PhysicalAnimation->SetSkeletalMeshComponent(GetMesh());
	Attributes->OnDamaged.AddDynamic(this, &AConquerCharacterBase::HandleDamaged);
	Attributes->OnDeath.AddDynamic(this, &AConquerCharacterBase::HandleDeath);
}

namespace
{
	// Where the mannequins live in the Third Person feature pack across recent engine versions.
	// The path from DefaultGame.ini is tried first; these are fallbacks so a renamed pack still works.
	const TCHAR* FallbackBodyMeshes[] = {
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
	};

	const TCHAR* FallbackAnimClasses[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"),
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Quinn.ABP_Quinn_C"),
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"),
	};
}

void AConquerCharacterBase::ApplyPlaceholderBody()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	GetCapsuleComponent()->SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	MeshComp->SetRelativeLocationAndRotation(MeshLocation, MeshRotation);

	if (MeshComp->GetSkeletalMeshAsset() == nullptr)
	{
		USkeletalMesh* Body = BodyMeshPath.IsValid() ? Cast<USkeletalMesh>(BodyMeshPath.TryLoad()) : nullptr;
		for (const TCHAR* Path : FallbackBodyMeshes)
		{
			if (Body)
			{
				break;
			}
			Body = LoadObject<USkeletalMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		if (Body)
		{
			MeshComp->SetSkeletalMeshAsset(Body);
		}
		else
		{
			UE_LOG(LogConquer, Warning, TEXT("%s: no placeholder body found. Add the Third Person feature pack, or set BodyMeshPath in Config/DefaultGame.ini."), *GetName());
		}
	}

	if (MeshComp->GetAnimClass() == nullptr && MeshComp->GetSkeletalMeshAsset() != nullptr)
	{
		UClass* AnimClass = AnimClassPath.IsValid() ? AnimClassPath.TryLoadClass<UAnimInstance>() : nullptr;
		for (const TCHAR* Path : FallbackAnimClasses)
		{
			if (AnimClass)
			{
				break;
			}
			AnimClass = LoadClass<UAnimInstance>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		if (AnimClass)
		{
			MeshComp->SetAnimInstanceClass(AnimClass);
		}
	}

	if (!Weapon->GetStaticMesh() && WeaponMeshPath.IsValid())
	{
		Weapon->SetStaticMesh(Cast<UStaticMesh>(WeaponMeshPath.TryLoad()));
	}
	Weapon->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, WeaponSocket);
	Weapon->SetRelativeLocationAndRotation(WeaponLocation, WeaponRotation);

	if (!Shield->GetStaticMesh() && ShieldMeshPath.IsValid())
	{
		Shield->SetStaticMesh(Cast<UStaticMesh>(ShieldMeshPath.TryLoad()));
	}
	Shield->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, ShieldSocket);
	Shield->SetRelativeLocationAndRotation(ShieldLocation, ShieldRotation);
	Shield->SetRelativeScale3D(FVector(ShieldScale));

	if (!BodyTint.Equals(FLinearColor::White))
	{
		for (int32 Index = 0; Index < MeshComp->GetNumMaterials(); ++Index)
		{
			if (UMaterialInstanceDynamic* Material = MeshComp->CreateAndSetMaterialInstanceDynamic(Index))
			{
				Material->SetVectorParameterValue(TEXT("Tint"), BodyTint);
			}
		}
	}

	if (UConquerAnimInstance* Locomotion = Cast<UConquerAnimInstance>(MeshComp->GetAnimInstance()))
	{
		auto LoadClip = [this](const FSoftObjectPath& Path) { return Path.IsValid() ? LoadAnimation<UAnimSequence>(Path) : nullptr; };
		Locomotion->Idle = LoadClip(IdleAnimPath);
		Locomotion->Walk = LoadClip(WalkAnimPath);
		Locomotion->Jog = LoadClip(JogAnimPath);
		Locomotion->WalkBack = LoadClip(WalkBackAnimPath);
		Locomotion->WalkLeft = LoadClip(WalkLeftAnimPath);
		Locomotion->WalkRight = LoadClip(WalkRightAnimPath);
		Locomotion->JogBack = LoadClip(JogBackAnimPath);
		Locomotion->JogLeft = LoadClip(JogLeftAnimPath);
		Locomotion->JogRight = LoadClip(JogRightAnimPath);

		auto LoadAll = [this](const TArray<FSoftObjectPath>& Paths)
		{
			TArray<TObjectPtr<UAnimSequence>> Clips;
			for (const FSoftObjectPath& Path : Paths)
			{
				Clips.Add(LoadOptionalClip(Path));
			}
			return Clips;
		};
		Locomotion->WalkDiagonals = LoadAll(WalkDiagonalAnimPaths);
		Locomotion->JogDiagonals = LoadAll(JogDiagonalAnimPaths);
		Locomotion->WalkStart = LoadOptionalClip(WalkStartAnimPath);
		Locomotion->JogStart = LoadOptionalClip(JogStartAnimPath);
		Locomotion->WalkStops = LoadAll(WalkStopAnimPaths);
		Locomotion->JogStops = LoadAll(JogStopAnimPaths);
		Locomotion->Turns = LoadAll(TurnAnimPaths);
		Locomotion->IdleBreaks = LoadAll(IdleBreakAnimPaths);
		Locomotion->IdleBreaks.RemoveAll([](const TObjectPtr<UAnimSequence>& Clip) { return Clip == nullptr; });

		const bool bFullSet = Locomotion->WalkDiagonals.Num() == 4 && !Locomotion->WalkDiagonals.Contains(nullptr);
		if (bFullSet && LocomotionWalkSpeed > 0.f && LocomotionJogSpeed > 0.f)
		{
			Locomotion->WalkSpeed = LocomotionWalkSpeed;
			Locomotion->JogSpeed = LocomotionJogSpeed;
		}
	}
}

void AConquerCharacterBase::ApplyPlaceholderAnimations()
{
	auto Load = [this](const FSoftObjectPath& Path) { return LoadAnimation<UAnimMontage>(Path); };
	auto Fill = [&Load](TObjectPtr<UAnimMontage>& Montage, const FSoftObjectPath& Path)
	{
		if (!Montage && Path.IsValid())
		{
			Montage = Load(Path);
		}
	};

	Fill(Combat->LightAttack.MontageLeft, LightLeftMontagePath);
	Fill(Combat->LightAttack.MontageTop, LightTopMontagePath);
	Fill(Combat->LightAttack.MontageRight, LightRightMontagePath);
	Fill(Combat->HeavyAttack.MontageLeft, HeavyMontagePath);
	Fill(Combat->HeavyAttack.MontageTop, HeavyMontagePath);
	Fill(Combat->HeavyAttack.MontageRight, HeavyMontagePath);
	Fill(Combat->GuardBreakAttack.MontageLeft, GuardBreakMontagePath);
	Fill(Combat->GuardBreakAttack.MontageTop, GuardBreakMontagePath);
	Fill(Combat->GuardBreakAttack.MontageRight, GuardBreakMontagePath);
	Fill(Combat->DodgeMontage, DodgeMontagePath);
	Fill(Combat->HitReactMontage, HitReactMontagePath);
	Fill(Combat->HeavyHitReactMontage, HeavyHitReactMontagePath);
	Fill(Combat->GuardBrokenMontage, GuardBrokenMontagePath);
	Fill(Combat->BlockMontage, BlockMontagePath);
	Fill(Combat->ThrustAttack.MontageLeft, ThrustMontagePath);
	Fill(Combat->ThrustAttack.MontageTop, ThrustMontagePath);
	Fill(Combat->ThrustAttack.MontageRight, ThrustMontagePath);
	Fill(Combat->DodgeAttack.MontageLeft, DodgeAttackMontagePath);
	Fill(Combat->DodgeAttack.MontageTop, DodgeAttackMontagePath);
	Fill(Combat->DodgeAttack.MontageRight, DodgeAttackMontagePath);
	Fill(Combat->ShotAttack.MontageLeft, ShotMontagePath);
	Fill(Combat->ShotAttack.MontageTop, ShotMontagePath);
	Fill(Combat->ShotAttack.MontageRight, ShotMontagePath);
	Fill(Combat->ParryMontage, ParryMontagePath);
	Fill(Combat->ParriedRecoilMontage, ParriedRecoilMontagePath);
	auto FillAll = [&Load](TArray<TObjectPtr<UAnimMontage>>& Montages, const TArray<FSoftObjectPath>& Paths)
	{
		if (Montages.Num() == 0)
		{
			for (const FSoftObjectPath& Path : Paths)
			{
				if (UAnimMontage* Montage = Load(Path))
				{
					Montages.Add(Montage);
				}
			}
		}
	};
	FillAll(Combat->LightCombo, LightComboMontagePaths);
	FillAll(Combat->HeavyCombo, HeavyComboMontagePaths);
	auto LoadIf = [&Load](const FSoftObjectPath& Path) { return Path.IsValid() ? Load(Path) : nullptr; };
	ShotReleaseMontage = LoadIf(ShotReleaseMontagePath);
	ThrustLightMontage = LoadIf(ThrustLightMontagePath);
	ThrustHeavyMontage = LoadIf(ThrustHeavyMontagePath);
	GetUpMontage = LoadIf(GetUpMontagePath);
	if (LungeRange > 0.f)
	{
		Combat->LungeRange = LungeRange;
	}
	// Without a dodge attack of its own, a quick overhead light.
	Fill(Combat->DodgeAttack.MontageLeft, LightTopMontagePath);
	Fill(Combat->DodgeAttack.MontageTop, LightTopMontagePath);
	Fill(Combat->DodgeAttack.MontageRight, LightTopMontagePath);

	if (DeathMontages.Num() == 0)
	{
		for (const FSoftObjectPath& Path : DeathMontagePaths)
		{
			if (UAnimMontage* Montage = Load(Path))
			{
				DeathMontages.Add(Montage);
			}
		}
	}

	// Movement is driven by code (dodges launch the capsule), so animations play in place.
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
}

UObject* AConquerCharacterBase::LoadAnimationObject(const FSoftObjectPath& Path) const
{
	// A local-only version of the clip (e.g. Mixamo's, which can't go in the public repo) wins when present.
	for (const FString& Override : LocalAnimOverrides)
	{
		FString From, To;
		if (Override.Split(TEXT("=>"), &From, &To) && Path.ToString().Contains(From))
		{
			const FSoftObjectPath Local(Path.ToString().Replace(*From, *To));
			if (FPackageName::DoesPackageExist(Local.GetLongPackageName()))
			{
				if (UObject* Asset = Local.TryLoad())
				{
					return Asset;
				}
			}
		}
	}

	UObject* Asset = Path.TryLoad();
	if (!Asset)
	{
		UE_LOG(LogConquer, Warning, TEXT("%s: animation %s not found. Run Tools/build_anims.py."), *GetName(), *Path.ToString());
	}
	return Asset;
}

UAnimSequence* AConquerCharacterBase::LoadOptionalClip(const FSoftObjectPath& Path) const
{
	if (!Path.IsValid() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		return nullptr;
	}
	return Cast<UAnimSequence>(Path.TryLoad());
}

void AConquerCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePhysicalHit(DeltaSeconds);
}

float AConquerCharacterBase::PlayMontage(UAnimMontage* Montage, float Rate)
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	return Montage && Anim ? Anim->Montage_Play(Montage, Rate) : 0.f;
}

void AConquerCharacterBase::PlayPhysicalHit(const FVector& Direction, float Strength)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bPhysicalHitReactions || IsDead() || !MeshComp->GetPhysicsAsset() || MeshComp->GetBoneIndex(PhysicalHitBone) == INDEX_NONE)
	{
		return;
	}
	// The bodies chase the animation with springs, so the blow knocks them off it and they swing back.
	FPhysicalAnimationData Springs;
	Springs.bIsLocalSimulation = false;
	Springs.OrientationStrength = 1500.f;
	Springs.AngularVelocityStrength = 150.f;
	Springs.PositionStrength = 1500.f;
	Springs.VelocityStrength = 150.f;
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(PhysicalHitBone, Springs, true);
	MeshComp->SetAllBodiesBelowSimulatePhysics(PhysicalHitBone, true, true);
	PhysicalHitWeight = 0.75f;
	MeshComp->SetAllBodiesBelowPhysicsBlendWeight(PhysicalHitBone, PhysicalHitWeight, false, true);
	MeshComp->AddImpulseToAllBodiesBelow(Direction.GetSafeNormal() * Strength, PhysicalHitBone, true, true);
}

void AConquerCharacterBase::UpdatePhysicalHit(float DeltaSeconds)
{
	if (PhysicalHitWeight <= 0.f || IsDead())
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	PhysicalHitWeight -= DeltaSeconds * 2.f;
	if (PhysicalHitWeight <= 0.f)
	{
		PhysicalHitWeight = 0.f;
		MeshComp->SetAllBodiesBelowSimulatePhysics(PhysicalHitBone, false, true);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}
	MeshComp->SetAllBodiesBelowPhysicsBlendWeight(PhysicalHitBone, PhysicalHitWeight, false, true);
}

bool AConquerCharacterBase::IsDead() const
{
	return Attributes && Attributes->IsDead();
}

float AConquerCharacterBase::GetCapsuleRadius() const
{
	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

void AConquerCharacterBase::Kill(AActor* Killer)
{
	if (!IsDead())
	{
		Attributes->MinHealth = 0.f;
		Attributes->ApplyDamage(Attributes->Health, Killer);
	}
}

void AConquerCharacterBase::HandleDamaged(float Amount, AActor* DamageCauser)
{
	Rage->AddRage(Amount * 0.6f);
	Rage->NotifyHealthChanged(Attributes->GetHealthFraction());
}

void AConquerCharacterBase::HandleDeath(AActor* Killer)
{
	UE_LOG(LogConquer, Log, TEXT("%s was killed by %s"), *GetName(), Killer ? *Killer->GetName() : TEXT("nothing"));

	Combat->Stagger(9999.f);
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp->GetAnimInstance();
	if (!DeathImpulse.IsNearlyZero() && MeshComp->GetPhysicsAsset())
	{
		// A big blow: limp, and thrown the way it was struck.
		PhysicalHitWeight = 0.f;
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetAllBodiesPhysicsBlendWeight(1.f);
		MeshComp->WakeAllRigidBodies();
		MeshComp->AddImpulseToAllBodiesBelow(DeathImpulse, NAME_None, true, true);
	}
	else if (Anim && DeathMontages.Num() > 0)
	{
		// Death montages don't blend out, so the body holds the last frame on the ground.
		Anim->Montage_Play(DeathMontages[FMath::RandRange(0, DeathMontages.Num() - 1)]);
	}
	else if (MeshComp->GetPhysicsAsset())
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
	}

	if (!IsPlayerTeam())
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}
