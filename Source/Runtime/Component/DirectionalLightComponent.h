#pragma once

#include "Component/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	UDirectionalLightComponent()
	{
		SetIntensity(1.0f);
		SetLightColor(FVector4(0.5f, 0.5f, 0.5f, 1.0f));
	}
	~UDirectionalLightComponent() override { UnregisterComponent(); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	bool bEnabled = true;
};
