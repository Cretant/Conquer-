#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConquerPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EPickupKind : uint8
{
	/** Heals Amount health. */
	Health,
	/** Sharpens her weapon: every attack does Amount times the damage, for the rest of the level. */
	Whetstone
};

/** Loot lying in the level, picked up by walking over it. A soft glow and a slow spin make it easy to spot. */
UCLASS()
class CONQUER_API AConquerPickup : public AActor
{
	GENERATED_BODY()

public:
	AConquerPickup();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	EPickupKind Kind = EPickupKind::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float Amount = 50.f;

	/** Shown as a hint when she picks it up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FString Label;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Glow;
};
