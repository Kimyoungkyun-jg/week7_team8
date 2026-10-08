#pragma once

#include "GameFramework/Actor.h"
#include "Component/FireBallComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ProjectileMovementComponent.h"

// 투사체 액터 클래스
class AProjectileActor : public AActor
{
	DECLARE_CLASS(AProjectileActor, AActor)
	REFLECT_START(ClassName)
	REFLECT_END()

public:
	AProjectileActor();
	virtual ~AProjectileActor() override = default;
    virtual void Tick(float DeltaTime) override;

    void Launch(const FVector& Position, const FVector& Direction, bool bUseStaticMesh = false);
    void ReturnToPool();
    bool IsInFlight() const { return bInFlight; }

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	UFireBallComponent* GetFireBallComponent() const { return FireBallComponent; }
	UStaticMeshComponent* GetFireBallMeshComponent() const { return FireBallMeshComponent; }
	UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }

private:
	bool bInFlight = false;
	float RemainingLife = 0.0f;
	float LifeTime = 5.0f;

	// 파이어볼 컴포넌트
	UFireBallComponent* FireBallComponent = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UStaticMeshComponent* FireBallMeshComponent = nullptr;
	// 투사체 이동 컴포넌트
	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;
};
