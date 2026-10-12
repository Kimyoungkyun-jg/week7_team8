#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/SkyLightComponent.h"

class ASkyLight : public ALightActor
{
	DECLARE_CLASS(ASkyLight, ALightActor)
	REFLECT_START(ASkyLight)
	REFLECT_END()

public:
	ASkyLight();
	USkyLightComponent* GetSkyLightComponent() const { return SkyLightComponent; }
	USceneComponent* GetLightComponent() const override { return SkyLightComponent; }

private:
	USkyLightComponent* SkyLightComponent = nullptr;
};
