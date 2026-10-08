#include "EnginePCH.h"
#include "ProjectileActor.h"
#include "Engine/World.h"
#include "Asset/AssetManager.h"

AProjectileActor::AProjectileActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	FireBallComponent = CreateDefaultSubobject<UFireBallComponent>("UFireBallComponent");
	SetRootComponent(FireBallComponent);
	FireBallComponent->SetAutoActivate(false);
	FireBallComponent->SetVisibility(false);

    // The depth-based FireBall effect needs a visible surface travelling with it.
    FireBallMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("FireBallMeshComponent");
    FireBallMeshComponent->SetupAttachment(FireBallComponent);
    FireBallMeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"));
    FireBallMeshComponent->SetRelativeScale3D(FVector(0.5f));
    FireBallMeshComponent->SetAutoActivate(false);
    FireBallMeshComponent->SetVisibility(false);

    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("ProjectileMeshComponent");
    StaticMeshComponent->SetupAttachment(FireBallComponent);
    StaticMeshComponent->SetAutoActivate(false);
    StaticMeshComponent->SetVisibility(false);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("UProjectileMovementComponent");
	ProjectileMovementComponent->SetUpdatedComponent(FireBallComponent);
    ProjectileMovementComponent->SetAutoActivate(false);
    ProjectileMovementComponent->SetComponentTickEnabled(false);
    ProjectileMovementComponent->OnComponentDeactivated.Add(
        [this](UActorComponent*) { if (bInFlight) ReturnToPool(); });
}

void AProjectileActor::Launch(const FVector& Position, const FVector& Direction, bool bUseStaticMesh)
{
    if (!GetWorld() || GetWorld()->GetWorldType() != EWorldType::PIE ||
        Direction.Dot(Direction) <= 0.0001f) return;
    const FVector Forward = Direction.Normalized();
    if (bUseStaticMesh && !StaticMeshComponent->GetStaticMesh()) return;
    ReturnToPool();
    // Detach both first so switching roots cannot create an attachment cycle.
    FireBallComponent->DetachFromParent();
    StaticMeshComponent->DetachFromParent();
    UPrimitiveComponent* ActiveComponent = bUseStaticMesh
        ? static_cast<UPrimitiveComponent*>(StaticMeshComponent)
        : static_cast<UPrimitiveComponent*>(FireBallComponent);
    UPrimitiveComponent* InactiveComponent = bUseStaticMesh
        ? static_cast<UPrimitiveComponent*>(FireBallComponent)
        : static_cast<UPrimitiveComponent*>(StaticMeshComponent);
    SetRootComponent(ActiveComponent);
    ActiveComponent->SetTransform(FTransform::Identity);
    ActiveComponent->SetRelativeLocation(Position);
    InactiveComponent->SetupAttachment(ActiveComponent);
    InactiveComponent->SetTransform(FTransform::Identity);
    // StopSimulating clears UpdatedComponent, so restore the selected root on every reuse.
    ProjectileMovementComponent->SetUpdatedComponent(ActiveComponent);
    ProjectileMovementComponent->SetVelocity(Forward * ProjectileMovementComponent->GetInitialSpeed());
    RemainingLife = LifeTime;
    bInFlight = true;
    ActiveComponent->SetVisibility(true);
    ActiveComponent->Activate();
    if (!bUseStaticMesh)
    {
        FireBallMeshComponent->SetRelativeScale3D(FVector(std::max(FireBallComponent->GetRadius(), 0.0f) * 0.5f));
        FireBallMeshComponent->SetVisibility(true);
        FireBallMeshComponent->Activate();
    }
    ProjectileMovementComponent->Activate(true);
    PrimaryActorTick.SetTickFunctionEnable(true);
}

void AProjectileActor::ReturnToPool()
{
    bInFlight = false;
    RemainingLife = 0.0f;
    ProjectileMovementComponent->SetVelocity(FVector::ZeroVector);
    // Clear bInFlight before broadcasting to avoid re-entering through the delegate.
    ProjectileMovementComponent->Deactivate();
    FireBallComponent->Deactivate();
    FireBallComponent->SetVisibility(false);
    FireBallMeshComponent->Deactivate();
    FireBallMeshComponent->SetVisibility(false);
    StaticMeshComponent->Deactivate();
    StaticMeshComponent->SetVisibility(false);
    PrimaryActorTick.SetTickFunctionEnable(false);
}

void AProjectileActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bInFlight) return;
    RemainingLife -= DeltaTime;
    if (RemainingLife <= 0.0f) ReturnToPool();
}
