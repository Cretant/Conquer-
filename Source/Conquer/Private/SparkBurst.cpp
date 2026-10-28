#include "SparkBurst.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASparkBurst::ASparkBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Root);
	Flash->SetAttenuationRadius(500.f);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0.f);
}

void ASparkBurst::Burst(const FVector& Direction, float Strength, UMaterialInterface* Material, const FLinearColor& LightColor)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	const int32 Count = FMath::RoundToInt(10.f * Strength);
	const FVector Along = Direction.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this);
		Spark->SetStaticMesh(Cube);
		if (Material)
		{
			Spark->SetMaterial(0, Material);
		}
		Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spark->SetCastShadow(false);
		Spark->SetupAttachment(Root);
		Spark->SetWorldScale3D(FVector(0.05f, 0.012f, 0.012f));
		Spark->RegisterComponent();
		const FVector Spray = (Along + FMath::VRand() * 0.9f + FVector(0.f, 0.f, 0.4f)).GetSafeNormal();
		Spark->SetWorldRotation(Spray.Rotation());
		Meshes.Add(Spark);
		Velocities.Add(Spray * FMath::FRandRange(350.f, 900.f) * FMath::Sqrt(Strength));
	}
	Flash->SetLightColor(LightColor);
	FlashIntensity = 4000.f * Strength;
	Flash->SetIntensity(FlashIntensity);
}

void ASparkBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Left = 1.f - Age / Life;
	if (Left <= 0.f)
	{
		Destroy();
		return;
	}
	for (int32 i = 0; i < Meshes.Num(); ++i)
	{
		Velocities[i].Z -= 980.f * DeltaSeconds;
		Meshes[i]->AddWorldOffset(Velocities[i] * DeltaSeconds);
		Meshes[i]->SetWorldRotation(Velocities[i].Rotation());
		Meshes[i]->SetWorldScale3D(FVector(0.05f * Left, 0.012f, 0.012f));
	}
	Flash->SetIntensity(FlashIntensity * Left * Left);
}
