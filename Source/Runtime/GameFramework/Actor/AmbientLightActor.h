#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/AmbientLightComponent.h"

class AAmbientLightActor : public AActor
{
	DECLARE_CLASS(AAmbientLightActor, AActor)
	REFLECT_START(AAmbientLightActor)
		REFLECT_END()

public:
	AAmbientLightActor();
	UAmbientLightComponent* GetAmbientLightComponent() const { return AmbientLightComponent; }

private:
	UBillboardComponent* BillboardComponent = nullptr;
	UAmbientLightComponent* AmbientLightComponent = nullptr;
};
