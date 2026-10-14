#pragma once

#include "Component/LightComponent.h"

class FLineBatcher;

class USpotLightComponent : public ULightComponent
{
	DECLARE_CLASS(USpotLightComponent, ULightComponent)

	REFLECT_START(ClassName)
		PROPERTY(InnerConeAngle)
		PROPERTY(OuterConeAngle)
		PROPERTY(AttenuationRadius)
		PROPERTY(Falloff)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	USpotLightComponent()
	{
		SetIntensity(1.0f);
		SetLightColor(FVector4(1.0f, 1.0f, 0.6f, 1.0f));
	}
	virtual ~USpotLightComponent() override { UnregisterComponent(); }

	void DrawDebug(FLineBatcher* LineBatcher) const;

	float GetInnerConeAngle() const { return InnerConeAngle; }
	void SetInnerConeAngle(float InAngle) { InnerConeAngle = InAngle; }

	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetOuterConeAngle(float InAngle) { OuterConeAngle = InAngle; }

	float GetFalloff() const { return Falloff; }
	void SetFalloff(float InFalloff) { Falloff = InFalloff; }

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }

protected:
	const char* GetEditorIconPath() const override { return "Editor/Icon/SpotLight_64x.png"; }
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	// 원뿔 중심축 기준 반각(도). 
	float InnerConeAngle = 15.0f;
	float OuterConeAngle = 30.0f;

	// 빛이 닿는 거리
	float AttenuationRadius = 5.0f;

	// Inner에서 Outer로 가며 밝기가 떨어지는 정도.
	float Falloff = 1.0f;

	bool bEnabled = true;
};
