#include "EnginePCH.h"
#include "GameFramework/Actor/PointLightActor.h"

APointLightActor::APointLightActor()
{
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	PointLightComponent->SetupAttachment(GetRootComponent());
}
