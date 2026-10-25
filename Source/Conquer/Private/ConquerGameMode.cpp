#include "ConquerGameMode.h"
#include "WrenCharacter.h"
#include "EnemySoldier.h"
#include "AttributesComponent.h"
#include "ConquerHUD.h"
#include "Conquer.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AConquerGameMode::AConquerGameMode()
{
	DefaultPawnClass = AWrenCharacter::StaticClass();
	HUDClass = AConquerHUD::StaticClass();
	SoldierClass = AEnemySoldier::StaticClass();
	CaptainClass = AEnemyCaptain::StaticClass();
}

void AConquerGameMode::BeginPlay()
{
	Super::BeginPlay();

	bool bLevelHasEnemies = false;
	for (TActorIterator<AEnemySoldier> It(GetWorld()); It; ++It)
	{
		bLevelHasEnemies = true;
		break;
	}

	if (bSpawnArenaWaves && !bLevelHasEnemies)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AConquerGameMode::SpawnFirstWave, 1.5f, false);
	}
}

FVector AConquerGameMode::GetArenaCentre() const
{
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return Player->GetActorLocation();
	}
	return FVector::ZeroVector;
}

AEnemySoldier* AConquerGameMode::SpawnEnemy(TSubclassOf<AEnemySoldier> Class, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FRotator FacingCentre = (GetArenaCentre() - Location).Rotation();
	AEnemySoldier* Enemy = GetWorld()->SpawnActor<AEnemySoldier>(Class, Location + FVector(0, 0, 100.f), FRotator(0.f, FacingCentre.Yaw, 0.f), Params);
	if (Enemy)
	{
		Enemy->GetAttributes()->OnDeath.AddDynamic(this, &AConquerGameMode::HandleEnemyDeath);
	}
	return Enemy;
}

void AConquerGameMode::SpawnFirstWave()
{
	const FVector Centre = GetArenaCentre();
	AliveInWave = 0;
	for (int32 i = 0; i < SoldiersInFirstWave; ++i)
	{
		const float Angle = (360.f / SoldiersInFirstWave) * i + 30.f;
		const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * SpawnRadius;
		if (SpawnEnemy(SoldierClass, Centre + Offset))
		{
			++AliveInWave;
		}
	}
	UE_LOG(LogConquer, Log, TEXT("Arena: spawned %d soldiers"), AliveInWave);
}

void AConquerGameMode::SpawnCaptain()
{
	bCaptainSpawned = true;
	const FVector Centre = GetArenaCentre();
	SpawnEnemy(CaptainClass, Centre + FRotator(0.f, 180.f, 0.f).Vector() * SpawnRadius);
	UE_LOG(LogConquer, Log, TEXT("Arena: the gate captain arrives"));
}

void AConquerGameMode::HandleEnemyDeath(AActor* Killer)
{
	if (bCaptainSpawned)
	{
		UE_LOG(LogConquer, Log, TEXT("Arena cleared"));
		return;
	}

	if (--AliveInWave <= 0)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AConquerGameMode::SpawnCaptain, 3.f, false);
	}
}

bool AConquerGameMode::TryTakeAttackToken(AActor* Enemy)
{
	const float Now = GetWorld()->GetTimeSeconds();

	int32 Active = 0;
	for (auto It = AttackTokenExpiry.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now)
		{
			It.RemoveCurrent();
			continue;
		}
		if (It.Key().Get() != Enemy)
		{
			++Active;
		}
	}

	if (Active >= MaxSimultaneousAttackers)
	{
		return false;
	}
	AttackTokenExpiry.Add(Enemy, Now + AttackTokenDuration);
	return true;
}
