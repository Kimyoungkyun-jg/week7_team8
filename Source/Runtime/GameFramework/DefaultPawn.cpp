#include "EnginePCH.h"
#include "Camera/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/AssetManager.h"
#include "ObjectSystem/ObjectFactory.h"
#include "DefaultPawn.h"
#include "Engine/World.h"
#include "GameFramework/Actor/ProjectileActor.h"

ADefaultPawn::ADefaultPawn()
{
	SetActorEnableCollision(false);
	ProjectileMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere");
	Projectiles.fill(nullptr); // The world is assigned after the constructor.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	SetRootComponent(StaticMeshComponent);

	StaticMeshComponent->SetStaticMesh(UAssetManager::GetMesh("Hitori"));
	CameraComponent->SetupAttachment(StaticMeshComponent);

	CameraComponent->SetRelativeLocation({ -5,0,8 });
	CameraComponent->SetRelativeRotation({ 45,0,0 });
	CameraComponent->SetExternalInputManaged(true);
}



void ADefaultPawn::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
    FVector NewLocation = GetActorLocation() + WorldDirection * (ScaleValue * MoveSpeed);
    if (RootComponent)
    {
        RootComponent->SetRelativeLocation(NewLocation);
    }
}

void ADefaultPawn::AddControllerYawInput(float Value)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += Value;
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(NewRotation);
	}
}

void ADefaultPawn::AddControllerPitchInput(float Value)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Pitch += Value;
	NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + Value, -89.0f, 89.0f);
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(NewRotation);
	}
}
void ADefaultPawn::BeginPlay()
{
    Super::BeginPlay();
    UWorld* PawnWorld = GetWorld();
    if (!PawnWorld || PawnWorld->GetWorldType() != EWorldType::PIE) return;
    for (AProjectileActor*& Projectile : Projectiles)
    {
        if (!Projectile) Projectile = PawnWorld->SpawnActor<AProjectileActor>();
    }
}

void ADefaultPawn::FireProjectile()
{
    UWorld* PawnWorld = GetWorld();
    if (!PawnWorld || PawnWorld->GetWorldType() != EWorldType::PIE || !CameraComponent) return;

    if (bUseStaticMeshProjectiles && !ProjectileMesh) return;

    int32 SelectedIndex = -1;
    for (int32 Offset = 0; Offset < ProjectilePoolSize; ++Offset)
    {
        const int32 Index = (NextProjectileIndex + Offset) % ProjectilePoolSize;
        if (Projectiles[Index] && !Projectiles[Index]->IsInFlight())
        {
            SelectedIndex = Index;
            break;
        }
    }
    // If all slots are flying, reuse the next slot in the circular pool.
    if (SelectedIndex < 0) SelectedIndex = NextProjectileIndex;
    AProjectileActor* Projectile = Projectiles[SelectedIndex];
    if (!Projectile) return;

    const FVector Forward = CameraComponent->GetWorldRotation().Quaternion().GetForwardVector().Normalized();
    const FVector Position = CameraComponent->GetWorldLocation() + Forward * 2.0f;
    if (bUseStaticMeshProjectiles) Projectile->GetStaticMeshComponent()->SetStaticMesh(ProjectileMesh);
    Projectile->Launch(Position, Forward, bUseStaticMeshProjectiles);
    NextProjectileIndex = (SelectedIndex + 1) % ProjectilePoolSize;
}
