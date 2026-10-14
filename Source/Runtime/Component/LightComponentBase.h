#pragma once

#include "SceneComponent.h"

class ULightComponentBase : public USceneComponent
{
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	REFLECT_START(ULightComponentBase)
		PROPERTY_TYPE(LightColor, Color)
		PROPERTY(Intensity)
		PROPERTY(bEnabled)
		REFLECT_END()

public:
	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; OnPropertyChanged("LightColor"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; OnPropertyChanged("Intensity"); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:	
	float Brightness = 1.0f;
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	bool bEnabled = true;
};	