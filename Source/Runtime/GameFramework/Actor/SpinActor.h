#pragma once

#include "GameFramework/Actor.h"
#include "Component/StaticMeshComponent.h"

class ASpinActor : public AActor
{
	DECLARE_CLASS(ASpinActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ASpinActor();
	virtual ~ASpinActor() override = default;

	virtual void BeginPlay() override;

	void Tick(float DeltaTime) override;

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	void SetPrimitiveType(EPrimitiveType Type);

	
private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	FRotator rotator;
};
