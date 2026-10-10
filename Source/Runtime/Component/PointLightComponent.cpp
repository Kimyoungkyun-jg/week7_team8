#include "EnginePCH.h"
#include "Component/PointLightComponent.h"
#include "Engine/World.h"
#include "Render/LineBatcher.h"

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (!LineBatcher || AttenuationRadius <= 0.0f)
		return;

	constexpr int32 CircleSegments = 48;
	const FVector Center = GetWorldLocation();
	const FVector4 Color = GetLightColor();
	const float Radius = AttenuationRadius;

	// 서로 직교하는 세 원으로 구형 감쇠 범위를 표시한다.
	for (int32 Plane = 0; Plane < 3; ++Plane)
	{
		for (int32 Segment = 0; Segment < CircleSegments; ++Segment)
		{
			const float Angle0 = 2.0f * PI * Segment / CircleSegments;
			const float Angle1 = 2.0f * PI * (Segment + 1) / CircleSegments;
			const float Cos0 = std::cosf(Angle0);
			const float Sin0 = std::sinf(Angle0);
			const float Cos1 = std::cosf(Angle1);
			const float Sin1 = std::sinf(Angle1);

			FVector Start;
			FVector End;
			if (Plane == 0) // XY 평면
			{
				Start = Center + FVector(Cos0 * Radius, Sin0 * Radius, 0.0f);
				End = Center + FVector(Cos1 * Radius, Sin1 * Radius, 0.0f);
			}
			else if (Plane == 1) // XZ 평면
			{
				Start = Center + FVector(Cos0 * Radius, 0.0f, Sin0 * Radius);
				End = Center + FVector(Cos1 * Radius, 0.0f, Sin1 * Radius);
			}
			else // YZ 평면
			{
				Start = Center + FVector(0.0f, Cos0 * Radius, Sin0 * Radius);
				End = Center + FVector(0.0f, Cos1 * Radius, Sin1 * Radius);
			}
			LineBatcher->AddLine(Start, End, Color);
		}
	}
}

void UPointLightComponent::CreateRenderState()
{
    GetWorld()->GetScene().AddPointLight(this);
}

void UPointLightComponent::DestroyRenderState()
{
    GetWorld()->GetScene().RemovePointLight(this);
}
