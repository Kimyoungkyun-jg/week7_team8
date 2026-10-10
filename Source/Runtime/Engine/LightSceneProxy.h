#pragma once

#include "Render/LightRenderData.h"

class ULightComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class USpotLightComponent;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(ULightComponent* InComponent) : Component(InComponent) {}
	virtual ~FLightSceneProxy() = default;
	virtual void Update() = 0;
	ULightComponent* GetComponent() const { return Component; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }

protected:
	void UpdateCommon(bool bComponentEnabled);
	ULightComponent* Component = nullptr;
	bool bEnabled = false;

private:
	bool bVisible = true;
	bool bHiddenInGame = false;
};

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FPointLightSceneProxy(UPointLightComponent* InComponent);
	void Update() override;
	const FPointLightRenderData& GetRenderData() const { return RenderData; }

private:
	FPointLightRenderData RenderData{};
};

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
	void Update() override;
	const FDirectionalLightRenderData& GetRenderData() const { return RenderData; }

private:
	FDirectionalLightRenderData RenderData{};
};

class FSpotLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FSpotLightSceneProxy(USpotLightComponent* InComponent);
	void Update() override;
	const FSpotLightRenderData& GetRenderData() const { return RenderData; }

private:
	FSpotLightRenderData RenderData{};
};
