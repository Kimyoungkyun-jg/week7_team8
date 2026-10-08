#pragma once

#include "Component/LightComponent.h"
#include "Math/Vector4.h"

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY_TYPE(LightColor, Color)
		PROPERTY(Intensity)
		PROPERTY(AttenuationRadius)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	~UPointLightComponent() override { UnregisterComponent(); }
	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; OnPropertyChanged("LightColor"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; OnPropertyChanged("Intensity"); }
	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; OnPropertyChanged("AttenuationRadius"); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	FVector4 LightColor{1.0f, 1.0f, 1.0f, 1.0f};
	float Intensity = 8.0f;
	float AttenuationRadius = 10.0f;
	bool bEnabled = true;
};
