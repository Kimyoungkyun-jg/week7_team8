#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/SpotLightComponent.h"

class ASpotLight : public ALightActor
{
	DECLARE_CLASS(ASpotLight, ALightActor)
	REFLECT_START(ASpotLight)
	REFLECT_END()

public:
	ASpotLight();
	USpotLightComponent* GetSpotLightComponent() const { return SpotLightComponent; }
	USceneComponent* GetLightComponent() const override { return SpotLightComponent; }

private:
	USpotLightComponent* SpotLightComponent = nullptr;
};
