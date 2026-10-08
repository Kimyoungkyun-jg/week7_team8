#include "EnginePCH.h"
#include "FireBallComponent.h"

FBox UFireBallComponent::CalcLocalBounds() const
{
    const float SafeRadius = std::max(Radius, 0.0f);
    return FBox{ FVector(-SafeRadius), FVector(SafeRadius) };
}

void UFireBallComponent::OnPropertyChanged(const FString& PropertyName)
{
    Super::OnPropertyChanged(PropertyName);
    if (PropertyName == "Radius") MarkTransformDirty();
}
