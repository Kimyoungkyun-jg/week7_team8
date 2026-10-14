#include "EnginePCH.h"
#include "SpotLightSceneProxy.h"
#include "Component/SpotLightComponent.h"

void FSpotLightSceneProxy::Update()
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Component);
	if (!SpotLightComponent)
	{
		return;
	}

	bVisible = SpotLightComponent->IsVisible();
	bHiddenInGame = SpotLightComponent->IsHiddenInGame();

	const FVector Position = SpotLightComponent->GetWorldLocation();
	const FVector4& Color = SpotLightComponent->GetLightColor();
	const FVector Direction = SpotLightComponent->GetForwardVector();
	const float Radius = std::max(0.0f, SpotLightComponent->GetAttenuationRadius());
	const float Intensity = std::max(0.0f, SpotLightComponent->GetIntensity());
	const float OuterAngle = std::max(0.0f, SpotLightComponent->GetOuterConeAngle());
	const float InnerAngle = std::max(0.0f, SpotLightComponent->GetInnerConeAngle());

	RenderInfo.Position = Position;
	RenderInfo.Color = FVector(std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z));
	RenderInfo.Direction = Direction.Normalized();
	RenderInfo.Range = Radius;
	RenderInfo.Intensity = Intensity;
	RenderInfo.Angle = FVector2(InnerAngle, OuterAngle);

	bEnabled = SpotLightComponent->IsEnabled() && Radius > 0.0f && Intensity > 0.0f;
}

ELightType FSpotLightSceneProxy::GetLightType() const
{
	return ELightType::Spot;
}
