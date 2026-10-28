#include "LevelTriggerVolume.h"
#include "WrenCharacter.h"
#include "ConquerGameMode.h"
#include "Components/BoxComponent.h"

ALevelTriggerVolume::ALevelTriggerVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetCanEverAffectNavigation(false);
	RootComponent = Box;
}

void ALevelTriggerVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ALevelTriggerVolume::HandleOverlap);
}

void ALevelTriggerVolume::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AWrenCharacter* Wren = Cast<AWrenCharacter>(OtherActor);
	AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	if (Wren && !Wren->IsDead() && GameMode)
	{
		GameMode->NotifyLevelTrigger(this);
	}
}
