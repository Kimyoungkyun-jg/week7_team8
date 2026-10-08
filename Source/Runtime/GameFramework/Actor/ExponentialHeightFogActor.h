#pragma once

#include "GameFramework/Actor.h"
#include "Component/ExponentialHeightFogComponent.h"

class AExponentialHeightFogActor : public AActor
{
	DECLARE_CLASS(AExponentialHeightFogActor, AActor);
public:
	AExponentialHeightFogActor();
	virtual ~AExponentialHeightFogActor();

	inline UExponentialHeightFogComponent* GetExponentialHeightFogComponent() const { return ExponentialHeightFogComponent; };
private:
	UExponentialHeightFogComponent* ExponentialHeightFogComponent;
};
