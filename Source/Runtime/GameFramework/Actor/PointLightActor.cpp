#include "EnginePCH.h"
#include "GameFramework/Actor/PointLightActor.h"

APointLightActor::APointLightActor()
{
	SetLightIcon("Editor/Icon/PointLight_64x.png");
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	PointLightComponent->SetupAttachment(GetRootComponent());
}
