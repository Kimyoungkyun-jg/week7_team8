#include "EnginePCH.h"
#include "Component/DirectionalLightComponent.h"
#include "Engine/World.h"

#include <cmath>

FVector UDirectionalLightComponent::GetDirection() const
{
	return GetWorldRotation().Quaternion().GetForwardVector().Normalized();
}

void UDirectionalLightComponent::SetDirection(const FVector& InDirection)
{
	if (!std::isfinite(InDirection.X) || !std::isfinite(InDirection.Y) ||
		!std::isfinite(InDirection.Z) || InDirection.Size() < 1.0e-5f)
		return;

	const FVector Direction = InDirection.Normalized();
	const float HorizontalLength = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);
	const FRotator Rotation(
		FMath::RadiansToDegrees(std::atan2(-Direction.Z, HorizontalLength)),
		FMath::RadiansToDegrees(std::atan2(Direction.Y, Direction.X)),
		GetWorldRotation().Roll);
	MoveComponent(FVector::ZeroVector, Rotation, false);
}

void UDirectionalLightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddDirectionalLight(this);
}

void UDirectionalLightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveDirectionalLight(this);
}
