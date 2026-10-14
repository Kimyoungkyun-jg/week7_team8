#pragma once

#include "LightSceneProxy.h"

class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
	FAmbientLightSceneProxy(ULightComponent* Component) : FLightSceneProxy(Component) {}

	virtual void Update() override;
	virtual ELightType GetLightType() const;
	const FAmbientLightInfo GetRenderInfo() const { return RenderInfo; }
		
private:
	FAmbientLightInfo RenderInfo = {};
};