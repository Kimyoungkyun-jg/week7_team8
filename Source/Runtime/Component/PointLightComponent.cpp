#include "EnginePCH.h"
#include "Component/PointLightComponent.h"
#include "Engine/World.h"

void UPointLightComponent::CreateRenderState()
{
    GetWorld()->GetScene().AddLight(this);
}

void UPointLightComponent::DestroyRenderState()
{
    GetWorld()->GetScene().RemoveLight(this);
}
