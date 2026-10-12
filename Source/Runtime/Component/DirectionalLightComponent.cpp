#include "EnginePCH.h"
#include "Component/DirectionalLightComponent.h"
#include "Engine/World.h"

#include <algorithm>
#include <cmath>

FVector UDirectionalLightComponent::GetWorldDirection() const
{
	const FVector LocalDirection = Direction.Size() < 1.0e-5f
		? FVector(1.0f, 0.0f, 0.0f) : Direction;
	return GetWorldRotation().Quaternion().RotateVector(LocalDirection).Normalized();
}

void UDirectionalLightComponent::SetDirection(const FVector& InDirection)
{
	if (!std::isfinite(InDirection.X) || !std::isfinite(InDirection.Y) ||
		!std::isfinite(InDirection.Z))
		return;

	Direction = InDirection;
	OnPropertyChanged("Direction");
}

void UDirectionalLightComponent::OnPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == "Direction")
	{
		const auto ClampAxis = [](float Value)
		{
			return std::isfinite(Value) ? std::clamp(Value, -1.0f, 1.0f) : 0.0f;
		};
		Direction = FVector(ClampAxis(Direction.X), ClampAxis(Direction.Y), ClampAxis(Direction.Z));
	}
	Super::OnPropertyChanged(PropertyName);
}

void UDirectionalLightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddDirectionalLight(this);
}

void UDirectionalLightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveDirectionalLight(this);
}
