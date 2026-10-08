#pragma once

#include "Actor.h"

class APlayerController;

class APawn : public AActor
{
	DECLARE_CLASS(APawn, AActor)
	REFLECT_START(ClassName)
		REFLECT_END()
public:
	APawn();
	virtual ~APawn() override = default;
	// 이동 및 회전
	virtual void AddMovementInput(const FVector& WorldDirection, float ScaleValue) {}
	virtual void AddControllerYawInput(float Value) {}
	virtual void AddControllerPitchInput(float Value) {}

	void PossessedBy(APlayerController* NewOwner) { Controller = NewOwner; }
	void UnPossessed() { Controller = nullptr; }
	APlayerController* GetController() const { return Controller; }

protected:
	APlayerController* Controller = nullptr;
};