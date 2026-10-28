#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ConquerMenuHUD.generated.h"

class UTexture2D;

/**
 * Title screen drawn on the canvas: the key art fills the screen (cropped, never stretched).
 * The art already has "Press any button" painted in, so by default the HUD pulses that line by fading
 * a dark patch over it. Set PromptText to draw a separate line near the bottom instead.
 */
UCLASS()
class CONQUER_API AConquerMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	AConquerMenuHUD();

	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "Menu")
	TSoftObjectPtr<UTexture2D> TitleScreen;

	/** Where the painted prompt sits in the art, in texture UVs (min X, min Y, max X, max Y). */
	UPROPERTY(EditAnywhere, Category = "Menu")
	FVector4 PaintedPromptUV = FVector4(0.15f, 0.515f, 0.34f, 0.575f);

	/** Drawn near the bottom when set; leave empty to pulse the prompt painted into the art. */
	UPROPERTY(EditAnywhere, Category = "Menu")
	FString PromptText;

	/** The fights on offer, drawn near the bottom of the screen. */
	UPROPERTY(EditAnywhere, Category = "Menu")
	FString OptionsText = TEXT("1   The Arena          2   The Keep Hall: Sir Garrick Vane");

	/** Seconds for one full fade out and back in. */
	UPROPERTY(EditAnywhere, Category = "Menu")
	float PulsePeriod = 2.4f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TitleTexture;
};
