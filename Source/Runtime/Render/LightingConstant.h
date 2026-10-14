#pragma once

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Matrix.h"

#define NUM_POINT_LIGHT 4
#define NUM_SPOT_LIGHT 4

struct FAmbientLightInfo
{
	FVector Color;
	float Intensity;
};

struct FDirectionalLightInfo
{
	FVector Direction;
	float Intensity;
	FVector Color;
	float Pad;
};

struct FPointLightInfo
{
	FVector Position;
	float Intensity;
	FVector Color;
	float Radius;
};

struct FSpotLightInfo
{
	FVector Position;
	float Range;
	FVector Color;
	float Intensity;
	FVector Direction;
	float Pad1;
	FVector2 Angle;
	FVector2 Pad2;
}; 

struct FLightingConstants
{
	FAmbientLightInfo Ambient;
	FDirectionalLightInfo Directional;
	FPointLightInfo PointLights[NUM_POINT_LIGHT];
	FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

struct FViewConstants
{
	FMatrix VP;
	FVector CamPos;
	float Pad;
};

static_assert(sizeof(FAmbientLightInfo) == 16);
static_assert(sizeof(FDirectionalLightInfo) == 32);
static_assert(sizeof(FPointLightInfo) == 32);
static_assert(sizeof(FSpotLightInfo) == 64);
static_assert(sizeof(FLightingConstants) == (16 + 32 + 32 * NUM_POINT_LIGHT + 64 * NUM_SPOT_LIGHT));
static_assert(sizeof(FViewConstants) == 80);