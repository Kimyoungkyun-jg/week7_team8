#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"

void FDirectionalLightSceneProxy::Update()
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	UDirectionalLightComponent* DirectionalComponent = Cast<UDirectionalLightComponent>(Component);
	if (!DirectionalComponent)
	{
		return;
	}

	bVisible = DirectionalComponent->IsVisible();
	bHiddenInGame = DirectionalComponent->IsHiddenInGame();

	const FVector Direction = DirectionalComponent->GetForwardVector();
	const FVector4& Color = DirectionalComponent->GetLightColor();
	const float Intensity = std::max(0.0f, DirectionalComponent->GetIntensity());

	RenderInfo.Direction = Direction;
	RenderInfo.Color = FVector(std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z));
	RenderInfo.Intensity = Intensity;
	
	bEnabled = DirectionalComponent->IsEnabled() && Intensity > 0.0f;
}

ELightType FDirectionalLightSceneProxy::GetLightType() const
{
	return ELightType::Directional;
}
