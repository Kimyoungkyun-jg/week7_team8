#pragma once

#include "Pawn.h"
#include "PlayerController.h"
#include <array>


class AProjectileActor;
class UCameraComponent;
class UStaticMeshComponent;
class UStaticMesh;

class ADefaultPawn : public APawn
{
	DECLARE_CLASS(ADefaultPawn, APawn)
	REFLECT_START(ClassName)
		PROPERTY(bUseStaticMeshProjectiles)
		PROPERTY(ProjectileMesh)
		REFLECT_END()
public:
	ADefaultPawn();
	virtual ~ADefaultPawn() override = default;
    virtual void BeginPlay() override;
    void FireProjectile();

    static constexpr int32 ProjectilePoolSize = 16;
    const std::array<AProjectileActor*, ProjectilePoolSize>& GetProjectiles() const { return Projectiles; }

	// 이동 및 회전
	virtual void AddMovementInput(const FVector& WorldDirection, float ScaleValue) override;
	virtual void AddControllerYawInput(float Value) override;
	virtual void AddControllerPitchInput(float Value) override;

    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    bool UsesStaticMeshProjectiles() const { return bUseStaticMeshProjectiles; }
    void SetUseStaticMeshProjectiles(bool bUseMesh) { bUseStaticMeshProjectiles = bUseMesh; }
    UStaticMesh* GetProjectileMesh() const { return ProjectileMesh; }
    void SetProjectileMesh(UStaticMesh* Mesh) { ProjectileMesh = Mesh; }

	// 카메라 컴포넌트 반환
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

protected:
	bool bUseStaticMeshProjectiles = false;
	UStaticMesh* ProjectileMesh = nullptr;
	std::array<AProjectileActor*, ProjectilePoolSize> Projectiles{};
	int32 NextProjectileIndex = 0;
	float MoveSpeed = 20.0f;
	UCameraComponent* CameraComponent = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;


};