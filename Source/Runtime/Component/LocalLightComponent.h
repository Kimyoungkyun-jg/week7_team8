#pragma once

#pragma once
#pragma once

#include "LightComponent.h"

class ULocalLightComponent : public ULightComponent
{
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)
	REFLECT_START(ULocalLightComponent)
		PROPERTY(AttenuationRadius)
		REFLECT_END()
public:
	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; OnPropertyChanged("AttenuationRadius"); }

private:
	float AttenuationRadius = 10.0f;
};