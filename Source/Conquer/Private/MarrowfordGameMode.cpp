#include "MarrowfordGameMode.h"
#include "WrenCharacter.h"
#include "EnemySoldier.h"
#include "EncounterVolume.h"
#include "LevelTriggerVolume.h"
#include "ConquerPickup.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "Conquer.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"

AMarrowfordGameMode::AMarrowfordGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	bSpawnArenaWaves = false;
	bShowControlsHint = false;
	MenuLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/MainMenu.MainMenu")));
}

void AMarrowfordGameMode::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<AEncounterVolume> It(GetWorld()); It; ++It)
	{
		Encounters.Add(*It);
		It->OnStarted.AddDynamic(this, &AMarrowfordGameMode::HandleEncounterStarted);
		It->OnDone.AddDynamic(this, &AMarrowfordGameMode::HandleEncounterDone);
	}
	SetShown(Tagged(TEXT("FarmFire")), false);

	// Black until the first shot of the opening is up.
	FadeAlpha = 1.f;
	bLetterbox = true;

#if !UE_BUILD_SHIPPING
	bTestMode = FParse::Param(FCommandLine::Get(), TEXT("ConquerMarrowfordTest"));
#endif
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &AMarrowfordGameMode::StartIntro, 0.1f, false);
	UE_LOG(LogConquer, Log, TEXT("Marrowford: %d encounters"), Encounters.Num());
}

void AMarrowfordGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	if (FadeDuration > 0.f)
	{
		const float Alpha = FMath::Clamp((Now - FadeStart) / FadeDuration, 0.f, 1.f);
		FadeAlpha = FMath::Lerp(FadeFrom, FadeTarget, Alpha);
		if (Alpha >= 1.f)
		{
			FadeDuration = 0.f;
		}
	}

	for (int32 i = Moves.Num() - 1; i >= 0; --i)
	{
		FMove& Move = Moves[i];
		AActor* Actor = Move.Actor.Get();
		const float Alpha = FMath::Clamp((Now - Move.Start) / Move.Duration, 0.f, 1.f);
		if (Actor)
		{
			Actor->SetActorLocation(FMath::Lerp(Move.From, Move.To, Alpha));
		}
		if (!Actor || Alpha >= 1.f)
		{
			if (Actor && Move.bIdleOnArrival)
			{
				PlayLoop(Actor, IntroIdleAnimPaths);
			}
			Moves.RemoveAt(i);
		}
	}
}

// --- The opening ---------------------------------------------------------------

void AMarrowfordGameMode::StartIntro()
{
	AWrenCharacter* Wren = GetWren();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Wren || !PC)
	{
		UE_LOG(LogConquer, Warning, TEXT("Marrowford: no Wren to start the opening with"));
		return;
	}
	SetUpWren(Wren);

	if (PC->InputComponent)
	{
		FInputKeyBinding& Skip = PC->InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AMarrowfordGameMode::OnSkipKey);
		Skip.bConsumeInput = false;
	}
	IntroStartRealTime = GetWorld()->GetRealTimeSeconds();

	if (bTestMode || FParse::Param(FCommandLine::Get(), TEXT("ConquerSkipIntro")))
	{
		EndIntro();
		if (bTestMode)
		{
			GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, 1.f, false);
		}
		return;
	}

	Wren->DisableInput(PC);
	Wren->ShowWeapon(false);
	for (AActor* Soldier : Tagged(TEXT("IntroSoldier")))
	{
		IntroSoldiers.Add(Soldier);
		IntroSoldierHomes.Add(Soldier->GetActorLocation());
	}
	IntroStep = 0;
	RunIntroStep();
}

void AMarrowfordGameMode::RunIntroStep()
{
	AWrenCharacter* Wren = GetWren();
	if (!Wren || bIntroDone)
	{
		return;
	}

	FVector FamilyCentre = Wren->GetActorLocation();
	const TArray<AActor*> Family = Tagged(TEXT("Family"));
	if (Family.Num() > 0)
	{
		FamilyCentre = FVector::ZeroVector;
		for (const AActor* Member : Family)
		{
			FamilyCentre += Member->GetActorLocation() / Family.Num();
		}
	}

	float Next = 0.f;
	switch (IntroStep++)
	{
	case 0:
		// Dusk at the farm. Soldiers come up the lane towards her family.
		ViewFrom(TEXT("IntroCam1"));
		FadeTo(0.f, 2.f);
		for (int32 i = 0; i < IntroSoldiers.Num(); ++i)
		{
			if (AActor* Soldier = IntroSoldiers[i].Get())
			{
				const FVector Home = IntroSoldierHomes[i];
				const FVector Stop = FamilyCentre + (Home - FamilyCentre).GetSafeNormal2D() * (350.f + 120.f * i);
				RunTo(Soldier, FVector(Stop.X, Stop.Y, Home.Z));
			}
		}
		Next = 4.f;
		break;
	case 1:
		ViewFrom(TEXT("IntroCam2"));
		Next = 3.f;
		break;
	case 2:
		ViewFrom(TEXT("IntroCam3"));
		Next = 2.f;
		break;
	case 3:
		// The blow is never shown: a hard cut to black. Her family is gone, the farmhouse burns, she is down.
		FadeAlpha = 1.f;
		FadeDuration = 0.f;
		Moves.Reset();
		SetShown(Family, false);
		SetShown(Tagged(TEXT("FarmFire")), true);
		Wren->SetKnockedDown(true);
		for (int32 i = 0; i < IntroSoldiers.Num(); ++i)
		{
			if (AActor* Soldier = IntroSoldiers[i].Get())
			{
				const FVector Around = Wren->GetActorLocation() + FRotator(0.f, 40.f + 110.f * i, 0.f).Vector() * 260.f;
				Soldier->SetActorLocation(FVector(Around.X, Around.Y, IntroSoldierHomes[i].Z));
				PlayLoop(Soldier, IntroIdleAnimPaths);
			}
		}
		Next = 2.f;
		break;
	case 4:
		// They leave her for dead and walk back down the lane.
		ViewFrom(TEXT("IntroCam4"));
		FadeTo(0.f, 1.2f);
		for (int32 i = 0; i < IntroSoldiers.Num(); ++i)
		{
			if (AActor* Soldier = IntroSoldiers[i].Get())
			{
				RunTo(Soldier, IntroSoldierHomes[i]);
			}
		}
		Next = 4.5f;
		break;
	case 5:
		FadeTo(1.f, 1.f);
		Next = 1.2f;
		break;
	case 6:
		// Later: she gets up among the flames, next to the pitchfork.
		Moves.Reset();
		for (const TWeakObjectPtr<AActor>& Soldier : IntroSoldiers)
		{
			SetShown({ Soldier.Get() }, false);
		}
		SetShown(Tagged(TEXT("IntroTorch")), false);
		Wren->SetKnockedDown(false);
		ViewFrom(TEXT("IntroCam5"));
		FadeTo(0.f, 1.f);
		Next = 2.5f;
		break;
	case 7:
		SetShown(Tagged(TEXT("PitchforkProp")), false);
		Wren->ShowWeapon(true);
		Next = 1.5f;
		break;
	default:
		EndIntro();
		return;
	}
	GetWorldTimerManager().SetTimer(IntroTimer, this, &AMarrowfordGameMode::RunIntroStep, Next, false);
}

void AMarrowfordGameMode::OnSkipKey(FKey Key)
{
	if (!bIntroDone && GetWorld()->GetRealTimeSeconds() - IntroStartRealTime > 0.6f)
	{
		UE_LOG(LogConquer, Log, TEXT("Marrowford: opening skipped (%s)"), *Key.ToString());
		EndIntro();
	}
}

void AMarrowfordGameMode::EndIntro()
{
	if (bIntroDone)
	{
		return;
	}
	bIntroDone = true;
	GetWorldTimerManager().ClearTimer(IntroTimer);
	Moves.Reset();

	SetShown(Tagged(TEXT("Family")), false);
	SetShown(Tagged(TEXT("IntroSoldier")), false);
	SetShown(Tagged(TEXT("IntroTorch")), false);
	SetShown(Tagged(TEXT("PitchforkProp")), false);
	SetShown(Tagged(TEXT("FarmFire")), true);

	bLetterbox = false;
	FadeTo(0.f, 0.6f);

	AWrenCharacter* Wren = GetWren();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (Wren && PC)
	{
		Wren->SetKnockedDown(false);
		Wren->ShowWeapon(true);
		Wren->EnableInput(PC);
		PC->SetControlRotation(FRotator(-10.f, Wren->GetActorRotation().Yaw, 0.f));
		PC->SetViewTargetWithBlend(Wren, 0.6f);
	}
	ShowBanner(TEXT("Marrowford"), 3.5f);
	ShowHint(TEXT("Move  WASD / left stick        Camera  mouse / right stick"), 7.f);
	UE_LOG(LogConquer, Log, TEXT("Marrowford: the opening ends, Wren takes up the pitchfork"));
}

void AMarrowfordGameMode::ViewFrom(FName CameraTag)
{
	const TArray<AActor*> Cameras = Tagged(CameraTag);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && Cameras.Num() > 0)
	{
		PC->SetViewTargetWithBlend(Cameras[0], 0.f);
	}
}

void AMarrowfordGameMode::FadeTo(float Target, float Seconds)
{
	FadeFrom = FadeAlpha;
	FadeTarget = Target;
	FadeStart = GetWorld()->GetTimeSeconds();
	FadeDuration = FMath::Max(Seconds, 0.01f);
}

void AMarrowfordGameMode::MoveActor(AActor* Actor, const FVector& To, float Seconds)
{
	if (!Actor)
	{
		return;
	}
	FMove Move;
	Move.Actor = Actor;
	Move.From = Actor->GetActorLocation();
	Move.To = To;
	Move.Start = GetWorld()->GetTimeSeconds();
	Move.Duration = FMath::Max(Seconds, 0.01f);
	Moves.Add(Move);

	// The placeholder bodies face +Y in their own space.
	const FVector Dir = (To - Move.From).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		Actor->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw - 90.f, 0.f));
	}
}

void AMarrowfordGameMode::RunTo(AActor* Soldier, const FVector& To)
{
	if (!Soldier)
	{
		return;
	}
	PlayLoop(Soldier, IntroRunAnimPaths);
	MoveActor(Soldier, To, FVector::Dist2D(Soldier->GetActorLocation(), To) / FMath::Max(IntroRunSpeed, 1.f));
	Moves.Last().bIdleOnArrival = true;
}

void AMarrowfordGameMode::PlayLoop(AActor* Figure, const TArray<FSoftObjectPath>& Paths)
{
	ASkeletalMeshActor* Body = Cast<ASkeletalMeshActor>(Figure);
	if (!Body)
	{
		return;
	}
	for (const FSoftObjectPath& Path : Paths)
	{
		if (FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			if (UAnimSequence* Clip = Cast<UAnimSequence>(Path.TryLoad()))
			{
				Body->GetSkeletalMeshComponent()->PlayAnimation(Clip, true);
				return;
			}
		}
	}
}

TArray<AActor*> AMarrowfordGameMode::Tagged(FName Tag) const
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithTag(this, Tag, Actors);
	return Actors;
}

void AMarrowfordGameMode::SetShown(const TArray<AActor*>& Actors, bool bShown)
{
	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}
		Actor->SetActorHiddenInGame(!bShown);
		Actor->SetActorEnableCollision(bShown);
		// Lights ignore the actor's hidden flag, so switch the components too.
		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (USceneComponent* Component : Components)
		{
			Component->SetVisibility(bShown);
		}
	}
}

// --- Wren, fights and checkpoints ----------------------------------------------

AWrenCharacter* AMarrowfordGameMode::GetWren() const
{
	return Cast<AWrenCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AMarrowfordGameMode::SetUpWren(AWrenCharacter* Wren)
{
	if (!Wren)
	{
		return;
	}
	Wren->GetRage()->bEnabled = bRageUnlocked;
	Wren->EquipPitchfork(!bHasSword);
	Wren->SetDamageBonus(DamageBonus);
	Wren->GetAttributes()->OnDamaged.AddUniqueDynamic(this, &AMarrowfordGameMode::HandleWrenDamaged);
	if (bTestMode)
	{
		Wren->GetAttributes()->MinHealth = 1.f;
	}
}

bool AMarrowfordGameMode::HandlePlayerDeath(AWrenCharacter* Wren)
{
	if (bFinished)
	{
		return true;
	}
	bBossFightActive = false;

	FTimerHandle FadeHandle, RespawnHandle;
	GetWorldTimerManager().SetTimer(FadeHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { FadeTo(1.f, 0.8f); }), FMath::Max(RespawnDelay - 1.f, 0.1f), false);
	GetWorldTimerManager().SetTimer(RespawnHandle, this, &AMarrowfordGameMode::Respawn, RespawnDelay, false);
	return true;
}

void AMarrowfordGameMode::Respawn()
{
	for (AEncounterVolume* Encounter : Encounters)
	{
		Encounter->ResetEncounter();
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	if (APawn* Old = PC->GetPawn())
	{
		PC->UnPossess();
		Old->Destroy();
	}
	if (bHasCheckpoint)
	{
		RestartPlayerAtTransform(PC, CheckpointTransform);
	}
	else
	{
		RestartPlayer(PC);
	}

	AWrenCharacter* Wren = GetWren();
	SetUpWren(Wren);
	if (Wren)
	{
		PC->SetControlRotation(FRotator(-10.f, Wren->GetActorRotation().Yaw, 0.f));
	}
	FadeTo(0.f, 1.f);
	UE_LOG(LogConquer, Log, TEXT("Marrowford: Wren rises again at checkpoint %d"), CheckpointOrder);
}

void AMarrowfordGameMode::NotifyLevelTrigger(ALevelTriggerVolume* Trigger)
{
	if (Trigger->Kind == ELevelTrigger::LevelExit)
	{
		FinishLevel();
		return;
	}
	if (Trigger->Order > CheckpointOrder)
	{
		CheckpointOrder = Trigger->Order;
		CheckpointTransform = FTransform(FRotator(0.f, Trigger->GetActorRotation().Yaw, 0.f), Trigger->GetActorLocation());
		bHasCheckpoint = true;
		ShowHint(TEXT("Checkpoint"), 2.5f);
		UE_LOG(LogConquer, Log, TEXT("Marrowford: checkpoint %d (%s)"), CheckpointOrder, *Trigger->Label);
	}
}

void AMarrowfordGameMode::NotifyPickup(EPickupKind Kind, float Amount)
{
	if (Kind == EPickupKind::Whetstone)
	{
		DamageBonus = Amount;
	}
}

void AMarrowfordGameMode::HandleEncounterStarted(AEncounterVolume* Encounter)
{
	if (Encounter->bBossFight)
	{
		bBossFightActive = true;
	}
}

void AMarrowfordGameMode::HandleEncounterDone(AEncounterVolume* Encounter)
{
	if (!Encounter->bBossFight)
	{
		return;
	}
	bBossFightActive = false;
	bHasSword = true;
	if (AWrenCharacter* Wren = GetWren())
	{
		Wren->EquipPitchfork(false);
	}
	ShowHint(TEXT("She takes the captain's sword. The gate is open."), 6.f);
	UE_LOG(LogConquer, Log, TEXT("Marrowford: Wren takes the gate captain's sword"));
}

void AMarrowfordGameMode::HandleWrenDamaged(float Amount, AActor* DamageCauser)
{
	const AWrenCharacter* Wren = GetWren();
	if (!bBossFightActive || bRageUnlocked || !Wren || Wren->IsDead()
		|| Wren->GetAttributes()->GetHealthFraction() > RageUnlockHealthFraction)
	{
		return;
	}
	bRageUnlocked = true;
	GetWren()->UnlockRage();
	ShowBanner(TEXT("RAGE"), 2.f);
	ShowHint(TEXT("Rage  R / Y when the bar is full"), 6.f);
	UE_LOG(LogConquer, Log, TEXT("Marrowford: Rage wakes"));
}

void AMarrowfordGameMode::FinishLevel()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (AWrenCharacter* Wren = GetWren())
	{
		Wren->DisableInput(GetWorld()->GetFirstPlayerController());
	}
	FadeTo(1.f, 2.f);
	UE_LOG(LogConquer, Log, TEXT("Marrowford complete"));

	FTimerHandle BannerHandle, MenuHandle;
	GetWorldTimerManager().SetTimer(BannerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ShowBanner(TEXT("Marrowford"), 4.f);
		ShowHint(TEXT("Complete"), 4.f);
	}), 2.f, false);
	GetWorldTimerManager().SetTimer(MenuHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, MenuLevel);
	}), 6.5f, false);
}

// --- Headless walk-through -----------------------------------------------------

void AMarrowfordGameMode::RunTestStep()
{
	AWrenCharacter* Wren = GetWren();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Wren || !PC)
	{
		GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, 1.f, false);
		return;
	}

	auto Find = [this](const TCHAR* Name) -> AEncounterVolume*
	{
		for (AEncounterVolume* Encounter : Encounters)
		{
			if (Encounter->EncounterName.Contains(Name))
			{
				return Encounter;
			}
		}
		return nullptr;
	};
	auto Teleport = [Wren](const FVector& Where)
	{
		Wren->GetAttributes()->Heal(1000.f);
		Wren->SetActorLocation(Where + FVector(0.f, 0.f, 60.f), false, nullptr, ETeleportType::TeleportPhysics);
	};
	auto Enter = [&](const TCHAR* Name)
	{
		if (AEncounterVolume* Encounter = Find(Name))
		{
			Teleport(Encounter->GetActorLocation());
			UE_LOG(LogConquer, Log, TEXT("Test: entered %s, trigger %s"), Name,
				Encounter->GetState() == EEncounterState::Waiting ? TEXT("did not fire (started by hand)") : TEXT("fired"));
			Encounter->Start();
		}
	};
	auto Shot = [PC](const TCHAR* Name)
	{
		PC->ConsoleCommand(TEXT("shot"));
		UE_LOG(LogConquer, Log, TEXT("Test shot: %s"), Name);
	};
	auto GoToCheckpoint = [&](int32 Order)
	{
		for (TActorIterator<ALevelTriggerVolume> It(GetWorld()); It; ++It)
		{
			if (It->Kind == ELevelTrigger::Checkpoint && It->Order == Order)
			{
				Teleport(It->GetActorLocation());
			}
		}
	};
	// Kills what is standing in the fight; true once it's over.
	auto Clear = [&](const TCHAR* Name) -> bool
	{
		AEncounterVolume* Encounter = Find(Name);
		if (!Encounter || Encounter->GetState() == EEncounterState::Done || ++TestTries > 15)
		{
			UE_LOG(LogConquer, Log, TEXT("Test: %s %s"), Name, Encounter && Encounter->GetState() == EEncounterState::Done ? TEXT("cleared") : TEXT("NOT cleared"));
			TestTries = 0;
			return true;
		}
		for (AEnemySoldier* Enemy : Encounter->GetLivingEnemies())
		{
			Enemy->Kill(Wren);
		}
		return false;
	};

	float Next = 1.5f;
	switch (TestStep)
	{
	case 0:  Enter(TEXT("Lane")); Next = 4.f; break;
	case 1:  Shot(TEXT("lane")); Next = 0.5f; break;
	case 2:  if (!Clear(TEXT("Lane"))) { Next = 1.5f; GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, Next, false); return; } break;
	case 3:
		for (TActorIterator<AConquerPickup> It(GetWorld()); It; ++It)
		{
			Teleport(It->GetActorLocation());
			break;
		}
		break;
	case 4:  Enter(TEXT("Square")); Next = 5.f; break;
	case 5:  Shot(TEXT("square")); Next = 0.5f; break;
	case 6:  if (!Clear(TEXT("Square"))) { GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, 1.5f, false); return; } break;
	case 7:  GoToCheckpoint(1); Next = 1.f; break;
	case 8:  Enter(TEXT("Bridge")); Next = 4.f; break;
	case 9:
		// Die on the bridge: she should come back at the checkpoint with the bridge fight reset.
		Shot(TEXT("bridge"));
		Wren->Kill(nullptr);
		Next = RespawnDelay + 2.f;
		break;
	case 10:
		UE_LOG(LogConquer, Log, TEXT("Test: after the respawn the bridge fight is %s, Wren at %s"),
			Find(TEXT("Bridge")) && Find(TEXT("Bridge"))->GetState() == EEncounterState::Waiting ? TEXT("reset") : TEXT("NOT reset"),
			*Wren->GetActorLocation().ToCompactString());
		Enter(TEXT("Bridge"));
		Next = 2.f;
		break;
	case 11: if (!Clear(TEXT("Bridge"))) { GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, 1.5f, false); return; } break;
	case 12: GoToCheckpoint(2); Next = 1.f; break;
	case 13: Enter(TEXT("Gate")); Next = 4.f; break;
	case 14:
	{
		Shot(TEXT("gate"));
		// Wound her badly: the captain's next hits (or this one) should wake Rage.
		AEncounterVolume* Gate = Find(TEXT("Gate"));
		const TArray<AEnemySoldier*> Captain = Gate ? Gate->GetLivingEnemies() : TArray<AEnemySoldier*>();
		UAttributesComponent* Health = Wren->GetAttributes();
		Health->ApplyDamage(Health->Health - Health->MaxHealth * 0.3f, Captain.Num() > 0 ? Captain[0] : nullptr);
		Next = 1.5f;
		break;
	}
	case 15: Shot(TEXT("rage")); Next = 0.5f; break;
	case 16: if (!Clear(TEXT("Gate"))) { GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, 1.5f, false); return; } Next = 3.f; break;
	case 17: Shot(TEXT("sword")); Next = 1.f; break;
	case 18:
		for (TActorIterator<ALevelTriggerVolume> It(GetWorld()); It; ++It)
		{
			if (It->Kind == ELevelTrigger::LevelExit)
			{
				Teleport(It->GetActorLocation());
			}
		}
		UE_LOG(LogConquer, Log, TEXT("Test: Rage %s, sword %s"), bRageUnlocked ? TEXT("unlocked") : TEXT("LOCKED"), bHasSword ? TEXT("taken") : TEXT("NOT taken"));
		Next = 3.f;
		break;
	case 19:
		Shot(TEXT("end"));
		return;
	default:
		return;
	}
	++TestStep;
	GetWorldTimerManager().SetTimer(TestTimer, this, &AMarrowfordGameMode::RunTestStep, Next, false);
}
