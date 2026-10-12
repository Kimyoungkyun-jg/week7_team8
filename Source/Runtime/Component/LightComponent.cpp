#include "EnginePCH.h"
#include "LightComponent.h"
#include "Engine/World.h"

void ULightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
	GetWorld()-> GetScene().RemoveLight(this);
}
