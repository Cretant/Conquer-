#include "ConquerPickup.h"
#include "WrenCharacter.h"
#include "AttributesComponent.h"
#include "ConquerGameMode.h"
#include "Conquer.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"

AConquerPickup::AConquerPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(120.f);
	Sphere->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Sphere;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Sphere);
	Glow->SetIntensity(800.f);
	Glow->SetAttenuationRadius(300.f);
	Glow->SetLightColor(FLinearColor(0.6f, 0.8f, 1.f));
	Glow->SetCastShadows(false);
}

void AConquerPickup::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AConquerPickup::HandleOverlap);
}

void AConquerPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Mesh->AddLocalRotation(FRotator(0.f, 60.f * DeltaSeconds, 0.f));
}

void AConquerPickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AWrenCharacter* Wren = Cast<AWrenCharacter>(OtherActor);
	if (!Wren || Wren->IsDead())
	{
		return;
	}

	switch (Kind)
	{
	case EPickupKind::Health:    Wren->GetAttributes()->Heal(Amount); break;
	case EPickupKind::Whetstone: Wren->SetDamageBonus(Amount); break;
	}
	UE_LOG(LogConquer, Log, TEXT("Wren picks up %s"), *Label);

	if (AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>())
	{
		GameMode->NotifyPickup(Kind, Amount);
		if (!Label.IsEmpty())
		{
			GameMode->ShowHint(Label, 4.f);
		}
	}
	Destroy();
}
