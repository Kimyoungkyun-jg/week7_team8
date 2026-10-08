#pragma once

#include "Math/Vector4.h"

class UPointLightComponent;

struct FPointLightRenderData
{
	FVector4 PositionRadius;
	FVector4 ColorIntensity;
};

inline constexpr uint32 MaxDeferredPointLights = 32;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(UPointLightComponent* InComponent) : Component(InComponent) { Update(); }
	void Update();
	UPointLightComponent* GetComponent() const { return Component; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	const FPointLightRenderData& GetRenderData() const { return RenderData; }

private:
	UPointLightComponent* Component = nullptr;
	FPointLightRenderData RenderData{};
	bool bEnabled = false;
	bool bVisible = true;
	bool bHiddenInGame = false;
};
