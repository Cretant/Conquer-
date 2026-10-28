#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "ArrowProjectile.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class AConquerCharacterBase;

/**
 * A burning arrow flying straight at Wren. It resolves through her combat component like any blow, from the side
 * it arrives on: a timed guard deflects it, a guard blocks it, a dodge's i-frames let it pass, otherwise it hits.
 * Walls and the ground stop it.
 */
UCLASS()
class CONQUER_API AArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	AArrowProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override { return Velocity; }

	void Launch(AConquerCharacterBase* InShooter, const FAttackSpec& InSpec, const FVector& InVelocity);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrow")
	float LifeSeconds = 3.f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<UPointLightComponent> Flame;

	TWeakObjectPtr<AConquerCharacterBase> Shooter;
	FAttackSpec Spec;
	FVector Velocity = FVector::ZeroVector;
	FVector Origin = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>> DodgedBy;
};
