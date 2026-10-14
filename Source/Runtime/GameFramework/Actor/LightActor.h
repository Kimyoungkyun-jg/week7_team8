#pragma once

#include "GameFramework/Actor.h"

// Common actor root for concrete lights.
class ALightActor : public AActor
{
	DECLARE_CLASS(ALightActor, AActor)

	REFLECT_START(ALightActor)
	REFLECT_END()

public:
	ALightActor();
	virtual USceneComponent* GetLightComponent() const { return nullptr; }
};
