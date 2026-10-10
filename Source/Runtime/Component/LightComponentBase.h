#pragma once

#include "SceneComponent.h"

class ULightComponentBase : public USceneComponent
{
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	REFLECT_START(ULightComponentBase)
		REFLECT_END()

public:
	float Brightness;
	float Intensity;
	FVector4 LightColor;
};