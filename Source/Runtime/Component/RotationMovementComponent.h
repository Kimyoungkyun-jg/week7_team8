#pragma once
#include "EnginePCH.h"
#include "MovementComponent.h"

class AActor;

class RotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(RotatingMovementComponent, UMovementComponent)
	REFLECT_START(RotatingMovementComponent)
		PROPERTY(RotationRate)
		PROPERTY(PivotTranslation)
		PROPERTY(bRotateLocal)
	REFLECT_END()

public:
	RotatingMovementComponent();

	void BeginPlay();
	void TickComponent(float DeltaTime) override;
	void SetRotationRate(FRotator Rotate);
	void SetPivotTranslation(FVector Pivot);
	void SetbRotationInLocalSpace(bool bRotationInLocal) { bRotateLocal = bRotationInLocal; }
	const FRotator GetRotationRate() { return RotationRate; }
	const FVector GetPivotTranslation() { return PivotTranslation; }
	const bool GetbRotationInLocalSpace() { return bRotateLocal; }

private:
	FRotator RotationRate;
	FVector PivotTranslation;
	bool bRotateLocal= true;
};
