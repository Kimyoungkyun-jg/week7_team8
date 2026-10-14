#pragma once

#include "LightSceneProxy.h"

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
	FPointLightSceneProxy(ULightComponent* Component) : FLightSceneProxy(Component) {}

	virtual void Update() override;
	virtual ELightType GetLightType() const override;
	const FPointLightInfo GetRenderInfo() const { return RenderInfo; }

private:
	FPointLightInfo RenderInfo = {};
};