#include "CombatTestDriver.h"
#include "WrenCharacter.h"
#include "EnemySoldier.h"
#include "ArcherSoldier.h"
#include "ArrowProjectile.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "ConquerGameMode.h"
#include "Conquer.h"
#include "AIController.h"
#include "Camera/CameraActor.h"
#include "AttributesComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ACombatTestDriver::ACombatTestDriver()
{
	PrimaryActorTick.bCanEverTick = true;
	bMoveTest = FParse::Param(FCommandLine::Get(), TEXT("ConquerMoveTest"));
	bFightFilm = FParse::Param(FCommandLine::Get(), TEXT("ConquerFightFilm"));
}

void ACombatTestDriver::FilmFrame(float Every)
{
	if (MoveTime >= NextFrame)
	{
		NextFrame = MoveTime + Every;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("shot"));
			++Frames;
		}
	}
}

void ACombatTestDriver::TickFightFilm(float DeltaSeconds)
{
	const float Prev = MoveTime;
	MoveTime += DeltaSeconds;
	auto At = [&](float T) { return Prev < T && MoveTime >= T; };
	UCombatComponent* Combat = Wren->GetCombat();

	if (At(0.1f))
	{
		Dummy = Spawn(AEnemySoldier::StaticClass(), 260.f, true);
		Dummy->GetAttributes()->MaxHealth = 400.f;
		Dummy->GetAttributes()->Heal(400.f);
		Face(Wren, Dummy);
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector Mid = (Wren->GetActorLocation() + Dummy->GetActorLocation()) * 0.5f;
		const FVector Eye = Mid + FVector(-60.f, -480.f, 70.f);
		FollowCamera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Mid - Eye).Rotation());
		if (PC && FollowCamera)
		{
			PC->SetViewTarget(FollowCamera);
			if (PC->MyHUD)
			{
				PC->MyHUD->bShowHUD = false;
			}
		}
		UE_LOG(LogConquer, Log, TEXT("Fight film: starting"));
	}
	if (!Dummy)
	{
		return;
	}
	if (FollowCamera && !Wren->IsExecuting())
	{
		// Keep both in frame from the side.
		const FVector Mid = (Wren->GetActorLocation() + Dummy->GetActorLocation()) * 0.5f;
		const FVector Want = Mid + FVector(-60.f, -480.f, 70.f);
		FollowCamera->SetActorLocation(FMath::VInterpTo(FollowCamera->GetActorLocation(), Want, DeltaSeconds, 3.f));
		FollowCamera->SetActorRotation((Mid - FollowCamera->GetActorLocation()).Rotation());
	}

	// Whenever he swings, she parries it (guard 0.1 s before it lands), unless she is busy attacking.
	UCombatComponent* His = Dummy->GetCombat();
	if (!Dummy->IsDead() && His->GetPhase() == EAttackPhase::Windup && His->GetAttackSerial() != LastParriedSerial
		&& His->GetAttackKind() != EAttackKind::GuardBreak && MoveTime > 1.f)
	{
		const float Elapsed = GetWorld()->GetTimeSeconds() - His->GetAttackStartTime();
		if (Elapsed >= His->GetSpec(His->GetAttackKind()).Windup - 0.1f && !Combat->IsBusy())
		{
			LastParriedSerial = His->GetAttackSerial();
			Wren->SetGuardHeld(true);
			Wren->SetGuardHeld(false);
		}
	}

	// Her side of it.
	auto Light = [&]() { Face(Wren, Dummy); Combat->StartAttack(Combat->CanDodgeAttack() ? EAttackKind::DodgeAttack : EAttackKind::Light, EGuardDirection::Right); };
	if (At(1.2f) || At(1.65f) || At(2.1f)) { Light(); }
	if (At(3.4f)) { Face(Wren, Dummy); Combat->StartAttack(EAttackKind::Heavy, EGuardDirection::Top); }
	if (At(5.0f)) { Wren->TryDodge(); }
	if (At(5.25f)) { Light(); }
	if (At(6.8f)) { Face(Wren, Dummy); Combat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top); }
	if (At(7.3f))
	{
		// Wound him enough to finish, while the guard break has him reeling.
		Dummy->GetAttributes()->MinHealth = 0.f;
		Dummy->GetAttributes()->ApplyDamage(Dummy->GetAttributes()->Health - 30.f, Wren);
		Dummy->GetCombat()->Stagger(2.f);
		UE_LOG(LogConquer, Log, TEXT("Fight film: execution %s"), Wren->TryExecute() ? TEXT("started") : TEXT("NOT possible"));
	}
	if (MoveTime >= 0.5f && MoveTime < 11.f)
	{
		FilmFrame(0.05f);
	}
	if (At(11.1f))
	{
		UE_LOG(LogConquer, Log, TEXT("Fight film: done, %d frames, soldier %s"), Frames, Dummy->IsDead() ? TEXT("dead") : TEXT("alive"));
	}
}

void ACombatTestDriver::TickMoveTest(float DeltaSeconds)
{
	const float Prev = MoveTime;
	MoveTime += DeltaSeconds;
	auto At = [&](float T) { return Prev < T && MoveTime >= T; };
	auto During = [&](float From, float To) { return MoveTime >= From && MoveTime < To; };

	if (At(0.1f))
	{
		// A camera beside her, following along her path (which runs along +X), no HUD.
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		FollowCamera = GetWorld()->SpawnActor<ACameraActor>(Wren->GetActorLocation() + FVector(0.f, -520.f, 60.f), FRotator(-4.f, 90.f, 0.f));
		if (PC && FollowCamera)
		{
			PC->SetViewTarget(FollowCamera);
			if (PC->MyHUD)
			{
				PC->MyHUD->bShowHUD = false;
			}
		}
		UE_LOG(LogConquer, Log, TEXT("Move test: starting"));
	}

	if (FollowCamera)
	{
		const FVector Want = Wren->GetActorLocation() + FVector(0.f, -520.f, 60.f);
		FollowCamera->SetActorLocation(FMath::VInterpTo(FollowCamera->GetActorLocation(), Want, DeltaSeconds, 4.f));
	}

	// Idle, set off and run, let go (a stop), a diagonal run, let go, then a sharp turn on the spot, then another.
	if (During(1.0f, 2.6f)) { Wren->AddMovementInput(FVector(1.f, 0.f, 0.f), 1.f); }
	// The diagonal is strafed, as locked on: she keeps facing ahead and the legs go forward-right.
	if (At(3.9f)) { Wren->GetCharacterMovement()->bOrientRotationToMovement = false; }
	if (During(4.0f, 5.2f)) { Wren->AddMovementInput(FVector(0.7f, 0.7f, 0.f), 1.f); }
	if (At(6.2f)) { Wren->GetCharacterMovement()->bOrientRotationToMovement = true; }
	if (At(6.6f)) { Wren->SetActorRotation(Wren->GetActorRotation() + FRotator(0.f, 110.f, 0.f)); UE_LOG(LogConquer, Log, TEXT("Move test: turn")); }
	if (At(8.4f)) { Wren->SetActorRotation(Wren->GetActorRotation() + FRotator(0.f, -170.f, 0.f)); UE_LOG(LogConquer, Log, TEXT("Move test: turn back")); }

	if (MoveTime >= 0.6f && MoveTime < 10.5f && MoveTime >= NextFrame)
	{
		NextFrame = MoveTime + 0.1f;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("shot"));
			++Frames;
		}
	}
	if (At(10.6f))
	{
		UE_LOG(LogConquer, Log, TEXT("Move test: done, %d frames"), Frames);
	}
}

AEnemySoldier* ACombatTestDriver::Spawn(TSubclassOf<AEnemySoldier> Class, float Distance, bool bKeepAI)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Where = Wren->GetActorLocation() + Wren->GetActorForwardVector() * Distance;
	AEnemySoldier* Enemy = GetWorld()->SpawnActor<AEnemySoldier>(Class, Where, FRotator::ZeroRotator, Params);
	if (!Enemy)
	{
		return nullptr;
	}
	if (!bKeepAI)
	{
		// A puppet: the test moves it.
		if (AController* Brain = Enemy->GetController())
		{
			Brain->UnPossess();
			Brain->Destroy();
		}
	}
	Face(Enemy, Wren);
	return Enemy;
}

void ACombatTestDriver::Face(AActor* Actor, const AActor* Target) const
{
	const FVector To = Target->GetActorLocation() - Actor->GetActorLocation();
	Actor->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
}

void ACombatTestDriver::Reset()
{
	if (Dummy)
	{
		if (!Dummy->IsDead())
		{
			Dummy->Destroy();
		}
		Dummy = nullptr;
	}
	for (TActorIterator<AArrowProjectile> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	Wren->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
	Wren->SetActorRotation(FRotator::ZeroRotator);
	Wren->SetGuardHeld(false);
	UAttributesComponent* Attributes = Wren->GetAttributes();
	Attributes->Heal(1000.f);
	Attributes->Stamina = Attributes->MaxStamina;
	Wren->GetCombat()->DodgeStrength = 1400.f;
}

void ACombatTestDriver::Next()
{
	++Step;
	StepTime = -0.001f;
}

void ACombatTestDriver::Check(const TCHAR* Name, bool bPass, const FString& Detail)
{
	(bPass ? Passed : Failed)++;
	UE_LOG(LogConquer, Log, TEXT("Combat test: %s %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
}

void ACombatTestDriver::Shot(const TCHAR* Name)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->ConsoleCommand(TEXT("shot"));
		UE_LOG(LogConquer, Log, TEXT("Test shot: %s"), Name);
	}
}

EHitOutcome ACombatTestDriver::LastOutcome() const
{
	const AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	return GameMode ? GameMode->LastPlayerOutcome : EHitOutcome::Ignored;
}

bool ACombatTestDriver::NewOutcomeSince(int32 Count) const
{
	const AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	return GameMode && GameMode->PlayerOutcomes > Count;
}

void ACombatTestDriver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Wren)
	{
		Wren = Cast<AWrenCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (!Wren)
		{
			return;
		}
		Start = Wren->GetActorLocation();
		Wren->GetAttributes()->MinHealth = 1.f;
		Reset();
	}
	if (bMoveTest)
	{
		TickMoveTest(DeltaSeconds);
		return;
	}
	if (bFightFilm)
	{
		TickFightFilm(DeltaSeconds);
		return;
	}

	const float Prev = StepTime;
	StepTime += DeltaSeconds;
	auto At = [&](float T) { return Prev < T && StepTime >= T; };
	const AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	const int32 Outcomes = GameMode ? GameMode->PlayerOutcomes : 0;
	UCombatComponent* Combat = Wren->GetCombat();
	auto OutcomeName = [this]() { return UEnum::GetDisplayValueAsText(LastOutcome()).ToString(); };

	switch (Step)
	{
	case 0:
		// Let the arena settle, and line up a shield guard and an archer for a look at them.
		if (At(0.2f))
		{
			Dummy = Spawn(AShieldSoldier::StaticClass(), 320.f);
			Dummy->AddActorWorldOffset(Wren->GetActorRightVector() * -110.f);
			Lineup = Spawn(AArcherSoldier::StaticClass(), 320.f);
			Lineup->AddActorWorldOffset(Wren->GetActorRightVector() * 110.f);
		}
		if (At(1.3f)) { Shot(TEXT("lineup")); }
		if (At(1.8f))
		{
			Lineup->Destroy();
			Next();
		}
		break;

	case 1:
		// Perfect parry: guard pressed 0.1 s before a light lands, out of lock-on (her guard turns to the blow).
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 140.f);
			Face(Wren, Dummy);
			Before = Wren->GetRage()->Rage;
			OutcomesBefore = Outcomes;
			Dummy->GetCombat()->StartAttack(EAttackKind::Light, EGuardDirection::Left);
		}
		if (At(0.35f)) { Wren->SetGuardHeld(true); }
		if (At(0.40f)) { Wren->SetGuardHeld(false); }
		if (At(0.52f)) { Shot(TEXT("parry")); }
		if (At(1.0f))
		{
			Check(TEXT("perfect parry"), NewOutcomeSince(OutcomesBefore) && LastOutcome() == EHitOutcome::Parried && Dummy->GetCombat()->IsStaggered()
				&& Wren->GetRage()->Rage > Before, FString::Printf(TEXT("(%s, Rage %.0f -> %.0f)"), *OutcomeName(), Before, Wren->GetRage()->Rage));
			Next();
		}
		break;

	case 2:
		// Late block: guard held from the start of the swing. Blocks, costs stamina, no parry.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 140.f);
			Face(Wren, Dummy);
			OutcomesBefore = Outcomes;
			Dummy->GetCombat()->StartAttack(EAttackKind::Light, EGuardDirection::Right);
			Wren->SetGuardHeld(true);
			Before = Wren->GetAttributes()->Stamina;
		}
		if (At(1.0f))
		{
			Check(TEXT("late block"), NewOutcomeSince(OutcomesBefore) && LastOutcome() == EHitOutcome::Blocked && Wren->GetAttributes()->Stamina < Before
				&& !Dummy->GetCombat()->IsStaggered(), FString::Printf(TEXT("(%s, stamina %.0f -> %.0f)"), *OutcomeName(), Before, Wren->GetAttributes()->Stamina));
			Wren->SetGuardHeld(false);
			Next();
		}
		break;

	case 3:
		// Her guard break opens a guarding soldier, and a light lands while he reels.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 140.f);
			Face(Wren, Dummy);
			OutcomesBefore = Outcomes;
			Combat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top);
		}
		if (At(0.3f)) { Shot(TEXT("guard break")); }
		if (At(0.6f))
		{
			Check(TEXT("guard break"), LastOutcome() == EHitOutcome::GuardBroken && Dummy->GetCombat()->IsStaggered(), FString::Printf(TEXT("(%s)"), *OutcomeName()));
			OutcomesBefore = Outcomes;
			Combat->StartAttack(EAttackKind::Light, EGuardDirection::Left);
		}
		if (At(1.2f))
		{
			Check(TEXT("punish after guard break"), NewOutcomeSince(OutcomesBefore) && LastOutcome() == EHitOutcome::Hit, FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Next();
		}
		break;

	case 4:
		// His guard break, countered by hers started 0.2 s into his.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 140.f);
			Face(Wren, Dummy);
			OutcomesBefore = Outcomes;
			Dummy->GetCombat()->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top);
		}
		if (At(0.2f)) { Combat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top); }
		if (At(1.0f))
		{
			Check(TEXT("guard break counter"), LastOutcome() == EHitOutcome::Countered && Dummy->GetCombat()->IsStaggered() && !Combat->IsStaggered(),
				FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Next();
		}
		break;

	case 5:
		// Dodge cancels: a light's recovery yes, a heavy's early windup yes, a heavy past its commit point no.
		if (At(0.f)) { Reset(); Combat->StartAttack(EAttackKind::Light, EGuardDirection::Top); }
		if (At(0.45f)) { Check(TEXT("dodge cancels a light's recovery"), Combat->Dodge(FVector::ZeroVector)); }
		if (At(1.2f)) { Wren->GetAttributes()->Stamina = 100.f; Combat->StartAttack(EAttackKind::Heavy, EGuardDirection::Top); }
		if (At(1.3f)) { Check(TEXT("dodge cancels a heavy's startup"), Combat->Dodge(FVector::ZeroVector)); }
		if (At(2.2f)) { Wren->GetAttributes()->Stamina = 100.f; Combat->StartAttack(EAttackKind::Heavy, EGuardDirection::Top); }
		if (At(2.7f)) { Check(TEXT("no dodge once a heavy commits"), !Combat->Dodge(FVector::ZeroVector)); }
		if (At(3.6f)) { Next(); }
		break;

	case 6:
		// I-frames: a dodge (on the spot, for the test) as the blow lands; then a dodge attack out of it.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 140.f);
			Face(Wren, Dummy);
			Combat->DodgeStrength = 0.f;
			Before = Wren->GetAttributes()->Health;
			OutcomesBefore = Outcomes;
			Dummy->GetCombat()->StartAttack(EAttackKind::Light, EGuardDirection::Top);
		}
		if (At(0.35f)) { Combat->Dodge(FVector::ZeroVector); }
		if (At(0.5f))
		{
			Check(TEXT("dodge i-frames"), LastOutcome() == EHitOutcome::Dodged && Wren->GetAttributes()->Health >= Before, FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Check(TEXT("dodge attack"), Combat->CanDodgeAttack() && Combat->StartAttack(EAttackKind::DodgeAttack, EGuardDirection::Top));
		}
		if (At(1.4f)) { Next(); }
		break;

	case 7:
		// Shield guard: blocks a light from the front, not from behind; a guard break opens him from the front.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AShieldSoldier::StaticClass(), 150.f);
			Face(Wren, Dummy);
			OutcomesBefore = Outcomes;
			Combat->StartAttack(EAttackKind::Light, EGuardDirection::Left);
		}
		if (At(0.3f)) { Shot(TEXT("shield")); }
		if (At(0.7f))
		{
			Check(TEXT("shield blocks from the front"), LastOutcome() == EHitOutcome::Blocked, FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Wren->SetActorLocation(Dummy->GetActorLocation() - Dummy->GetActorForwardVector() * 140.f, false, nullptr, ETeleportType::TeleportPhysics);
			Face(Wren, Dummy);
			OutcomesBefore = Outcomes;
			Combat->StartAttack(EAttackKind::Light, EGuardDirection::Left);
		}
		if (At(1.4f))
		{
			Check(TEXT("shield flanked from behind"), NewOutcomeSince(OutcomesBefore) && LastOutcome() == EHitOutcome::Hit, FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Wren->SetActorLocation(Dummy->GetActorLocation() + Dummy->GetActorForwardVector() * 140.f, false, nullptr, ETeleportType::TeleportPhysics);
			Face(Wren, Dummy);
			Wren->GetAttributes()->Stamina = 100.f;
			Combat->StartAttack(EAttackKind::GuardBreak, EGuardDirection::Top);
		}
		if (At(2.0f))
		{
			Check(TEXT("shield guard broken"), LastOutcome() == EHitOutcome::GuardBroken && !Dummy->GetCombat()->IsShieldUp(), FString::Printf(TEXT("(%s)"), *OutcomeName()));
			OutcomesBefore = Outcomes;
			Combat->StartAttack(EAttackKind::Light, EGuardDirection::Left);
		}
		if (At(2.6f))
		{
			Check(TEXT("shield punished while open"), NewOutcomeSince(OutcomesBefore) && LastOutcome() == EHitOutcome::Hit, FString::Printf(TEXT("(%s)"), *OutcomeName()));
			Next();
		}
		break;

	case 8:
	{
		// An archer with his own AI: deflect the first arrow, dodge the second, take the third.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AArcherSoldier::StaticClass(), 1500.f, true);
			Face(Wren, Dummy);
			Combat->DodgeStrength = 0.f;
			ArrowPhase = 0;
			OutcomesBefore = Outcomes;
		}
		AArrowProjectile* Incoming = nullptr;
		for (TActorIterator<AArrowProjectile> It(GetWorld()); It; ++It)
		{
			Incoming = *It;
		}
		if (Incoming && ArrowPhase % 2 == 0)
		{
			const float Dist = FVector::Dist(Incoming->GetActorLocation(), Wren->GetActorLocation());
			const float Speed = Incoming->GetVelocity().Size();
			if (ArrowPhase == 0 && Dist < Speed * 0.08f)
			{
				Shot(TEXT("arrow"));
				Wren->SetGuardHeld(true);
				Wren->SetGuardHeld(false);
				ArrowPhase = 1;
			}
			else if (ArrowPhase == 2 && Dist < Speed * 0.1f)
			{
				Wren->GetAttributes()->Stamina = 100.f;
				Combat->Dodge(FVector::ZeroVector);
				ArrowPhase = 3;
			}
			else if (ArrowPhase == 4)
			{
				Before = Wren->GetAttributes()->Health;
				ArrowPhase = 5;
			}
		}
		if (NewOutcomeSince(OutcomesBefore))
		{
			OutcomesBefore = Outcomes;
			const EHitOutcome Outcome = LastOutcome();
			if (ArrowPhase == 1) { Check(TEXT("arrow deflected"), Outcome == EHitOutcome::Parried, FString::Printf(TEXT("(%s)"), *OutcomeName())); ArrowPhase = 2; }
			else if (ArrowPhase == 3) { Check(TEXT("arrow dodged"), Outcome == EHitOutcome::Dodged, FString::Printf(TEXT("(%s)"), *OutcomeName())); ArrowPhase = 4; }
			else if (ArrowPhase == 5)
			{
				Check(TEXT("arrow hits"), Outcome == EHitOutcome::Hit && Wren->GetAttributes()->Health < Before, FString::Printf(TEXT("(%s)"), *OutcomeName()));
				ArrowPhase = 6;
			}
		}
		if (ArrowPhase == 6 || StepTime > 30.f)
		{
			if (ArrowPhase != 6)
			{
				Check(TEXT("archer finished in time"), false, FString::Printf(TEXT("(stuck at phase %d)"), ArrowPhase));
			}
			Next();
		}
		break;
	}

	case 9:
		// An execution: a staggered, wounded soldier finished with a heavy, and her control back after.
		if (At(0.f))
		{
			Reset();
			Dummy = Spawn(AEnemySoldier::StaticClass(), 150.f);
			Face(Wren, Dummy);
			Dummy->GetAttributes()->ApplyDamage(Dummy->GetAttributes()->MaxHealth * 0.8f, Wren);
			Dummy->GetCombat()->Stagger(3.f);
			Check(TEXT("execution starts"), Wren->TryExecute() && Wren->IsExecuting());
		}
		if (At(0.6f)) { Shot(TEXT("execution")); }
		if (At(3.5f))
		{
			Check(TEXT("execution kills and ends"), Dummy->IsDead() && !Wren->IsExecuting(), FString::Printf(TEXT("(dead %d, executing %d)"), Dummy->IsDead(), Wren->IsExecuting()));
			Next();
		}
		break;

	case 10:
		UE_LOG(LogConquer, Log, TEXT("Combat test: done, %d passed, %d failed"), Passed, Failed);
		Reset();
		Next();
		break;

	default:
		break;
	}
}
