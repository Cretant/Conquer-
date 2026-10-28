#include "EncounterVolume.h"
#include "EnemySoldier.h"
#include "WrenCharacter.h"
#include "ConquerGameMode.h"
#include "Conquer.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

AEncounterVolume::AEncounterVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(400.f, 400.f, 200.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetCanEverAffectNavigation(false);
	RootComponent = Box;
}

void AEncounterVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &AEncounterVolume::HandleOverlap);
	SetBlockersActive(false);
}

void AEncounterVolume::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AWrenCharacter* Wren = Cast<AWrenCharacter>(OtherActor);
	if (Wren && !Wren->IsDead() && State == EEncounterState::Waiting)
	{
		Start();
	}
}

void AEncounterVolume::Start()
{
	if (State != EEncounterState::Waiting)
	{
		return;
	}

	State = EEncounterState::Running;
	NextWave = 0;
	NextWaveTime = GetWorld()->GetTimeSeconds() + (Waves.Num() > 0 ? Waves[0].Delay : 0.f);
	SetBlockersActive(true);
	UE_LOG(LogConquer, Log, TEXT("Encounter %s starts (%d waves)"), *EncounterName, Waves.Num());

	if (AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>())
	{
		if (bBossFight)
		{
			GameMode->ShowBanner(EncounterName, 3.f);
		}
		if (!StartHint.IsEmpty())
		{
			GameMode->ShowHint(StartHint, 8.f);
		}
	}
	OnStarted.Broadcast(this);
}

void AEncounterVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (OpenElapsed >= 0.f)
	{
		// Raise the gate actors out of the way.
		OpenElapsed += DeltaSeconds;
		const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Min(OpenElapsed / FMath::Max(OpenSeconds, 0.01f), 1.f));
		for (int32 i = 0; i < OpenOnDone.Num() && i < OpenFrom.Num(); ++i)
		{
			if (OpenOnDone[i])
			{
				OpenOnDone[i]->SetActorLocation(OpenFrom[i] + FVector(0.f, 0.f, OpenHeight * Alpha));
			}
		}
		if (Alpha >= 1.f)
		{
			OpenElapsed = -1.f;
		}
	}

	if (State != EEncounterState::Running)
	{
		return;
	}

	const int32 Alive = CountAlive();
	if (NextWave < Waves.Num())
	{
		if (NextWaveTime < 0.f && Alive <= Waves[NextWave].StartWhenAliveAtMost)
		{
			NextWaveTime = GetWorld()->GetTimeSeconds() + Waves[NextWave].Delay;
		}
		if (NextWaveTime >= 0.f && GetWorld()->GetTimeSeconds() >= NextWaveTime)
		{
			SpawnWave(Waves[NextWave++]);
			NextWaveTime = -1.f;
		}
		return;
	}

	if (Alive == 0)
	{
		// The way back stays shut: there is no going back once she's through.
		State = EEncounterState::Done;
		UE_LOG(LogConquer, Log, TEXT("Encounter %s cleared"), *EncounterName);

		OpenFrom.Reset();
		for (AActor* Gate : OpenOnDone)
		{
			OpenFrom.Add(Gate ? Gate->GetActorLocation() : FVector::ZeroVector);
		}
		OpenElapsed = OpenOnDone.Num() > 0 ? 0.f : -1.f;
		OnDone.Broadcast(this);
	}
}

void AEncounterVolume::SpawnWave(const FEncounterWave& Wave)
{
	UClass* Class = Wave.EnemyClass ? Wave.EnemyClass.Get() : AEnemySoldier::StaticClass();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i = 0; i < Wave.Count; ++i)
	{
		const FVector Location = SpawnPoints.Num() > 0 ? SpawnPoints[NextSpawnPoint++ % SpawnPoints.Num()] : GetActorLocation();
		const FVector Facing = Player ? Player->GetActorLocation() - Location : GetActorForwardVector();
		if (AEnemySoldier* Enemy = GetWorld()->SpawnActor<AEnemySoldier>(Class, Location + FVector(0.f, 0.f, 100.f), FRotator(0.f, Facing.Rotation().Yaw, 0.f), Params))
		{
			Enemies.Add(Enemy);
		}
	}
	UE_LOG(LogConquer, Log, TEXT("Encounter %s: wave of %d %s"), *EncounterName, Wave.Count, *Class->GetName());
}

int32 AEncounterVolume::CountAlive() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AEnemySoldier>& Enemy : Enemies)
	{
		if (Enemy.IsValid() && !Enemy->IsDead())
		{
			++Alive;
		}
	}
	return Alive;
}

TArray<AEnemySoldier*> AEncounterVolume::GetLivingEnemies() const
{
	TArray<AEnemySoldier*> Living;
	for (const TWeakObjectPtr<AEnemySoldier>& Enemy : Enemies)
	{
		if (Enemy.IsValid() && !Enemy->IsDead())
		{
			Living.Add(Enemy.Get());
		}
	}
	return Living;
}

void AEncounterVolume::ResetEncounter()
{
	if (State != EEncounterState::Running)
	{
		return;
	}

	for (const TWeakObjectPtr<AEnemySoldier>& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	State = EEncounterState::Waiting;
	NextWave = 0;
	NextWaveTime = -1.f;
	NextSpawnPoint = 0;
	SetBlockersActive(false);
	UE_LOG(LogConquer, Log, TEXT("Encounter %s reset"), *EncounterName);
}

void AEncounterVolume::SetBlockersActive(bool bActive)
{
	for (AActor* Blocker : CloseOnStart)
	{
		if (Blocker)
		{
			Blocker->SetActorHiddenInGame(!bActive);
			Blocker->SetActorEnableCollision(bActive);
			// Lights ignore the actor's hidden flag, so switch the components too.
			TInlineComponentArray<USceneComponent*> Components(Blocker);
			for (USceneComponent* Component : Components)
			{
				Component->SetVisibility(bActive);
			}
		}
	}
}
