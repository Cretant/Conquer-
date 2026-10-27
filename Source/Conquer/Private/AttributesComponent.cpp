#include "AttributesComponent.h"

UAttributesComponent::UAttributesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAttributesComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Stamina = MaxStamina;
}

void UAttributesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Stamina < MaxStamina && Now - LastStaminaSpendTime >= StaminaRegenDelay)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * DeltaTime);
	}
}

bool UAttributesComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
	if (IsDead() || Amount <= 0.f)
	{
		return false;
	}

	Health = FMath::Max(0.f, Health - Amount);
	OnDamaged.Broadcast(Amount, DamageCauser);

	if (IsDead())
	{
		OnDeath.Broadcast(DamageCauser);
		return true;
	}
	return false;
}

void UAttributesComponent::Heal(float Amount)
{
	if (!IsDead())
	{
		Health = FMath::Min(MaxHealth, Health + Amount);
	}
}

bool UAttributesComponent::TrySpendStamina(float Amount)
{
	if (Stamina < Amount)
	{
		return false;
	}
	DrainStamina(Amount);
	return true;
}

void UAttributesComponent::DrainStamina(float Amount)
{
	Stamina = FMath::Max(0.f, Stamina - Amount);
	LastStaminaSpendTime = GetWorld()->GetTimeSeconds();
}
