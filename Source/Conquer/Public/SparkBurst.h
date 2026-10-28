#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SparkBurst.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

/** Steel on steel: a spray of glowing sparks and a flash of light, gone in a third of a second. */
UCLASS(NotPlaceable)
class CONQUER_API ASparkBurst : public AActor
{
	GENERATED_BODY()

public:
	ASparkBurst();

	virtual void Tick(float DeltaSeconds) override;

	/** Throws the sparks out from here, mostly along Direction; Strength scales how many and how fast. */
	void Burst(const FVector& Direction, float Strength, UMaterialInterface* Material, const FLinearColor& LightColor);

private:
	struct FSpark
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity;
	};

	UPROPERTY(VisibleAnywhere, Category = "Sparks")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Sparks")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	TArray<FVector> Velocities;
	float Age = 0.f;
	float Life = 0.35f;
	float FlashIntensity = 0.f;
};
