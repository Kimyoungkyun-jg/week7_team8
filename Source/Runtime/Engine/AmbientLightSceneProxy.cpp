#include "EnginePCH.h"
#include "AmbientLightSceneProxy.h"
#include "Component/AmbientLightComponent.h"

void FAmbientLightSceneProxy::Update()
{
	if (!Component)
	{
		return;
	}

	UAmbientLightComponent* AmbientLightComponent = Cast<UAmbientLightComponent>(Component);
	if (!AmbientLightComponent)
	{
		return;
	}

	bVisible = AmbientLightComponent->IsVisible();
	bHiddenInGame = AmbientLightComponent->IsHiddenInGame();
	
	const FVector4& Color = AmbientLightComponent->GetLightColor();
	const float Intensity = std::max(0.0f, AmbientLightComponent->GetIntensity());

	RenderInfo.Color = FVector(std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z));
	RenderInfo.Intensity = Intensity;
	
	bEnabled = AmbientLightComponent->IsEnabled() && Intensity > 0.0f;
}

ELightType FAmbientLightSceneProxy::GetLightType() const
{
	return ELightType::Ambient;
}
