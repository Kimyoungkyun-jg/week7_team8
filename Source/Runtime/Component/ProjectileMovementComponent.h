#pragma once

#include "MovementComponent.h"

class UProjectileMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)
	REFLECT_START(ClassName)
		PROPERTY_RANGE(InitialSpeed, 0.0f, 100.0f)
		PROPERTY_RANGE(MaxSpeed, 0.0f, 200.0f)
		PROPERTY(bShouldBounce)
		PROPERTY_RANGE(Bounciness, 0.0f, 1.0f)
		PROPERTY(bRotationFollowsVelocity)
		PROPERTY(bIsGravityEnabled)
		PROPERTY_RANGE(GravityScale, 0.0f, 2.0f)
		PROPERTY(bSweep)
		PROPERTY(bIsFrictionEnabled)
		PROPERTY_RANGE(Friction, 0.0f, 1.0f)
		REFLECT_END()
public:
	UProjectileMovementComponent();
	UProjectileMovementComponent(float InInitialSpeed, float InMaxSpeed, bool bInShouldBounce, float InBounciness, float InGravityScale);
	virtual ~UProjectileMovementComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	// 가속도 계산. 중력, 호밍, 외부 힘 등을 적용할 수 있다.
	FVector ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const;
	// 충돌 처리. Bounce, Stop 등 처리
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	// 최대 속도 제한. MaxSpeed를 초과하면 MaxSpeed로 제한한다.
	FVector LimitVelocity(FVector NewVelocity) const;
	// 속도 계산. 가속도와 DeltaTime을 적용하여 새로운 속도를 계산한다.
	virtual FVector ComputeVelocity(FVector InitialVelocity, float DeltaTime) const;
	// 이동량 계산
	virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;
	// 충돌 후 반사 속도 계산. Bounce 시 반사 속도를 계산한다.
	virtual FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);
	// 시뮬레이션 중지(속도가 일정 이하거나 반복 횟수가 많을 때)
	void StopSimulating(const FHitResult& HitResult);

	float GetInitialSpeed() const { return InitialSpeed; }
	void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
	float GetMaxSpeed() const { return MaxSpeed; }
	void SetMaxSpeed(float InMaxSpeed) { MaxSpeed = InMaxSpeed; }
	bool GetShouldBounce() const { return bShouldBounce; }
	void SetShouldBounce(bool bInShouldBounce) { bShouldBounce = bInShouldBounce; }
	bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }
	void SetRotationFollowsVelocity(bool bInRotationFollowsVelocity) { bRotationFollowsVelocity = bInRotationFollowsVelocity; }
	float GetBounciness() const { return Bounciness; }
	void SetBounciness(float InBounciness) { Bounciness = InBounciness; }
	float GetGravityScale() const { return GravityScale; }
	void SetGravityScale(float InGravityScale) { GravityScale = InGravityScale; }
	bool GetIsGravityEnabled() const { return bIsGravityEnabled; }
	void SetIsGravityEnabled(bool bInIsGravityEnabled) { bIsGravityEnabled = bInIsGravityEnabled; }
	bool GetSweep() const { return bSweep; }
	void SetSweep(bool bInSweep) { bSweep = bInSweep; }
	bool GetIsFrictionEnabled() const { return bIsFrictionEnabled; }
	void SetIsFrictionEnabled(bool bInIsFrictionEnabled) { bIsFrictionEnabled = bInIsFrictionEnabled; }
	float GetFriction() const { return Friction; }
	void SetFriction(float InFriction) { Friction = InFriction; }

private:
	float InitialSpeed = 20.0f;
	bool bShouldBounce = true;
	float Bounciness = 0.6f;
	bool bRotationFollowsVelocity = true;
	bool bIsGravityEnabled = true;
	float GravityScale = 1.0f;
	bool bSweep = true;
	bool bIsFrictionEnabled = false;
	bool bInitialVelocityInLocalSpace = true;



	float Friction = 0.2f;
	// 정지 속ㄷ도 임계값. 이 값보다 속도가 작으면 시뮬레이션을 중지한다.
	float BounceVelocityStopSimulatingThreshold = 5.0f;
	// 시뮬레이션 반복 횟수 제한. 이 값보다 반복 횟수가 많으면 시뮬레이션을 중지한다.
	int32 MaxSimulationIterations = 4;
};