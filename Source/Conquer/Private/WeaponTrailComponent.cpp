#include "WeaponTrailComponent.h"
#include "CombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UWeaponTrailComponent::UWeaponTrailComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	SetCanEverAffectNavigation(false);
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UWeaponTrailComponent::Setup(UStaticMeshComponent* InWeapon, UCombatComponent* InCombat, UMaterialInterface* Material)
{
	Weapon = InWeapon;
	Combat = InCombat;
	if (Material)
	{
		SetMaterial(0, Material);
	}
	SetWorldTransform(FTransform::Identity);
}

void UWeaponTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UStaticMeshComponent* Blade = Weapon.Get();
	const UCombatComponent* Fighter = Combat.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Blade && Fighter && Blade->GetStaticMesh() && Fighter->IsInStrikeWindow() && Blade->IsVisible())
	{
		// The weapons lie along their own +X from the grip: the trail runs from a little up the blade to its tip.
		const FBox BladeBox = Blade->GetStaticMesh()->GetBoundingBox();
		const FTransform& Grip = Blade->GetComponentTransform();
		const float Reach = BladeBox.Max.X;
		Samples.Add({ Grip.TransformPosition(FVector(Reach * 0.25f, 0.f, 0.f)), Grip.TransformPosition(FVector(Reach, 0.f, 0.f)), Now });
	}
	Samples.RemoveAll([this, Now](const FSample& Sample) { return Now - Sample.Time > FadeSeconds; });
	Rebuild();
}

void UWeaponTrailComponent::Rebuild()
{
	if (Samples.Num() < 2)
	{
		if (GetNumSections() > 0)
		{
			ClearAllMeshSections();
		}
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float Fade = 1.f - FMath::Clamp((Now - Samples[i].Time) / FadeSeconds, 0.f, 1.f);
		const float U = static_cast<float>(i) / (Samples.Num() - 1);
		Vertices.Add(Samples[i].Base);
		Vertices.Add(Samples[i].Tip);
		UVs.Add(FVector2D(U, 0.f));
		UVs.Add(FVector2D(U, 1.f));
		FLinearColor Faded = Color;
		Faded.A *= Fade;
		Colors.Add(Faded * FLinearColor(1.f, 1.f, 1.f, 0.4f));
		Colors.Add(Faded);
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		if (i > 0)
		{
			const int32 A = (i - 1) * 2;
			Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
			// Both faces, so it shows from either side.
			Triangles.Append({ A, A + 2, A + 1, A + 1, A + 2, A + 3 });
		}
	}
	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}
