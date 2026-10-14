#include "EnginePCH.h"
#include "LightActor.h"

ALightActor::ALightActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("LightRoot"));
}
