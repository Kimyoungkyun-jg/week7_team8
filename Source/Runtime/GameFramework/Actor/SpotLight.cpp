#include "EnginePCH.h"
#include "GameFramework/Actor/SpotLight.h"

ASpotLight::ASpotLight()
{
	SetLightIcon("Editor/Icon/SpotLight_64x.png");
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SpotLightComponent->SetupAttachment(GetRootComponent());
}
