#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "WeaponTrailComponent.generated.h"

class UStaticMeshComponent;
class UCombatComponent;

/**
 * A ribbon traced by the blade through a swing: from just before the contact frame to the end of the strike, the
 * weapon's base and tip are sampled each frame and joined into a strip that fades out behind them. Built in world
 * space; the mesh's own transform stays at the origin.
 */
UCLASS(ClassGroup = (Conquer))
class CONQUER_API UWeaponTrailComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWeaponTrailComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Setup(UStaticMeshComponent* InWeapon, UCombatComponent* InCombat, UMaterialInterface* Material);

	/** How long a sample lingers before it has faded out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float FadeSeconds = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	FLinearColor Color = FLinearColor(1.f, 0.85f, 0.6f, 0.8f);

private:
	struct FSample
	{
		FVector Base;
		FVector Tip;
		float Time;
	};

	void Rebuild();

	TWeakObjectPtr<UStaticMeshComponent> Weapon;
	TWeakObjectPtr<UCombatComponent> Combat;
	TArray<FSample> Samples;
};
