#include "EnginePCH.h"
#include "MovementComponent.h"
#include "GameFramework/Actor.h"

UMovementComponent::UMovementComponent()
	: Velocity(FVector::ZeroVector)
	, UpdatedComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UMovementComponent::~UMovementComponent()
{
}

UMovementComponent::UMovementComponent(float InMaxSpeed, float InAcceleration, float InDeceleration)
	: Velocity(FVector::ZeroVector)
	, MaxSpeed(InMaxSpeed)
	, UpdatedComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!UpdatedComponent && GetOwner())
	{
		UpdatedComponent = GetOwner()->GetRootComponent();
	}
}

void UMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit)
{
	return MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit);
}

bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit)
{
	return MoveUpdatedComponentImpl(Delta, NewRotation.Quaternion(), bSweep, OutHit);
}

bool UMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit)
{
	if (!UpdatedComponent) return false;
	// 이동시킬 컴포넌트의 MoveComponent 함수를 호출하여 실제 이동을 수행
	return UpdatedComponent->MoveComponent(Delta, NewRotation, bSweep, OutHit);
}

void UMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{

}