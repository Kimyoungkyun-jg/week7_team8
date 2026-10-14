#include "EnginePCH.h"
#include "Engine/LightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/SkyLightComponent.h"
#include "Component/AmbientLightComponent.h"

#include <algorithm>
#include <cmath>

void FLightSceneProxy::UpdateCommon(bool bComponentEnabled)
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	bVisible = Component->IsVisible();
	bHiddenInGame = Component->IsHiddenInGame();
	bEnabled = bComponentEnabled && Component->GetIntensity() > 0.0f;
}

FAmbientLightSceneProxy::FAmbientLightSceneProxy(UAmbientLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
	Update();
}

void FAmbientLightSceneProxy::Update()
{
	UAmbientLightComponent* AmbientComponent = static_cast<UAmbientLightComponent*>(Component);
	UpdateCommon(AmbientComponent && AmbientComponent->IsEnabled());
	if (!AmbientComponent) return;
	const FVector4& Color = AmbientComponent->GetLightColor();
	RenderData.ColorIntensity = FVector4(std::max(0.0f, Color.X), std::max(0.0f, Color.Y),
		std::max(0.0f, Color.Z), std::max(0.0f, AmbientComponent->GetIntensity()));
}

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
	Update();
}

void FPointLightSceneProxy::Update()
{
	auto* PointComponent = static_cast<UPointLightComponent*>(Component);
	UpdateCommon(PointComponent && PointComponent->IsEnabled());
	if (!PointComponent) return;

	const FVector Position = PointComponent->GetWorldLocation();
	const FVector4& Color = PointComponent->GetLightColor();
	const float Radius = std::max(0.0f, PointComponent->GetAttenuationRadius());
	const float Intensity = std::max(0.0f, PointComponent->GetIntensity());
	RenderData.PositionRadius = FVector4(Position.X, Position.Y, Position.Z, Radius);
	RenderData.ColorIntensity = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), Intensity);
	RenderData.Falloff = FVector4(std::max(0.0f, PointComponent->GetRadiusFallOff()), 0.0f, 0.0f, 0.0f);
	bEnabled = bEnabled && Radius > 0.0f;
}

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
	Update();
}

void FDirectionalLightSceneProxy::Update()
{
	auto* DirectionalComponent = static_cast<UDirectionalLightComponent*>(Component);
	UpdateCommon(DirectionalComponent && DirectionalComponent->IsEnabled());
	if (!DirectionalComponent) return;

	const FVector Direction = DirectionalComponent->GetWorldDirection();
	const FVector4& Color = DirectionalComponent->GetLightColor();
	const float Intensity = std::max(0.0f, DirectionalComponent->GetIntensity());
	RenderData.DirectionIntensity = FVector4(Direction.X, Direction.Y, Direction.Z, Intensity);
	RenderData.Color = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), 1.0f);
}

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
	Update();
}

void FSpotLightSceneProxy::Update()
{
	auto* SpotComponent = static_cast<USpotLightComponent*>(Component);
	UpdateCommon(SpotComponent && SpotComponent->IsEnabled());
	if (!SpotComponent) return;

	const FVector Position = SpotComponent->GetWorldLocation();
	const FVector Direction = SpotComponent->GetWorldRotation().Quaternion().GetForwardVector().Normalized();
	const FVector4& Color = SpotComponent->GetLightColor();
	const float Radius = std::max(0.0f, SpotComponent->GetAttenuationRadius());
	const float Intensity = std::max(0.0f, SpotComponent->GetIntensity());
	const float OuterAngle = FMath::Clamp(SpotComponent->GetOuterConeAngle(), 0.0f, 89.0f);
	const float InnerAngle = FMath::Clamp(SpotComponent->GetInnerConeAngle(), 0.0f, OuterAngle);
	RenderData.PositionRadius = FVector4(Position.X, Position.Y, Position.Z, Radius);
	RenderData.DirectionOuterCos = FVector4(Direction.X, Direction.Y, Direction.Z,
		std::cos(FMath::DegreesToRadians(OuterAngle)));
	RenderData.ColorIntensity = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), Intensity);
	RenderData.InnerCosFalloff = FVector4(std::cos(FMath::DegreesToRadians(InnerAngle)),
		std::max(0.0f, SpotComponent->GetFalloff()), 0.0f, 0.0f);
	bEnabled = bEnabled && Radius > 0.0f;
}

FSkyLightSceneProxy::FSkyLightSceneProxy(USkyLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
	Update();
}

void FSkyLightSceneProxy::Update()
{
	auto* SkyComponent = static_cast<USkyLightComponent*>(Component);
	UpdateCommon(SkyComponent && SkyComponent->IsEnabled());
	if (!SkyComponent) return;

	const FVector4& Color = SkyComponent->GetLightColor();
	RenderData.ColorIntensity = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z),
		std::max(0.0f, SkyComponent->GetIntensity()));
	RenderData.SourcePath = SkyComponent->GetSourcePath();
}
