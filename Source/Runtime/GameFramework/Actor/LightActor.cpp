#include "EnginePCH.h"
#include "LightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"

ALightActor::ALightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
}

void ALightActor::SetLightIcon(const char* Path)
{
	if (UTexture2D* Icon = UAssetManager::Get().LoadTexture(Path))
		BillboardComponent->SetSprite(Icon);
}
