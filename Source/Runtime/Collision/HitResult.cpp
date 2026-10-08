#include "EnginePCH.h"
#include "HitResult.h"
#include "Component/PrimitiveComponent.h"

AActor* FHitResult::GetActor() const
{
	return HitComponent ? HitComponent->GetOwner() : nullptr;
}
