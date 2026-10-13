#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/DirectionalLightComponent.h"

class ADirectionalLightActor : public AActor
{
	DECLARE_CLASS(ADirectionalLightActor, AActor)
	REFLECT_START(ADirectionalLightActor)
		REFLECT_END()

public:
	ADirectionalLightActor();
	UDirectionalLightComponent* GetDirectionalLightComponent() const { return DirectionalLightComponent; }

private:
	UBillboardComponent* BillboardComponent = nullptr;
	UDirectionalLightComponent* DirectionalLightComponent = nullptr;
};
