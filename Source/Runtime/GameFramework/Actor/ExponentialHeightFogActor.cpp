#include "EnginePCH.h"
#include "ExponentialHeightFogActor.h"

AExponentialHeightFogActor::AExponentialHeightFogActor()
{
	ExponentialHeightFogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>("UExponentialHeightFogComponent");
	SetRootComponent(ExponentialHeightFogComponent);
}

AExponentialHeightFogActor::~AExponentialHeightFogActor()
{
}

