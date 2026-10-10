#pragma once

#include "LightComponentBase.h"

enum ELightComponentType
{
	LightType_Directional = 0,
	LightType_Point = 1,
	LightType_Spot = 2,
	LightType_Rect = 3,
	LightType_MAX = 4,
	LightType_NumBits = 2
};

static_assert(LightType_MAX <= (1 << LightType_NumBits),
	"LightType_NumBits is not large enough to hold all possible light types");

class ULightComponent : public ULightComponentBase
{
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	REFLECT_START(ULightComponent)
	REFLECT_END()
};
