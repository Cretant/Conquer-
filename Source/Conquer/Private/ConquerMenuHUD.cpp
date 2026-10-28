#include "ConquerMenuHUD.h"
#include "Conquer.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

AConquerMenuHUD::AConquerMenuHUD()
{
	TitleScreen = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/T_TitleScreen.T_TitleScreen")));
}

void AConquerMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	TitleTexture = TitleScreen.LoadSynchronous();
	if (!TitleTexture)
	{
		UE_LOG(LogConquer, Warning, TEXT("Menu: title screen texture %s not found"), *TitleScreen.ToString());
	}
}

void AConquerMenuHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor::Black, 0.f, 0.f, W, H);

	// 0 at the bright end of the pulse, 1 at the dim end.
	const float Dim = 0.5f - 0.5f * FMath::Cos(GetWorld()->GetRealTimeSeconds() * UE_TWO_PI / FMath::Max(PulsePeriod, 0.1f));

	if (TitleTexture && TitleTexture->GetSurfaceHeight() > 0.f && H > 0.f)
	{
		// Aspect-fill: crop the texture's long side so the art covers the screen without stretching.
		const float ScreenAspect = W / H;
		const float ImageAspect = TitleTexture->GetSurfaceWidth() / TitleTexture->GetSurfaceHeight();
		float U = 0.f, V = 0.f, UW = 1.f, VH = 1.f;
		if (ScreenAspect > ImageAspect)
		{
			VH = ImageAspect / ScreenAspect;
			V = (1.f - VH) * 0.5f;
		}
		else
		{
			UW = ScreenAspect / ImageAspect;
			U = (1.f - UW) * 0.5f;
		}
		DrawTexture(TitleTexture, 0.f, 0.f, W, H, U, V, UW, VH, FLinearColor::White, BLEND_Opaque);

		if (PromptText.IsEmpty())
		{
			// The art behind the painted prompt is near black, so a dark patch reads as the text fading.
			const float X0 = (PaintedPromptUV.X - U) / UW * W;
			const float Y0 = (PaintedPromptUV.Y - V) / VH * H;
			const float X1 = (PaintedPromptUV.Z - U) / UW * W;
			const float Y1 = (PaintedPromptUV.W - V) / VH * H;
			DrawRect(FLinearColor(0.012f, 0.01f, 0.01f, 0.8f * Dim), X0, Y0, X1 - X0, Y1 - Y0);
		}
	}

	if (!OptionsText.IsEmpty())
	{
		const float Scale = FMath::Max(1.f, H / 720.f) * 1.1f;
		float TextW = 0.f, TextH = 0.f;
		GetTextSize(OptionsText, TextW, TextH, nullptr, Scale);
		DrawText(OptionsText, FLinearColor(0.92f, 0.86f, 0.78f, 0.85f), (W - TextW) * 0.5f, H * 0.93f - TextH * 0.5f, nullptr, Scale);
	}

	if (!PromptText.IsEmpty())
	{
		const float Scale = FMath::Max(1.f, H / 720.f) * 1.3f;
		float TextW = 0.f, TextH = 0.f;
		GetTextSize(PromptText, TextW, TextH, nullptr, Scale);
		DrawText(PromptText, FLinearColor(0.92f, 0.86f, 0.78f, 1.f - 0.8f * Dim), (W - TextW) * 0.5f, H * 0.88f - TextH * 0.5f, nullptr, Scale);
	}
}
