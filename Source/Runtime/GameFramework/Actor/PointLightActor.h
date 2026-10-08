#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/PointLightComponent.h"

class APointLightActor : public AActor
{
	DECLARE_CLASS(APointLightActor, AActor)
	REFLECT_START(APointLightActor)
	REFLECT_END()

public:
	APointLightActor();
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
	UBillboardComponent* BillboardComponent = nullptr;
	UPointLightComponent* PointLightComponent = nullptr;
};
