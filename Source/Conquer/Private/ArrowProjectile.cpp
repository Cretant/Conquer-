#include "ArrowProjectile.h"
#include "ConquerCharacterBase.h"
#include "CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AArrowProjectile::AArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

	// The basic shapes stand along Z; pitched over they lie along the arrow's +X.
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Root);
	Shaft->SetStaticMesh(Cylinder.Object);
	Shaft->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Shaft->SetRelativeScale3D(FVector(0.025f, 0.025f, 0.8f));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetStaticMesh(Cone.Object);
	Head->SetRelativeLocation(FVector(45.f, 0.f, 0.f));
	Head->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Head->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.12f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Burning, so it reads at night as it comes.
	Flame = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Root);
	Flame->SetRelativeLocation(FVector(40.f, 0.f, 0.f));
	Flame->SetIntensity(900.f);
	Flame->SetAttenuationRadius(380.f);
	Flame->SetLightColor(FLinearColor(1.f, 0.45f, 0.12f));
	Flame->SetCastShadows(false);
}

void AArrowProjectile::Launch(AConquerCharacterBase* InShooter, const FAttackSpec& InSpec, const FVector& InVelocity)
{
	Shooter = InShooter;
	Spec = InSpec;
	Velocity = InVelocity;
	Origin = GetActorLocation();
}

void AArrowProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	LifeSeconds -= DeltaSeconds;
	const FVector From = GetActorLocation();
	const FVector To = From + Velocity * DeltaSeconds;
	if (LifeSeconds <= 0.f || Velocity.IsNearlyZero())
	{
		Destroy();
		return;
	}

	// Walls and the ground.
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ArrowFlight), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		Destroy();
		return;
	}

	for (TActorIterator<AConquerCharacterBase> It(GetWorld()); It; ++It)
	{
		AConquerCharacterBase* Target = *It;
		if (!Target->IsPlayerTeam() || Target->IsDead() || DodgedBy.Contains(Target))
		{
			continue;
		}
		const FVector Centre = Target->GetActorLocation();
		const FVector Offset = Centre - FMath::ClosestPointOnSegment(Centre, From, To);
		const float HalfHeight = Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (Offset.Size2D() > Target->GetCapsuleRadius() + 20.f || FMath::Abs(Offset.Z) > HalfHeight + 10.f)
		{
			continue;
		}

		const EGuardDirection Side = UCombatComponent::SideFrom(Target, Origin);
		const EHitOutcome Outcome = Target->GetCombat()->ReceiveAttack(Shooter.Get(), Spec, EAttackKind::Shot, Side, 1.f, false);
		if (Outcome == EHitOutcome::Dodged)
		{
			DodgedBy.Add(Target);
			continue;
		}
		Destroy();
		return;
	}

	SetActorLocationAndRotation(To, Velocity.Rotation());
}
