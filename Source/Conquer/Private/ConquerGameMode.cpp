#include "ConquerGameMode.h"
#include "WrenCharacter.h"
#include "EnemySoldier.h"
#include "AttributesComponent.h"
#include "ConquerHUD.h"
#include "CombatTestDriver.h"
#include "SparkBurst.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Conquer.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"

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

#if !UE_BUILD_SHIPPING
	// -ConquerCombatTest: the duel mechanics, run headless instead of the arena waves.
	if (FParse::Param(FCommandLine::Get(), TEXT("ConquerCombatTest")) || FParse::Param(FCommandLine::Get(), TEXT("ConquerMoveTest"))
		|| FParse::Param(FCommandLine::Get(), TEXT("ConquerFightFilm")))
	{
		bSpawnArenaWaves = false;
		GetWorld()->SpawnActor<ACombatTestDriver>();
	}
#endif

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

#if !UE_BUILD_SHIPPING
	// -ConquerCaptainTest brings the gate captain in with the first wave.
	if (bSpawnArenaWaves && FParse::Param(FCommandLine::Get(), TEXT("ConquerCaptainTest")))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AConquerGameMode::SpawnCaptain, 2.f, false);
	}

	// Headless checks: -ConquerShots=4,6,8 saves a screenshot (Saved/Screenshots) at each of those seconds.
	FString Shots;
	if (FParse::Value(FCommandLine::Get(), TEXT("ConquerShots="), Shots, false))
	{
		TArray<FString> Times;
		Shots.ParseIntoArray(Times, TEXT(","));
		for (const FString& Time : Times)
		{
			PendingShotTimes.Add(FCString::Atof(*Time));
		}
		PendingShotTimes.Sort();
		ScheduleNextShot();
	}

	// -ConquerShotView=X,Y,Z,Pitch,Yaw watches from a fixed camera instead of over Wren's shoulder
	// (add -ConquerShotHideHUD for a clean picture, e.g. a map from above).
	FString View;
	if (FParse::Value(FCommandLine::Get(), TEXT("ConquerShotView="), View, false))
	{
		TArray<FString> Parts;
		View.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() == 5)
		{
			const FVector Location(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			const FRotator Rotation(FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]), 0.f);
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Location, Rotation]()
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					PC->SetViewTarget(GetWorld()->SpawnActor<ACameraActor>(Location, Rotation));
					if (PC->MyHUD && FParse::Param(FCommandLine::Get(), TEXT("ConquerShotHideHUD")))
					{
						PC->MyHUD->bShowHUD = false;
					}
				}
			}), 0.5f, false);
		}
	}
#endif
}

void AConquerGameMode::ScheduleNextShot()
{
	if (PendingShotTimes.Num() == 0)
	{
		return;
	}
	const float Delay = FMath::Max(0.1f, PendingShotTimes[0] - GetWorld()->GetTimeSeconds());
	GetWorldTimerManager().SetTimer(ShotTimer, this, &AConquerGameMode::TakeShot, Delay, false);
}

void AConquerGameMode::TakeShot()
{
	PendingShotTimes.RemoveAt(0);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->ConsoleCommand(TEXT("shot"));
		const APawn* Pawn = PC->GetPawn();
		UE_LOG(LogConquer, Log, TEXT("Shot at %.1fs: player %s at %s, camera at %s"), GetWorld()->GetTimeSeconds(),
			Pawn ? *Pawn->GetName() : TEXT("none"), Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("-"),
			*PC->PlayerCameraManager->GetCameraLocation().ToCompactString());
	}
	ScheduleNextShot();
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
	if (!bSpawnArenaWaves && !bCaptainSpawned)
	{
		return;
	}
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

void AConquerGameMode::ShowBanner(const FString& Text, float Duration)
{
	Banner = Text;
	BannerStart = GetWorld()->GetTimeSeconds();
	BannerEnd = BannerStart + Duration;
	UE_LOG(LogConquer, Log, TEXT("Banner: %s"), *Text);
}

void AConquerGameMode::ShowHint(const FString& Text, float Duration)
{
	Hint = Text;
	HintStart = GetWorld()->GetTimeSeconds();
	HintEnd = HintStart + Duration;
}

void AConquerGameMode::QueueHint(const FString& Text, float Duration)
{
	if (GetWorld()->GetTimeSeconds() >= HintEnd - 0.5f && PendingHints.Num() == 0)
	{
		ShowHint(Text, Duration);
		return;
	}
	PendingHints.Add(TPair<FString, float>(Text, Duration));
	if (!GetWorldTimerManager().IsTimerActive(HintTimer))
	{
		GetWorldTimerManager().SetTimer(HintTimer, this, &AConquerGameMode::ShowNextHint, FMath::Max(HintEnd - GetWorld()->GetTimeSeconds(), 0.1f), false);
	}
}

void AConquerGameMode::ShowNextHint()
{
	if (PendingHints.Num() == 0)
	{
		return;
	}
	const TPair<FString, float> Next = PendingHints[0];
	PendingHints.RemoveAt(0);
	ShowHint(Next.Key, Next.Value);
	if (PendingHints.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(HintTimer, this, &AConquerGameMode::ShowNextHint, Next.Value, false);
	}
}

void AConquerGameMode::ShowFirstTimeHint(FName Key, const FString& Text, float Duration)
{
	if (!HintsShown.Contains(Key))
	{
		HintsShown.Add(Key);
		QueueHint(Text, Duration);
	}
}

void AConquerGameMode::NotifyEnemySpawned(AEnemySoldier* Enemy)
{
	if (Enemy && !Enemy->FirstSightHint.IsEmpty())
	{
		ShowFirstTimeHint(Enemy->GetClass()->GetFName(), Enemy->FirstSightHint, 8.f);
	}
}

void AConquerGameMode::HitStop(float Seconds, AActor* One, AActor* Two)
{
	// Each fighter's own clock slows almost to a stop; everyone else carries on. Rage's speed-up is kept and restored.
	for (AActor* Actor : { One, Two })
	{
		if (Actor && !Frozen.Contains(Actor))
		{
			Frozen.Add(Actor, Actor->CustomTimeDilation);
			Actor->CustomTimeDilation = 0.05f;
		}
	}
	GetWorldTimerManager().SetTimer(HitStopTimer, this, &AConquerGameMode::EndHitStop, Seconds, false);
}

void AConquerGameMode::EndHitStop()
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& Entry : Frozen)
	{
		if (AActor* Actor = Entry.Key.Get())
		{
			Actor->CustomTimeDilation = Entry.Value;
		}
	}
	Frozen.Reset();
}

USoundBase* AConquerGameMode::LoadSound(const FSoftObjectPath& Path)
{
	if (!Path.IsValid())
	{
		return nullptr;
	}
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Path.ToString()))
	{
		return *Found;
	}
	USoundBase* Sound = Cast<USoundBase>(Path.TryLoad());
	Sounds.Add(Path.ToString(), Sound);
	return Sound;
}

void AConquerGameMode::PlaySwing(AActor* Swinger, EAttackKind Kind)
{
	if (Swinger && Kind != EAttackKind::Shot)
	{
		const bool bHeavy = Kind == EAttackKind::Heavy || Kind == EAttackKind::Thrust;
		if (USoundBase* Sound = LoadSound(bHeavy ? HeavySwingSoundPath : SwingSoundPath))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Swinger->GetActorLocation(), FMath::FRandRange(0.8f, 1.f), FMath::FRandRange(0.9f, 1.1f));
		}
	}
}

void AConquerGameMode::SpawnSparks(const FVector& Where, const FVector& Direction, float Strength, const FLinearColor& LightColor)
{
	if (!SparkMaterial && SparkMaterialPath.IsValid())
	{
		SparkMaterial = Cast<UMaterialInterface>(SparkMaterialPath.TryLoad());
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASparkBurst* Burst = GetWorld()->SpawnActor<ASparkBurst>(Where, FRotator::ZeroRotator, Params))
	{
		Burst->Burst(Direction, Strength, SparkMaterial, LightColor);
	}
}

void AConquerGameMode::PlayCombatEffects(EHitOutcome Outcome, EAttackKind Kind, AConquerCharacterBase* Attacker, AConquerCharacterBase* Defender)
{
	if (!Defender)
	{
		return;
	}
	// Where the weapons meet: between the two at chest height (or at the defender, for an arrow).
	const FVector At = Defender->GetActorLocation() + FVector(0.f, 0.f, 45.f);
	const FVector Toward = Attacker ? (Attacker->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D() : Defender->GetActorForwardVector();
	const FVector Contact = At + Toward * (Defender->GetCapsuleRadius() + 25.f);
	const bool bBig = Kind == EAttackKind::Heavy || Kind == EAttackKind::Thrust;

	USoundBase* Sound = nullptr;
	float Kick = 0.f;
	switch (Outcome)
	{
	case EHitOutcome::Parried:
		SpawnSparks(Contact, Toward, 1.8f, FLinearColor(1.f, 0.8f, 0.35f));
		Sound = LoadSound(ParrySoundPath);
		Kick = 0.8f;
		break;
	case EHitOutcome::Blocked:
		SpawnSparks(Contact, Toward, bBig ? 1.f : 0.6f, FLinearColor(1.f, 0.7f, 0.4f));
		Sound = LoadSound(BlockSoundPath);
		Kick = bBig ? 0.5f : 0.25f;
		break;
	case EHitOutcome::GuardBroken:
	case EHitOutcome::Countered:
		Sound = LoadSound(ThudSoundPath);
		Kick = 0.5f;
		break;
	case EHitOutcome::Hit:
		Sound = LoadSound(HitSoundPath);
		Kick = bBig ? 0.7f : 0.3f;
		break;
	default:
		break;
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Contact, 1.f, FMath::FRandRange(0.9f, 1.1f));
	}
	if (Kick > 0.f)
	{
		Defender->OnCombatImpact(Kick);
		if (Attacker)
		{
			Attacker->OnCombatImpact(Kick * 0.6f);
		}
	}
}

void AConquerGameMode::Flash(const FLinearColor& Color, float Seconds)
{
	FlashColor = Color;
	FlashStart = GetWorld()->GetRealTimeSeconds();
	FlashSeconds = FMath::Max(Seconds, 0.01f);
}

FLinearColor AConquerGameMode::GetFlash() const
{
	const float Fade = 1.f - (GetWorld()->GetRealTimeSeconds() - FlashStart) / FlashSeconds;
	FLinearColor Color = FlashColor;
	Color.A *= FMath::Clamp(Fade, 0.f, 1.f);
	return Color;
}

void AConquerGameMode::ShowCallout(const FString& Text)
{
	Callout = Text;
	CalloutStart = GetWorld()->GetRealTimeSeconds();
}

float AConquerGameMode::GetCalloutAlpha() const
{
	const float Age = GetWorld()->GetRealTimeSeconds() - CalloutStart;
	return FMath::Clamp(1.f - (Age - 0.4f) / 0.4f, 0.f, 1.f);
}

float AConquerGameMode::MessageAlpha(float Start, float End) const
{
	// Fades in over a third of a second and out over the last half second.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= End)
	{
		return 0.f;
	}
	return FMath::Clamp(FMath::Min((Now - Start) / 0.3f, (End - Now) / 0.5f), 0.f, 1.f);
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
