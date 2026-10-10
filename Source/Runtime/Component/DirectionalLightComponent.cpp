#include "EnginePCH.h"
#include "Component/DirectionalLightComponent.h"
#include "Engine/World.h"

void UDirectionalLightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddDirectionalLight(this);
}

void UDirectionalLightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveDirectionalLight(this);
}
