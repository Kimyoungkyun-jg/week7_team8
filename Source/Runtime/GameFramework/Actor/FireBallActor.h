#pragma once

#include "GameFramework/Actor.h"
#include "Component/FireBallComponent.h"
#include "Component/StaticMeshComponent.h"

class AFireBallActor : public AActor
{
	DECLARE_CLASS(AFireBallActor, AActor)
	REFLECT_START(ClassName)
		REFLECT_END()

public:
	AFireBallActor();
	virtual ~AFireBallActor() = default;

	UFireBallComponent* GetFireBallComponent() const { return FireBallComponent; }

private:
	UFireBallComponent* FireBallComponent;
	UStaticMeshComponent* SphereMesh = nullptr;
};
