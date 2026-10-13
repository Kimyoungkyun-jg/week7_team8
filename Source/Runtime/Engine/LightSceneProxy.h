#pragma once

#include "Math/Vector4.h"
#include "Component/LightComponent.h"

class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;

struct FPointLightRenderData
{
	FVector4 PositionRadius;
	FVector4 ColorIntensity;
	FVector4 FalloffPadding; // X: Falloff, YZW: 패딩
};

struct FSpotLightRenderData
{
	FVector4 PositionRadius; // XYZ: 위치, W: 반경
	FVector4 ColorIntensity; // RGB: 색, W: 강도
	FVector4 DirectionInnerOuterCos; // XYZ: 정규환된 방향, W: cos(Outer)
	FVector4 InnerCosFalloff; // X: cos(Inner), Y: Falloff, ZW: 패딩
};

struct FDirectionalLightRenderData
{
	FVector4 Direction; // XYZ: 정규화된 방향, W: 패딩
	FVector4 ColorIntensity; // RGB: 색, W: 강도
};

inline constexpr uint32 MaxDeferredPointLights = 32;
inline constexpr uint32 MaxForwardPointLights = 32;
inline constexpr uint32 MaxForwardSpotLights = 32;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(ULightComponent* InComponent, ELightType InLightType) : Component(InComponent), LightType(InLightType) { Update(); }
	void Update();
	ULightComponent* GetComponent() const { return Component; }
	ELightType GetLightType() const { return LightType; }
	
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }

	const FDirectionalLightRenderData& GetDirectionalLightData() const { return DirectionalData; }
	const FPointLightRenderData& GetPointLightData() const { return PointData; }
	const FSpotLightRenderData& GetSpotLightData() const { return SpotData; }
	
	const FVector4& GetColorIntensity() const{ return ColorIntensity; }

protected:
	bool UpdateCommonState();

	ULightComponent* Component = nullptr;
	FVector4 ColorIntensity{};

	// LightType에 해당하는 데이터만 갱신하고 읽는다.
	FDirectionalLightRenderData DirectionalData{};
	FPointLightRenderData PointData{};
	FSpotLightRenderData SpotData{};

	bool bEnabled = false;
	bool bVisible = false;
	bool bHiddenInGame = false;

private:
	ELightType LightType;
};
