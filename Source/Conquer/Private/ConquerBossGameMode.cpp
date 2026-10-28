#include "ConquerBossGameMode.h"
#include "GarrickBoss.h"
#include "Conquer.h"
#include "AttributesComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

AConquerBossGameMode::AConquerBossGameMode()
{
	bSpawnArenaWaves = false;
	BossClass = AGarrickBoss::StaticClass();
}

void AConquerBossGameMode::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &AConquerBossGameMode::SpawnBoss, 1.5f, false);
}

void AConquerBossGameMode::SpawnBoss()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Forward = Player ? Player->GetActorForwardVector() : FVector::ForwardVector;
	bCaptainSpawned = true;  // the boss's death ends the fight
	Boss = SpawnEnemy(BossClass, GetArenaCentre() + Forward * BossDistance);
	if (Boss.IsValid())
	{
		UE_LOG(LogConquer, Log, TEXT("Keep Hall: the boss enters"));
	}

#if !UE_BUILD_SHIPPING
	// Headless check of the whole fight: -ConquerBossTest wounds the boss into phase two, then down, then executes him.
	if (FParse::Param(FCommandLine::Get(), TEXT("ConquerBossTest")))
	{
		GetWorldTimerManager().SetTimer(TestTimer, this, &AConquerBossGameMode::AdvanceBossTest, 4.5f, false);
		// Nobody is at the controls, so Wren can't die (a death would restart the level mid-test).
		if (AConquerCharacterBase* Wren = Cast<AConquerCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Wren->GetAttributes()->MinHealth = 1.f;
		}
	}
#endif
}

void AConquerBossGameMode::AdvanceBossTest()
{
	AConquerCharacterBase* Target = Boss.Get();
	if (!Target)
	{
		return;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	switch (TestStep++)
	{
	case 0: Target->GetAttributes()->ApplyDamage(250.f, Player); break;  // into phase two
	case 1: Target->GetAttributes()->ApplyDamage(400.f, Player); break;  // down on one knee
	case 2: Target->Kill(Player); break;                                // the execution
	default: return;
	}
	UE_LOG(LogConquer, Log, TEXT("Boss test step %d: %s at %.0f health"), TestStep, *Target->GetName(), Target->GetAttributes()->Health);
	GetWorldTimerManager().SetTimer(TestTimer, this, &AConquerBossGameMode::AdvanceBossTest, 5.f, false);
}
