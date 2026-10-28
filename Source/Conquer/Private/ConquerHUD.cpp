#include "ConquerHUD.h"
#include "WrenCharacter.h"
#include "EnemySoldier.h"
#include "GarrickBoss.h"
#include "CombatComponent.h"
#include "AttributesComponent.h"
#include "RageComponent.h"
#include "ConquerGameMode.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor GuardIdle(0.75f, 0.75f, 0.75f, 0.5f);
	const FLinearColor GuardActive(1.f, 1.f, 1.f, 1.f);
	const FLinearColor IncomingAttack(0.95f, 0.1f, 0.1f, 1.f);
	const FLinearColor IncomingUnblockable(1.f, 0.55f, 0.f, 1.f);
	const FLinearColor RageRed(0.8f, 0.05f, 0.05f, 1.f);

	FVector2D SideOffset(EGuardDirection Side, float Size)
	{
		switch (Side)
		{
		case EGuardDirection::Left:  return FVector2D(-Size, 0.f);
		case EGuardDirection::Right: return FVector2D(Size, 0.f);
		default:                     return FVector2D(0.f, -Size);
		}
	}
}

void AConquerHUD::DrawHUD()
{
	Super::DrawHUD();

	AWrenCharacter* Wren = Cast<AWrenCharacter>(GetOwningPawn());
	if (!Wren || !Canvas)
	{
		return;
	}

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// Cutscenes: bars top and bottom, nothing else of the HUD.
	const AConquerGameMode* GameMode = GetWorld()->GetAuthGameMode<AConquerGameMode>();
	if (GameMode && GameMode->bLetterbox)
	{
		DrawRect(FLinearColor::Black, 0.f, 0.f, W, H * 0.12f);
		DrawRect(FLinearColor::Black, 0.f, H * 0.88f, W, H * 0.12f);
		DrawOverlay(GameMode);
		return;
	}

	// Rage tints the whole screen red.
	if (Wren->GetRage()->IsRageActive())
	{
		DrawRect(FLinearColor(0.6f, 0.f, 0.f, 0.18f), 0.f, 0.f, W, H);
	}

	// Player bars, bottom left.
	const float BarX = 40.f;
	float BarY = H - 120.f;
	DrawBar(BarX, BarY, 320.f, 16.f, Wren->GetAttributes()->GetHealthFraction(), FLinearColor(0.75f, 0.12f, 0.1f), TEXT("Health"));
	BarY += 26.f;
	DrawBar(BarX, BarY, 260.f, 10.f, Wren->GetAttributes()->GetStaminaFraction(), FLinearColor(0.85f, 0.75f, 0.3f), TEXT("Stamina"));
	BarY += 20.f;
	const URageComponent* Rage = Wren->GetRage();
	if (Rage->bEnabled)
	{
		const bool bRageReady = Rage->GetRageFraction() >= 1.f && !Rage->IsRageActive();
		DrawBar(BarX, BarY, 260.f, 10.f, Rage->GetRageFraction(), RageRed, bRageReady ? TEXT("RAGE READY [R / Y]") : TEXT("Rage"));
	}

	// Boss health across the top of the screen.
	for (TActorIterator<AGarrickBoss> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), Wren->GetActorLocation()) < 4000.f)
		{
			const float BossW = W * 0.4f;
			DrawText(It->DisplayName, FLinearColor::White, (W - BossW) * 0.5f, 34.f, nullptr, 1.2f);
			DrawBar((W - BossW) * 0.5f, 56.f, BossW, 12.f, It->GetAttributes()->GetHealthFraction(), FLinearColor(0.6f, 0.08f, 0.06f), FString());
			break;
		}
	}

	// Enemy guard indicators.
	for (TActorIterator<AEnemySoldier> It(GetWorld()); It; ++It)
	{
		AEnemySoldier* Enemy = *It;
		if (Enemy->IsDead() || FVector::Dist(Enemy->GetActorLocation(), Wren->GetActorLocation()) > IndicatorRange)
		{
			continue;
		}

		const FVector Screen = Project(Enemy->GetActorLocation() + FVector(0.f, 0.f, 120.f), false);
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		const bool bTarget = Wren->GetLockTarget() == Enemy;
		const FVector2D Centre(Screen.X, Screen.Y);
		DrawGuardIndicator(Centre, Enemy->GetCombat(), bTarget ? 26.f : 16.f, bTarget);

		// Small health bar above the indicator; captains get a name.
		const float BarW = bTarget ? 90.f : 50.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), Centre.X - BarW * 0.5f, Centre.Y - 52.f, BarW, 5.f);
		DrawRect(FLinearColor(0.75f, 0.12f, 0.1f), Centre.X - BarW * 0.5f, Centre.Y - 52.f, BarW * Enemy->GetAttributes()->GetHealthFraction(), 5.f);
		if (Enemy->IsA<AEnemyCaptain>() || bTarget)
		{
			DrawText(Enemy->DisplayName, FLinearColor::White, Centre.X - BarW * 0.5f, Centre.Y - 70.f, nullptr, 1.f);
		}
		if (bTarget && Enemy->GetCombat()->IsStaggered() && Enemy->GetAttributes()->GetHealthFraction() <= Wren->ExecuteHealthFraction)
		{
			DrawText(TEXT("EXECUTE [Heavy]"), IncomingUnblockable, Centre.X - 45.f, Centre.Y + 34.f, nullptr, 1.f);
		}
	}

	// Wren's own guard, shown under the crosshair while locked on.
	if (Wren->IsLockedOn())
	{
		DrawGuardIndicator(FVector2D(W * 0.5f, H * 0.62f), Wren->GetCombat(), 22.f, true);
	}

	if (!GameMode || GameMode->bShowControlsHint)
	{
		DrawControlsHint();
	}
	DrawOverlay(GameMode);
}

void AConquerHUD::DrawOverlay(const AConquerGameMode* GameMode)
{
	if (!GameMode)
	{
		return;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Scale = FMath::Max(1.f, H / 720.f);

	if (GameMode->FadeAlpha > 0.f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, FMath::Clamp(GameMode->FadeAlpha, 0.f, 1.f)), 0.f, 0.f, W, H);
	}

	auto DrawCentred = [this, W](const FString& Text, float Y, float TextScale, const FLinearColor& Color)
	{
		float TextW = 0.f, TextH = 0.f;
		GetTextSize(Text, TextW, TextH, nullptr, TextScale);
		DrawText(Text, Color, (W - TextW) * 0.5f, Y - TextH * 0.5f, nullptr, TextScale);
	};

	if (const float Alpha = GameMode->GetBannerAlpha(); Alpha > 0.f)
	{
		DrawCentred(GameMode->GetBanner(), H * 0.3f, 2.6f * Scale, FLinearColor(0.95f, 0.88f, 0.78f, Alpha));
	}
	if (const float Alpha = GameMode->GetHintAlpha(); Alpha > 0.f)
	{
		DrawCentred(GameMode->GetHint(), H * 0.78f, 1.2f * Scale, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha));
	}
}

void AConquerHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FString& Label)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
	DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.85f), X + W + 10.f, Y - 3.f, nullptr, 0.9f);
}

void AConquerHUD::DrawGuardIndicator(const FVector2D& Centre, const UCombatComponent* Combat, float Size, bool bHighlight)
{
	const float Box = Size * 0.55f;
	const bool bAttacking = Combat->GetPhase() == EAttackPhase::Windup || Combat->GetPhase() == EAttackPhase::Active;

	for (EGuardDirection Side : { EGuardDirection::Left, EGuardDirection::Top, EGuardDirection::Right })
	{
		const FVector2D P = Centre + SideOffset(Side, Size);
		const bool bGuarding = Combat->GetGuardDirection() == Side || Combat->bBlockAllDirections;
		FLinearColor Color = bGuarding && Combat->bGuardRaised ? GuardActive : GuardIdle;
		float Scale = 1.f;

		if (bAttacking && Combat->GetAttackDirection() == Side)
		{
			Color = Combat->IsCurrentAttackUnblockable() ? IncomingUnblockable : IncomingAttack;
			Scale = 1.5f;
		}
		if (!bHighlight)
		{
			Color.A *= 0.7f;
		}
		const float S = Box * Scale;
		DrawRect(Color, P.X - S * 0.5f, P.Y - S * 0.5f, S, S);
	}

	if (Combat->IsStaggered())
	{
		DrawText(TEXT("OPEN"), FLinearColor::Yellow, Centre.X - 16.f, Centre.Y + Size + 6.f, nullptr, 0.9f);
	}
}

void AConquerHUD::DrawControlsHint()
{
	static const TCHAR* Lines[] = {
		TEXT("Move WASD / left stick    Camera mouse / right stick"),
		TEXT("Light LMB / RB    Heavy RMB / RT    Guard break E / X    Feint Q / B"),
		TEXT("Dodge Space / A    Lock on Tab / LT (then mouse or right stick picks guard side)"),
		TEXT("Rage R / Y when the bar is full. Heavy on a staggered, wounded enemy executes."),
	};
	float Y = 20.f;
	for (const TCHAR* Line : Lines)
	{
		DrawText(Line, FLinearColor(1.f, 1.f, 1.f, 0.6f), 20.f, Y, nullptr, 0.85f);
		Y += 16.f;
	}
}
