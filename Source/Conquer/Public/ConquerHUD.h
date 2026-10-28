#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatTypes.h"
#include "ConquerHUD.generated.h"

class AConquerCharacterBase;
class AConquerGameMode;
class UCombatComponent;

/**
 * Placeholder HUD drawn on the canvas: health, stamina and Rage bars, plus the For Honor style
 * guard indicator over each nearby enemy (their guard, and a red or orange flash for incoming attacks).
 */
UCLASS()
class CONQUER_API AConquerHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float IndicatorRange = 1800.f;

private:
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FString& Label);
	void DrawGuardIndicator(const FVector2D& Centre, const UCombatComponent* Combat, float Size, bool bHighlight);
	void DrawControlsHint();
	void DrawOverlay(const AConquerGameMode* GameMode);
};
