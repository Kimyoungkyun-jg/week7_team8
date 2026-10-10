#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"

// Common actor root for concrete lights.
class ALightActor : public AActor
{
	DECLARE_CLASS(ALightActor, AActor)

	REFLECT_START(ALightActor)
	REFLECT_END()

public:
	ALightActor();
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	virtual USceneComponent* GetLightComponent() const { return nullptr; }

protected:
	void SetLightIcon(const char* Path);

private:
	UBillboardComponent* BillboardComponent = nullptr;
};
