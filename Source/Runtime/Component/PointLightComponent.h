#pragma once

#include "Component/LightComponent.h"
#include "Math/Vector4.h"

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(AttenuationRadius)		
	REFLECT_END()

public:
	~UPointLightComponent() override { UnregisterComponent(); }				
	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; OnPropertyChanged("AttenuationRadius"); }	

protected:
	float AttenuationRadius = 10.0f;	
};
