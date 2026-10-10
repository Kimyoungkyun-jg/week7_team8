#pragma once

#include "Math/Vector4.h"

#include <cstddef>

inline constexpr uint32 MaxForwardPointLights = 4;
inline constexpr uint32 MaxForwardSpotLights = 4;
inline constexpr uint32 MaxDirectionalLights = 32;
inline constexpr uint32 MaxDeferredPointLights = 32;

// Matches FAmbientLightInfo in UberLit.hlsl.
struct FAmbientLightRenderData
{
	FVector4 ColorIntensity;
};

// Matches FDirectionalLightInfo in UberLit.hlsl.
struct FDirectionalLightRenderData
{
	FVector4 DirectionIntensity;
	FVector4 Color;
};

// Matches FPointLightInfo in UberLit.hlsl. DeferredLightingShader.hlsl reads
// Falloff.X as its RadiusFallOff scalar at the same byte offset.
struct FPointLightRenderData
{
	FVector4 PositionRadius;
	FVector4 ColorIntensity;
	FVector4 Falloff;
};

// Matches FSpotLightInfo in UberLit.hlsl.
struct FSpotLightRenderData
{
	FVector4 PositionRadius;
	FVector4 DirectionOuterCos;
	FVector4 ColorIntensity;
	FVector4 InnerCosFalloff;
};

// Matches the Lighting constant buffer at register(b1) in UberLit.hlsl.
struct FForwardLightingConstants
{
	FAmbientLightRenderData Ambient;
	FDirectionalLightRenderData DirectionalLights[MaxDirectionalLights];
	FPointLightRenderData PointLights[MaxForwardPointLights];
	FSpotLightRenderData SpotLights[MaxForwardSpotLights];
	FVector4 CameraPosition;
	uint32 LightCounts[4]; // x: point count, y: spot count, z: directional count
};

static_assert(sizeof(FVector4) == 16);
static_assert(sizeof(FAmbientLightRenderData) == 16);
static_assert(sizeof(FDirectionalLightRenderData) == 32);
static_assert(sizeof(FPointLightRenderData) == 48);
static_assert(sizeof(FSpotLightRenderData) == 64);
static_assert(offsetof(FPointLightRenderData, Falloff) == 32);
static_assert(offsetof(FForwardLightingConstants, DirectionalLights) == 16);
static_assert(offsetof(FForwardLightingConstants, PointLights) == 1040);
static_assert(offsetof(FForwardLightingConstants, SpotLights) == 1232);
static_assert(offsetof(FForwardLightingConstants, CameraPosition) == 1488);
static_assert(offsetof(FForwardLightingConstants, LightCounts) == 1504);
static_assert(sizeof(FForwardLightingConstants) == 1520);
