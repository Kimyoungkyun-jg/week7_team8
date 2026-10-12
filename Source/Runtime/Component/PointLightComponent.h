#pragma once

#include "LocalLightComponent.h"
#include "Math/Vector4.h"

class UPointLightComponent : public ULocalLightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULocalLightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(Falloff)
	REFLECT_END()

public:
	UPointLightComponent() = default;
	UPointLightComponent(UPointLightComponent&&) = delete;
	~UPointLightComponent() override = default;
	UPointLightComponent(const UPointLightComponent&) = delete;
	UPointLightComponent& operator=(const UPointLightComponent&) = delete;


	float GetFalloff() const { return Falloff; }
	void SetFalloff(float InFalloff) { Falloff = InFalloff; }

	ELightType GetLightType() const override { return ELightType::Point; }

protected:
/*	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;*/

private:
	float Falloff = 1.0f;

};
