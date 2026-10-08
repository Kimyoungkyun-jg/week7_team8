#include "EnginePCH.h"
#include "RotationMovementComponent.h"
#include "ActorComponent.h"
#include "GameFramework/Actor.h"

RotatingMovementComponent::RotatingMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	RotationRate.Yaw = 180.0f;
	bRotateLocal = true;
}

void RotatingMovementComponent::SetRotationRate(FRotator Rotate)
{
	RotationRate = Rotate;
}

void RotatingMovementComponent::SetPivotTranslation(FVector Pivot)
{
	PivotTranslation = Pivot;
}

void RotatingMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void RotatingMovementComponent::TickComponent(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!UpdatedComponent)
		return;
	const FQuat LocalRotation = UpdatedComponent->GetRelativeRotation().Quaternion();
	const FQuat DeltaRotation = FRotator(RotationRate.Pitch*DeltaTime, RotationRate.Yaw * DeltaTime, RotationRate.Roll * DeltaTime ).Quaternion();
	const FQuat NewRotation = bRotateLocal ? (LocalRotation * DeltaRotation) : (DeltaRotation * LocalRotation); // Local 회전시 Old가 마지막, World 기준일경우 Delta가 마지막

	FVector DeltaLocation = FVector::ZeroVector;
	if (!(PivotTranslation==ZeroVector))
	{
		const FVector LocalPivot = LocalRotation.RotateVector(PivotTranslation); // 객체 원점 -> 회전 중심축을 향하는 벡터
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = (LocalPivot - NewPivot); // -New - (-Local) => -New + Local => Local - New
	}

	UpdatedComponent->SetRelativeRotation(NewRotation.ToFRotator());
	UpdatedComponent->SetRelativeLocation(UpdatedComponent->GetRelativeLocation() + DeltaLocation);
}
