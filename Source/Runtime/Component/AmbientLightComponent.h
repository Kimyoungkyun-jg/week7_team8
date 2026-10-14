#pragma once

#include "Component/LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	REFLECT_START(UAmbientLightComponent)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	UAmbientLightComponent() { SetIntensity(1.0f); }
	~UAmbientLightComponent() override { UnregisterComponent(); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	bool bEnabled = true;
};
