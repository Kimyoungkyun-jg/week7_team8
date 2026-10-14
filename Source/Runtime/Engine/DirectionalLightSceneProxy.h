#pragma once

#include "LightSceneProxy.h"

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:	
	FDirectionalLightSceneProxy(ULightComponent* Component) : FLightSceneProxy(Component) {}

	virtual void Update() override;
	virtual ELightType GetLightType() const;
	const FDirectionalLightInfo GetRenderInfo() const { return RenderInfo; }

private:
	FDirectionalLightInfo RenderInfo = {};
};