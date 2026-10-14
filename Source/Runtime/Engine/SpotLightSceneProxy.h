#pragma once

#include "LightSceneProxy.h"

class FSpotLightSceneProxy : public FLightSceneProxy
{
public:
	FSpotLightSceneProxy(ULightComponent* Component) : FLightSceneProxy(Component) {}

	virtual void Update() override;
	virtual ELightType GetLightType() const override;
	const FSpotLightInfo GetRenderInfo() const { return RenderInfo; }

private:
	FSpotLightInfo RenderInfo = {};
};