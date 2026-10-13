#include "EnginePCH.h"
#include "GameFramework/Actor/AmbientLightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"

AAmbientLightActor::AAmbientLightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	if (UTexture2D* LightIcon = UAssetManager::Get().LoadTexture("Editor/Icon/AmbientLight_256x.png"))
	{
		BillboardComponent->SetSprite(LightIcon);
	}
	AmbientLightComponent = CreateDefaultSubobject<UAmbientLightComponent>("UAmbientLightComponent");
	AmbientLightComponent->SetupAttachment(BillboardComponent);
}
