#pragma once

#include "Component/LightComponent.h"

class FLineBatcher;

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(AttenuationRadius)
		PROPERTY_RANGE(RadiusFallOff, 0.0f, 10.0f)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	~UPointLightComponent() override { UnregisterComponent(); }
	void DrawDebug(FLineBatcher* LineBatcher) const;
	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; OnPropertyChanged("AttenuationRadius"); }
	float GetRadiusFallOff() const { return RadiusFallOff; }
	void SetRadiusFallOff(float InFallOff) { RadiusFallOff = InFallOff; OnPropertyChanged("RadiusFallOff"); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:
	const char* GetEditorIconPath() const override { return "Editor/Icon/PointLight_64x.png"; }
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	float AttenuationRadius = 10.0f;
	float RadiusFallOff = 2.0f;
	bool bEnabled = true;
};
