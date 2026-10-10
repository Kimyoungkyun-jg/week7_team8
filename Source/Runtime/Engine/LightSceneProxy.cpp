#include "EnginePCH.h"
#include "Engine/LightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Component/DirectionalLightComponent.h"

#include <algorithm>

void FLightSceneProxy::Update()
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	bVisible = Component->IsVisible();
	bHiddenInGame = Component->IsHiddenInGame();
	const FVector Position = Component->GetWorldLocation();
	const FVector4& Color = Component->GetLightColor();
	const float Radius = std::max(0.0f, Component->GetAttenuationRadius());
	const float Intensity = std::max(0.0f, Component->GetIntensity());
	RenderData.PositionRadius = FVector4(Position.X, Position.Y, Position.Z, Radius);
	RenderData.ColorIntensity = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), Intensity);
	RenderData.Falloff = FVector4(std::max(0.0f, Component->GetRadiusFallOff()), 0.0f, 0.0f, 0.0f);
	bEnabled = Component->IsEnabled() && Radius > 0.0f && Intensity > 0.0f;
}

void FDirectionalLightSceneProxy::Update()
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	bVisible = Component->IsVisible();
	bHiddenInGame = Component->IsHiddenInGame();
	const FVector Direction = Component->GetWorldRotation().Quaternion().GetForwardVector().Normalized();
	const FVector4& Color = Component->GetLightColor();
	const float Intensity = std::max(0.0f, Component->GetIntensity());
	RenderData.DirectionIntensity = FVector4(Direction.X, Direction.Y, Direction.Z, Intensity);
	RenderData.Color = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), 1.0f);
	bEnabled = Component->IsEnabled() && Intensity > 0.0f;
}
