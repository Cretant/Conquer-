#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributesComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamagedSignature, float, Amount, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownedSignature, AActor*, DamageCauser);

/** Health and stamina. Stamina regenerates after a short delay once the character stops spending it. */
UCLASS(ClassGroup = (Conquer), meta = (BlueprintSpawnableComponent))
class CONQUER_API UAttributesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributesComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns true if this damage killed the owner. */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	bool ApplyDamage(float Amount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void Heal(float Amount);

	/** Spends stamina if there is enough. Returns false and spends nothing otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	bool TrySpendStamina(float Amount);

	/** Drains stamina even below the cost, used for blocked hits. */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void DrainStamina(float Amount);

	UFUNCTION(BlueprintPure, Category = "Attributes")
	bool IsDead() const { return Health <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetStaminaFraction() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnDamagedSignature OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnDeathSignature OnDeath;

	/** Fires once when damage would have taken health below MinHealth. */
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnDownedSignature OnDowned;

	/** Damage can't take health below this. Above zero, the owner goes down instead of dying (bosses wait for an execution). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MinHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attributes")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxStamina = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attributes")
	float Stamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float StaminaRegenPerSecond = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float StaminaRegenDelay = 0.8f;

protected:
	virtual void BeginPlay() override;

private:
	float LastStaminaSpendTime = -100.f;
};
