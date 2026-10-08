#include "EnginePCH.h"
#include "SpinActor.h"

ASpinActor::ASpinActor()
{
	PrimaryActorTick.bCanEverTick = true;
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
	SetRootComponent(StaticMeshComponent);
}

void ASpinActor::BeginPlay()
{
	Super::BeginPlay();

	rotator = GetActorRotation();
}

void ASpinActor::Tick(float DeltaTime)
{
	rotator.Yaw += 50.0f * DeltaTime;
	if (StaticMeshComponent)
	{
		StaticMeshComponent->SetRelativeRotation(rotator);
	}
}

void ASpinActor::SetPrimitiveType(EPrimitiveType Type)
{
}
