#include "EnginePCH.h"
#include "GameFramework/Actor/DirectionalLight.h"
#include "Asset/AssetManager.h"
#include "Component/StaticMeshComponent.h"
#include <cmath>

ADirectionalLight::ADirectionalLight()
{
	SetLightIcon("Editor/Icon/DirectionalLight_64x.png");
	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("UDirectionalLightComponent");
	DirectionalLightComponent->SetupAttachment(GetRootComponent());

	DirectionArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>("DirectionArrow");
	DirectionArrowComponent->SetupAttachment(DirectionalLightComponent);
	DirectionArrowComponent->SetStaticMesh(UAssetManager::GetMesh("Arrow"));
	DirectionArrowComponent->SetHiddenInGame(true);
	DirectionalLightComponent->OnDirectionChanged.AddRaw(this, &ADirectionalLight::UpdateDirectionArrow);
	UpdateDirectionArrow();
}

void ADirectionalLight::UpdateDirectionArrow()
{
	const FMatrix LightWorld = DirectionalLightComponent->GetWorldMatrix();
	FVector LocalDirection = DirectionalLightComponent->GetDirection();
	// 부모의 비균일 스케일에도 메시의 +Z가 실제 월드 조명 방향을 가리키게 한다.
	if (std::abs(LightWorld.Determinant()) > 1.0e-8f)
		LocalDirection = LightWorld.Inverse().TransformVector(DirectionalLightComponent->GetWorldDirection());
	if (LocalDirection.Size() < 1.0e-5f)
		LocalDirection = FVector(1.0f, 0.0f, 0.0f);

	const float Horizontal = std::sqrt(LocalDirection.X * LocalDirection.X + LocalDirection.Y * LocalDirection.Y);
	DirectionArrowComponent->SetRelativeRotation(FRotator(
		FMath::RadiansToDegrees(std::atan2(Horizontal, LocalDirection.Z)),
		FMath::RadiansToDegrees(std::atan2(LocalDirection.Y, LocalDirection.X)), 0.0f));
}
