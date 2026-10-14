#include "EnginePCH.h"
#include "GameFramework/Actor/SkyLight.h"

ASkyLight::ASkyLight()
{
	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>("USkyLightComponent");
	SkyLightComponent->SetupAttachment(GetRootComponent());
}
