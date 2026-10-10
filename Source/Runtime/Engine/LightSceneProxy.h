#pragma once

#include "Render/LightRenderData.h"

class UPointLightComponent;
class UDirectionalLightComponent;

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

class FDirectionalLightSceneProxy
{
public:
	explicit FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
		: Component(InComponent) { Update(); }
	void Update();
	UDirectionalLightComponent* GetComponent() const { return Component; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	const FDirectionalLightRenderData& GetRenderData() const { return RenderData; }

private:
	UDirectionalLightComponent* Component = nullptr;
	FDirectionalLightRenderData RenderData{};
	bool bEnabled = false;
	bool bVisible = true;
	bool bHiddenInGame = false;
};
