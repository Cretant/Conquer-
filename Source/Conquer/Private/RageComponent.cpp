#include "RageComponent.h"
#include "Conquer.h"

URageComponent::URageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled)
	{
		return;
	}

	if (bRageActive)
	{
		RageTimeLeft -= DeltaTime;
		Rage = MaxRage * FMath::Max(0.f, RageTimeLeft / Duration);
		if (RageTimeLeft <= 0.f)
		{
			EndRage();
		}
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastGainTime > DrainDelay)
	{
		Rage = FMath::Max(0.f, Rage - DrainPerSecond * DeltaTime);
	}
}

void URageComponent::AddRage(float Amount)
{
	if (!bEnabled || bRageActive || Amount <= 0.f)
	{
		return;
	}
	Rage = FMath::Min(MaxRage, Rage + Amount);
	LastGainTime = GetWorld()->GetTimeSeconds();
}

bool URageComponent::TryActivate()
{
	if (!bEnabled || bRageActive || Rage < MaxRage)
	{
		return false;
	}

	bRageActive = true;
	RageTimeLeft = Duration;
	UE_LOG(LogConquer, Log, TEXT("%s unleashes Rage"), *GetOwner()->GetName());
	OnRageStateChanged.Broadcast(true);
	return true;
}

void URageComponent::NotifyHealthChanged(float HealthFraction)
{
	if (!bEnabled || bLastStandUsed || bRageActive || HealthFraction <= 0.f || HealthFraction > LastStandHealthFraction)
	{
		return;
	}

	bLastStandUsed = true;
	Rage = MaxRage;
	TryActivate();
}

void URageComponent::EndRage()
{
	bRageActive = false;
	Rage = 0.f;
	OnRageStateChanged.Broadcast(false);
}
