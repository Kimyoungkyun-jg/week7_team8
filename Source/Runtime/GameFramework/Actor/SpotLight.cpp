#include "EnginePCH.h"
#include "GameFramework/Actor/SpotLight.h"

ASpotLight::ASpotLight()
{
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SpotLightComponent->SetupAttachment(GetRootComponent());
}

void ASpotLight::Tick(float DeltaTime)
{


}


