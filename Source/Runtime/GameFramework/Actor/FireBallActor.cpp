#include "EnginePCH.h"
#include "FireBallActor.h"
#include "Component/FireBallComponent.h"
#include "Asset/AssetManager.h"
AFireBallActor::AFireBallActor()
{
	PrimaryActorTick.bCanEverTick = false;
	FireBallComponent = CreateDefaultSubobject<UFireBallComponent>("UFireBallComponent");
	SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>("UPrimitiveComponent");
	if(UStaticMesh* SphereMeshAsset = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"))
	{
		SphereMesh->SetStaticMesh(SphereMeshAsset);
	}
	SphereMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	SetRootComponent(SphereMesh);

	FireBallComponent->SetupAttachment(SphereMesh);
}

