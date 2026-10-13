#pragma once

#include "Render/LightRenderData.h"
#include "Render/ForwardLightCluster.h"
#include "Render/Buffer.h"
#include "Render/SkyLightEnvironment.h"
#include <unordered_map>

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

// 뷰 사이에서 재사용하는 조명 GPU 리소스와 SkyLight 캐시. 패스나 드로우는 실행하지 않는다.
class FSceneLightResources
{
public:
	bool Init();
	void PrepareSkyLight(const FSkyLightRenderData& SkyLight);
	const FSkyLightEnvironment* FindSkyLightEnvironment(const FString& SourcePath) const;
	FSkyLightEnvironment* FindPendingSkyLightEnvironment(const FString& SourcePath);
	bool UploadViewLights(const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights,
		uint32 (&Counts)[4], uint32 (&Offsets)[4]);
	FStructuredBuffer* GetSceneLights() const { return SceneLightBuffer.get(); }
	FConstantBuffer* GetForwardConstants() const { return ForwardLightCB.get(); }
	FConstantBuffer* GetDeferredConstants() const { return DeferredLightCB.get(); }
	FForwardLightClusterGrid& GetClusters() { return ForwardClusterGrid; }

private:
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	FForwardLightClusterGrid ForwardClusterGrid;
	std::unordered_map<FString, TUniquePtr<FSkyLightEnvironment>> Environments;
};
