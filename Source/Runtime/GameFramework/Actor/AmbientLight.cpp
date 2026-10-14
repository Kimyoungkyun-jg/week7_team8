#include "EnginePCH.h"
#include "GameFramework/Actor/AmbientLight.h"

AAmbientLight::AAmbientLight()
{
	AmbientLightComponent = CreateDefaultSubobject<UAmbientLightComponent>("UAmbientLightComponent");
	AmbientLightComponent->SetupAttachment(GetRootComponent());
}
