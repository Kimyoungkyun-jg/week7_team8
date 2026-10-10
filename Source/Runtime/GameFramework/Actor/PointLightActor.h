#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/PointLightComponent.h"

class APointLightActor : public ALightActor
{
	DECLARE_CLASS(APointLightActor, ALightActor)
	REFLECT_START(APointLightActor)
	REFLECT_END()

public:
	APointLightActor();
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }
	USceneComponent* GetLightComponent() const override { return PointLightComponent; }

private:
	UPointLightComponent* PointLightComponent = nullptr;
};
