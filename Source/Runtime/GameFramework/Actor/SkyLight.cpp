#include "EnginePCH.h"
#include "GameFramework/Actor/SkyLight.h"

ASkyLight::ASkyLight()
{
	SetLightIcon("Editor/Icon/SkyLight_64x.png");
	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>("USkyLightComponent");
	SkyLightComponent->SetupAttachment(GetRootComponent());
}
