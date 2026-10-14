#include "EnginePCH.h"
#include "PointLightSceneProxy.h"
#include "Component/PointLightComponent.h"

void FPointLightSceneProxy::Update()
{	
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Component);
	if (!PointLightComponent)
	{
		return;
	}

	bVisible = PointLightComponent->IsVisible();
	bHiddenInGame = PointLightComponent->IsHiddenInGame();

	const FVector Position = PointLightComponent->GetWorldLocation();
	const FVector4& Color = PointLightComponent->GetLightColor();
	const float Radius = std::max(0.0f, PointLightComponent->GetAttenuationRadius());
	const float Intensity = std::max(0.0f, PointLightComponent->GetIntensity());

	RenderInfo.Position = FVector(Position.X, Position.Y, Position.Z);
	RenderInfo.Radius = Radius;	
	RenderInfo.Color = FVector(std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z));
	RenderInfo.Intensity = Intensity;	

	bEnabled = PointLightComponent->IsEnabled() && Radius > 0.0f && Intensity > 0.0f;
}

ELightType FPointLightSceneProxy::GetLightType() const
{
	return ELightType::Point;
}
