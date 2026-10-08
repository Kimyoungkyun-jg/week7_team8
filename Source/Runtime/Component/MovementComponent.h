#pragma once

#include "ActorComponent.h"
#include "SceneComponent.h"

class UMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UMovementComponent, UActorComponent)
	REFLECT_START(ClassName)
		PROPERTY(MaxSpeed)
		PROPERTY(UpdatedComponent)
		REFLECT_END()
public:
	UMovementComponent();
	UMovementComponent(float InMaxSpeed, float InAcceleration, float InDeceleration);
	virtual ~UMovementComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	// 이동 시킬 대상 컴포넌트의 이동 함수를 호출
	bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL);
	bool MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit = NULL);
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL);

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector);

	FVector GetVelocity() const { return Velocity; }
	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }

	float GetMaxSpeed() const { return MaxSpeed; }
	void SetMaxSpeed(float InMaxSpeed) { MaxSpeed = InMaxSpeed; }

	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
	void SetUpdatedComponent(USceneComponent* InUpdatedComponent) { UpdatedComponent = InUpdatedComponent; }

protected:
	FVector Velocity = FVector::ZeroVector;
	float MaxSpeed = 100.0f;
	USceneComponent* UpdatedComponent = nullptr;
};