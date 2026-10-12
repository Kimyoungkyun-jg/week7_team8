#include "EnginePCH.h"
#include "Engine/LightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"

#include <algorithm>

void FLightSceneProxy::Update()
{
	if(!UpdateCommonState())
		return;

	switch (LightType)
	{
		case ELightType::Ambient:
		{
			break;
		}
		

		case ELightType::Directional:
		{
			const auto* Light = static_cast<UDirectionalLightComponent*>(Component);
			FTransform WorldTransform;
			WorldTransform.Rotation = Light->GetWorldRotation();
			const FVector Direction = WorldTransform.GetForward().Normalized();
			DirectionalData.ColorIntensity = ColorIntensity;
			DirectionalData.Direction = FVector4(Direction, 0.0f);
			bEnabled = Component->IsEnabled() && ColorIntensity.W > 0.0f;
			break;
		}

		case ELightType::Spot:
		{
			const auto* Spot = static_cast<const USpotLightComponent*>(Component);
			FTransform WorldTransform;
			WorldTransform.Rotation = Spot->GetWorldRotation();

			const FVector Direction = WorldTransform.GetForward().Normalized();
			const FVector Position = Spot->GetWorldLocation();
			const float Radius = std::max(0.0f, Spot->GetAttenuationRadius());

			const float OuterAngle =
				FMath::Clamp(Spot->GetOuterConeAngle(), 0.0f, 89.0f);
			const float InnerAngle =
				FMath::Clamp(Spot->GetInnerConeAngle(), 0.0f, OuterAngle);

			const float OuterCos =
				std::cos(FMath::DegreesToRadians(OuterAngle));
			const float InnerCos =
				std::cos(FMath::DegreesToRadians(InnerAngle));

			SpotData.PositionRadius = FVector4(Position, Radius);
			SpotData.ColorIntensity = ColorIntensity;
			SpotData.DirectionInnerOuterCos = FVector4(Direction, OuterCos);
			SpotData.InnerCosFalloff = FVector4(InnerCos, std::max(0.0f, Spot->GetFalloff()), 0.0f, 0.0f);
			bEnabled = bEnabled && Radius > 0.0f;
			break;
		}

		case ELightType::Point:
		{
			const auto* Point = static_cast<UPointLightComponent*>(Component);
			const FVector Position = Component->GetWorldLocation();
			const float Radius = std::max(0.0f, Point->GetAttenuationRadius());
			PointData.ColorIntensity = ColorIntensity;
			PointData.FalloffPadding.X = std::max(0.0f, Point->GetFalloff());
			PointData.PositionRadius = FVector4(Position, Radius);
			bEnabled = bEnabled && Radius > 0.0f;
			break;
		}

	}
	
}

bool FLightSceneProxy::UpdateCommonState()
{
	if (!Component)
	{
		bEnabled = false;
		return false;
	}

	bVisible = Component->IsVisible();
	bHiddenInGame = Component->IsHiddenInGame();
	const FVector4& Color = Component->GetLightColor();
	const float Intensity = std::max(0.0f, Component->GetIntensity());
	ColorIntensity = FVector4(
		std::max(0.0f, Color.X), std::max(0.0f, Color.Y), std::max(0.0f, Color.Z), Intensity);
	
	bEnabled = Component->IsEnabled() && Intensity > 0.0f;

	return true;
}
