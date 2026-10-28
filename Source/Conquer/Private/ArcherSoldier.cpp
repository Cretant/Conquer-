#include "ArcherSoldier.h"
#include "ArrowProjectile.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "ConquerGameMode.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AArcherSoldier::AArcherSoldier()
{
	AIControllerClass = AArcherAIController::StaticClass();
	ArrowClass = AArrowProjectile::StaticClass();

	DisplayName = TEXT("Archer");
	FirstSightHint = TEXT("Archers loose burning arrows: tap Guard (F / LB) as one arrives to deflect it, or dodge");

	Attributes->MaxHealth = 30.f;
	Combat->LightAttack.Damage = 7.f;
	EngageRange = 1400.f;
	BlockChance = 0.1f;
	GuardBreakChance = 0.f;
	AttackCooldownMin = 2.2f;
	AttackCooldownMax = 3.6f;
	GetCharacterMovement()->MaxWalkSpeed = 300.f;

	DrawGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("DrawGlow"));
	DrawGlow->SetupAttachment(RootComponent);
	DrawGlow->SetRelativeLocation(FVector(50.f, 0.f, 50.f));
	DrawGlow->SetIntensity(0.f);
	DrawGlow->SetAttenuationRadius(450.f);
	DrawGlow->SetLightColor(FLinearColor(1.f, 0.5f, 0.15f));
	DrawGlow->SetCastShadows(false);
}

void AArcherSoldier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bDrawing = Combat->GetAttackKind() == EAttackKind::Shot && Combat->GetPhase() == EAttackPhase::Windup;
	DrawGlow->SetIntensity(bDrawing ? 1500.f : 0.f);
}

void AArcherSoldier::FireProjectile(const FAttackSpec& Spec)
{
	const APawn* Target = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Target || !ArrowClass)
	{
		return;
	}

	const FVector Muzzle = GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 50.f);
	const float Flight = FVector::Dist(Muzzle, Target->GetActorLocation()) / ArrowSpeed;
	const FVector Aim = Target->GetActorLocation() + Target->GetVelocity() * Flight * LeadFraction;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Velocity = (Aim - Muzzle).GetSafeNormal() * ArrowSpeed;
	if (AArrowProjectile* Arrow = GetWorld()->SpawnActor<AArrowProjectile>(ArrowClass, Muzzle, Velocity.Rotation(), Params))
	{
		Arrow->Launch(this, Spec, Velocity);
	}
	Combat->PlayReaction(ShotReleaseMontage);
}

bool AArcherAIController::TryAttack(AEnemySoldier* Me, AConquerCharacterBase* Target, UCombatComponent* MyCombat, float Dist)
{
	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	const bool bClose = Dist <= 300.f;
	if (!bClose && (Dist > MyCombat->ShotAttack.Range || !LineOfSightTo(Target)))
	{
		return false;
	}
	if (GameMode && !GameMode->TryTakeAttackToken(Me))
	{
		AttackCooldown = 0.4f;
		return false;
	}
	// Up close he clubs her with the bow; otherwise he draws and shoots.
	return bClose ? MyCombat->StartAttack(EAttackKind::Light, EGuardDirection::Right) : MyCombat->StartAttack(EAttackKind::Shot, EGuardDirection::Top);
}
