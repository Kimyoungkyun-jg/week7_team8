#pragma once

#include "LightComponentBase.h"

class FLightSceneProxy;

enum class ELightType : uint8
{
	Ambient,
	Point,
	Spot,
	Directional,
};

class ULightComponent : public ULightComponentBase
{
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	REFLECT_START(ULightComponent)
		REFLECT_END()
public:
	virtual ELightType GetLightType() const = 0;

	~ULightComponent() override
	{
		UnregisterComponent();
	};

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;
private:
};