#include "EnginePCH.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"

APointLightActor::APointLightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	if (UTexture2D* LightIcon = UAssetManager::Get().LoadTexture("Editor/Icon/PointLight_64x.png"))
	{
		BillboardComponent->SetSprite(LightIcon);
	}
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	PointLightComponent->SetupAttachment(BillboardComponent);
}
