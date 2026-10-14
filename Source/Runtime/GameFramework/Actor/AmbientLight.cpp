#include "EnginePCH.h"
#include "GameFramework/Actor/AmbientLight.h"

AAmbientLight::AAmbientLight()
{
	SetLightIcon("Editor/Icon/SkyLight_64x.png");
	AmbientLightComponent = CreateDefaultSubobject<UAmbientLightComponent>("UAmbientLightComponent");
	AmbientLightComponent->SetupAttachment(GetRootComponent());
}
