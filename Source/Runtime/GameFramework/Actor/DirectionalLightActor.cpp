#include "EnginePCH.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"

ADirectionalLightActor::ADirectionalLightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	if (UTexture2D* LightIcon = UAssetManager::Get().LoadTexture("Editor/Icon/DirectionalLight_256x.png"))
	{
		BillboardComponent->SetSprite(LightIcon);
	}
	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("UDirectionalLightComponent");
	DirectionalLightComponent->SetupAttachment(BillboardComponent);
}
