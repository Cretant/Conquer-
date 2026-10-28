#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BurningPatch.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/** A patch of burning oil on the floor: hurts the player while she stands in it. */
UCLASS(Config = Game)
class CONQUER_API ABurningPatch : public AActor
{
	GENERATED_BODY()

public:
	ABurningPatch();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire")
	float Radius = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire")
	float DamagePerSecond = 10.f;

	/** Glowing material for the patch, set in DefaultGame.ini. */
	UPROPERTY(Config)
	FSoftObjectPath MaterialPath;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Fire")
	TObjectPtr<UStaticMeshComponent> Flames;

	UPROPERTY(VisibleAnywhere, Category = "Fire")
	TObjectPtr<UPointLightComponent> Glow;

	float DamageTimer = 0.f;
	float Age = 0.f;
};
