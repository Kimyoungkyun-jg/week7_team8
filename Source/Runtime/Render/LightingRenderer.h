#pragma once

#include "Render/LightRenderData.h"
#include "Render/RenderEnums.h"
#include "Render/ForwardLightCluster.h"
#include "Render/Buffer.h"
#include "Render/SkyLightEnvironment.h"
#include <unordered_map>

class FRenderQueue;
class FTexture2D;
struct FSceneView;
struct FSceneViewTargets;

struct FDeferredLightConstants
{
	FMatrix InverseViewProjection;
	FVector4 CameraPosition;
	FVector4 CameraForward;
	FVector4 ViewportPass;
	FAmbientLightRenderData Ambient;
	uint32 LightCounts[4];
	uint32 LightOffsets[4];
	FVector4 SkyColorIntensity;
};

// 조명 데이터 업로드, SkyLight 캐시, Forward 클러스터와 Deferred 조명 패스를 소유한다.
class FLightingRenderer
{
public:
	bool Init();
	void PrepareSkyLight(const FSkyLightRenderData& SkyLight);
	const FSkyLightEnvironment* FindSkyLightEnvironment(const FString& SourcePath) const;
	bool BeginForwardLighting(const FSceneView& View,
		const FAmbientLightRenderData& AmbientLight,
		const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights, ELightingModel LightingModel,
		bool bEnableClusteredLightCulling, const FSkyLightRenderData* SkyLight);
	void EndForwardLighting();
	bool BeginDeferredLighting(const FSceneView& View, uint32 ViewportWidth, uint32 ViewportHeight,
		const FAmbientLightRenderData& AmbientLight,
		const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights, const FSkyLightRenderData* SkyLight);
	void PrepareAdditiveDeferredLighting();
	void EndDeferredLighting();

private:
	bool UpdateSceneLights(const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights,
		uint32 (&Counts)[4], uint32 (&Offsets)[4]);
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	FDeferredLightConstants DeferredConstants{};
	FForwardLightClusterGrid ForwardClusterGrid;
	std::unordered_map<FString, TUniquePtr<FSkyLightEnvironment>> Environments;
};
