#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RageComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRageStateChanged, bool, bActive);

/**
 * Wren's signature system. Fighting fills the meter; a full meter can be unleashed for a burst of speed,
 * damage and unblockable heavies. Near death it fills by itself once per life (the last stand).
 */
UCLASS(ClassGroup = (Conquer), meta = (BlueprintSpawnableComponent))
class CONQUER_API URageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URageComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Rage")
	void AddRage(float Amount);

	/** Unleashes Rage if the meter is full. Returns true if it started. */
	UFUNCTION(BlueprintCallable, Category = "Rage")
	bool TryActivate();

	/** Called whenever the owner takes damage; triggers the last stand at low health. */
	void NotifyHealthChanged(float HealthFraction);

	UFUNCTION(BlueprintPure, Category = "Rage")
	bool IsRageActive() const { return bRageActive; }

	UFUNCTION(BlueprintPure, Category = "Rage")
	float GetRageFraction() const { return MaxRage > 0.f ? Rage / MaxRage : 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Rage")
	FOnRageStateChanged OnRageStateChanged;

	/** Only the player uses Rage. Enemies keep the component switched off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	float MaxRage = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rage")
	float Rage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	float Duration = 10.f;

	/** Meter lost per second after this long without fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	float DrainPerSecond = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	float DrainDelay = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage")
	float LastStandHealthFraction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage|Effects")
	float SpeedMultiplier = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rage|Effects")
	float DamageMultiplier = 1.5f;

private:
	void EndRage();

	bool bRageActive = false;
	bool bLastStandUsed = false;
	float RageTimeLeft = 0.f;
	float LastGainTime = -100.f;
};
