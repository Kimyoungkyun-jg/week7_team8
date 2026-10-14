#pragma once

#include "Render/LightRenderData.h"

class ULightComponentBase;
class UPointLightComponent;
class UDirectionalLightComponent;
class USpotLightComponent;
class USkyLightComponent;
class UAmbientLightComponent;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(ULightComponentBase* InComponent) : Component(InComponent) {}
	virtual ~FLightSceneProxy() = default;
	virtual void Update() = 0;
	ULightComponentBase* GetComponent() const { return Component; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }

protected:
	void UpdateCommon(bool bComponentEnabled);
	ULightComponentBase* Component = nullptr;
	bool bEnabled = false;

private:
	bool bVisible = true;
	bool bHiddenInGame = false;
};

class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
	void Update() override;
	const FAmbientLightRenderData& GetRenderData() const { return RenderData; }

private:
	FAmbientLightRenderData RenderData{};
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

class FSkyLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FSkyLightSceneProxy(USkyLightComponent* InComponent);
	void Update() override;
	const FSkyLightRenderData& GetRenderData() const { return RenderData; }

private:
	FSkyLightRenderData RenderData{};
};
