#include "EnginePCH.h"
#include "Component/AmbientLightComponent.h"
#include "Engine/World.h"

void UAmbientLightComponent::CreateRenderState()
{
	GetWorld()->GetScene().AddAmbientLight(this);
}

void UAmbientLightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveAmbientLight(this);
}
