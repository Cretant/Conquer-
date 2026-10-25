#include "ConquerCharacterBase.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "Conquer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

AConquerCharacterBase::AConquerCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Attributes = CreateDefaultSubobject<UAttributesComponent>(TEXT("Attributes"));
	Rage = CreateDefaultSubobject<URageComponent>(TEXT("Rage"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

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
	Attributes->OnDamaged.AddDynamic(this, &AConquerCharacterBase::HandleDamaged);
	Attributes->OnDeath.AddDynamic(this, &AConquerCharacterBase::HandleDeath);
}

void AConquerCharacterBase::ApplyPlaceholderBody()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp->GetSkeletalMeshAsset() == nullptr && BodyMeshPath.IsValid())
	{
		if (USkeletalMesh* Body = Cast<USkeletalMesh>(BodyMeshPath.TryLoad()))
		{
			MeshComp->SetSkeletalMeshAsset(Body);
		}
		else
		{
			UE_LOG(LogConquer, Warning, TEXT("%s: could not load body mesh %s. Add the Third Person feature pack or fix the path in DefaultGame.ini."), *GetName(), *BodyMeshPath.ToString());
		}
	}

	if (MeshComp->GetAnimClass() == nullptr && AnimClassPath.IsValid())
	{
		if (UClass* AnimClass = AnimClassPath.TryLoadClass<UAnimInstance>())
		{
			MeshComp->SetAnimInstanceClass(AnimClass);
		}
	}
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
	if (MeshComp->GetPhysicsAsset())
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
	}

	if (!IsPlayerTeam())
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}
