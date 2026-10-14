#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/DirectionalLightComponent.h"

class UStaticMeshComponent;

class ADirectionalLight : public ALightActor
{
	DECLARE_CLASS(ADirectionalLight, ALightActor)
	REFLECT_START(ADirectionalLight)
	REFLECT_END()

public:
	ADirectionalLight();
	UDirectionalLightComponent* GetDirectionalLightComponent() const { return DirectionalLightComponent; }
	UStaticMeshComponent* GetDirectionArrowComponent() const { return DirectionArrowComponent; }
	USceneComponent* GetLightComponent() const override { return DirectionalLightComponent; }

private:
	void UpdateDirectionArrow();

	UDirectionalLightComponent* DirectionalLightComponent = nullptr;
	UStaticMeshComponent* DirectionArrowComponent = nullptr;
};
