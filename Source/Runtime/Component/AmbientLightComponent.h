#pragma once

#include "LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	REFLECT_START(UAmbientLightComponent)
		REFLECT_END()
public:
	UAmbientLightComponent() = default;
	~UAmbientLightComponent() override = default;

	ELightType GetLightType() const override { return ELightType::Ambient; }
private:
};