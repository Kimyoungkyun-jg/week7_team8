#pragma once

#include "SceneComponent.h"
#include "Math/Vector4.h"

class ULightComponentBase : public USceneComponent
{
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	REFLECT_START(ULightComponentBase)
		PROPERTY_TYPE(LightColor, Color)
		PROPERTY(Intensity)
	REFLECT_END()

public:
	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; OnPropertyChanged("LightColor"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; OnPropertyChanged("Intensity"); }

private:
	FVector4 LightColor{1.0f, 1.0f, 1.0f, 1.0f};
	float Intensity = 8.0f;
};
