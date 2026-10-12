#pragma once

#include "Math/Vector4.h"
#include "Core/EngineString.h"

#include <cstddef>

// UberLit.hlsl의 FAmbientLightInfo와 같은 배치다.
struct FAmbientLightRenderData
{
	FVector4 ColorIntensity;
};

// Sky Light 설정의 씬 스냅샷. SourcePath로 캐시된 irradiance 큐브맵을 찾는다.
struct FSkyLightRenderData
{
	FVector4 ColorIntensity;
	FString SourcePath;
};

// 방향광 씬 프록시에서 복사한 원본 데이터다.
struct FDirectionalLightRenderData
{
	FVector4 DirectionIntensity;
	FVector4 Color;
};

// 포인트 라이트 씬 프록시에서 복사한 원본 데이터다.
struct FPointLightRenderData
{
	FVector4 PositionRadius;
	FVector4 ColorIntensity;
	FVector4 Falloff;
};

// 스포트라이트 씬 프록시에서 복사한 원본 데이터다.
struct FSpotLightRenderData
{
	FVector4 PositionRadius;
	FVector4 DirectionOuterCos;
	FVector4 ColorIntensity;
	FVector4 InnerCosFalloff;
};

// Forward와 Deferred가 공유하는 구조화 버퍼 원소다. 라이트를 종류별로 연속 배치하고,
// 패스 상수의 개수와 시작 위치로 각 종류의 구간을 찾는다.
struct FSceneLightRenderInfo
{
	FVector4 PositionRadius;    // 방향광에서는 xyz가 빛의 진행 방향
	FVector4 DirectionOuterCos; // 스포트라이트 전용
	FVector4 ColorIntensity;    // rgb: 색상, w: 광도
	FVector4 Parameters;        // 포인트: x 거리 감쇠, 스포트: x 안쪽 각도의 코사인, y 거리 감쇠
};

// UberLit.hlsl의 b1 조명 상수 버퍼와 같은 배치다.
struct FForwardLightingConstants
{
	FAmbientLightRenderData Ambient;
	FVector4 CameraPosition;
	uint32 LightCounts[4];  // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수
	uint32 LightOffsets[4]; // 공유 조명 버퍼에서 각 종류가 시작되는 위치, 순서는 위와 같음
	FVector4 SkyColorIntensity;
};

static_assert(sizeof(FVector4) == 16);
static_assert(sizeof(FAmbientLightRenderData) == 16);
static_assert(sizeof(FDirectionalLightRenderData) == 32);
static_assert(sizeof(FPointLightRenderData) == 48);
static_assert(sizeof(FSpotLightRenderData) == 64);
static_assert(sizeof(FSceneLightRenderInfo) == 64);
static_assert(offsetof(FPointLightRenderData, Falloff) == 32);
static_assert(offsetof(FForwardLightingConstants, CameraPosition) == 16);
static_assert(offsetof(FForwardLightingConstants, LightCounts) == 32);
static_assert(offsetof(FForwardLightingConstants, LightOffsets) == 48);
static_assert(offsetof(FForwardLightingConstants, SkyColorIntensity) == 64);
static_assert(sizeof(FForwardLightingConstants) == 80);
