#include "BurningPatch.h"
#include "ConquerCharacterBase.h"
#include "AttributesComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABurningPatch::ABurningPatch()
{
	PrimaryActorTick.bCanEverTick = true;

	Flames = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flames"));
	RootComponent = Flames;
	Flames->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flames->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Flames->SetStaticMesh(Cylinder.Object);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Flames);
	Glow->SetLightColor(FLinearColor(1.f, 0.45f, 0.12f));
	Glow->SetIntensity(6000.f);
	Glow->SetAttenuationRadius(450.f);
	Glow->SetCastShadows(false);
}

void ABurningPatch::BeginPlay()
{
	Super::BeginPlay();

	// The basic cylinder is 100 units across and tall: flatten it into a disc of the right size.
	Flames->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.03f));
	Glow->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 60.f));
	if (UMaterialInterface* Material = MaterialPath.IsValid() ? Cast<UMaterialInterface>(MaterialPath.TryLoad()) : nullptr)
	{
		Flames->SetMaterial(0, Material);
	}
}

void ABurningPatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	Glow->SetIntensity(6000.f * (0.8f + 0.2f * FMath::Sin(Age * 11.f) * FMath::Sin(Age * 3.7f)));

	DamageTimer += DeltaSeconds;
	if (DamageTimer < 0.5f)
	{
		return;
	}
	const float Tick = DamageTimer;
	DamageTimer = 0.f;

	AConquerCharacterBase* Player = Cast<AConquerCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || Player->IsDead())
	{
		return;
	}
	const FVector Delta = Player->GetActorLocation() - GetActorLocation();
	if (Delta.Size2D() <= Radius + Player->GetCapsuleRadius() && FMath::Abs(Delta.Z) < 200.f)
	{
		Player->GetAttributes()->ApplyDamage(DamagePerSecond * Tick, this);
	}
}
