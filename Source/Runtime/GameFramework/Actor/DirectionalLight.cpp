#include "EnginePCH.h"
#include "GameFramework/Actor/DirectionalLight.h"

ADirectionalLight::ADirectionalLight()
{
	SetLightIcon("Editor/Icon/PointLight_64x.png");
	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("UDirectionalLightComponent");
	DirectionalLightComponent->SetupAttachment(GetRootComponent());
}
