#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/DirectionalLightComponent.h"

class ADirectionalLight : public ALightActor
{
	DECLARE_CLASS(ADirectionalLight, ALightActor)
	REFLECT_START(ADirectionalLight)
	REFLECT_END()

public:
	ADirectionalLight();
	UDirectionalLightComponent* GetDirectionalLightComponent() const { return DirectionalLightComponent; }
	USceneComponent* GetLightComponent() const override { return DirectionalLightComponent; }

private:
	UDirectionalLightComponent* DirectionalLightComponent = nullptr;
};
