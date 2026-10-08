#include "EnginePCH.h"
#include "Pawn.h"

APawn::APawn()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
}