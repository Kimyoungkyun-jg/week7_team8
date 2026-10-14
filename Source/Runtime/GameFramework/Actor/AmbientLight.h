#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/AmbientLightComponent.h"

class AAmbientLight : public ALightActor
{
	DECLARE_CLASS(AAmbientLight, ALightActor)
	REFLECT_START(AAmbientLight)
	REFLECT_END()

public:
	AAmbientLight();
	USceneComponent* GetLightComponent() const override { return AmbientLightComponent; }

private:
	UAmbientLightComponent* AmbientLightComponent = nullptr;
};
