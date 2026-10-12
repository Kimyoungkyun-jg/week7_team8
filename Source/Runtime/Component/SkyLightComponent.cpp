#include "EnginePCH.h"
#include "Component/SkyLightComponent.h"
#include "Engine/World.h"

void USkyLightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddSkyLight(this);
}

void USkyLightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveSkyLight(this);
}
