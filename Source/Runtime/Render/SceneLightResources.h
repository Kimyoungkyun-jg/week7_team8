#pragma once

#include "Render/LightRenderData.h"
#include "Render/ForwardLightTiles.h"
#include "Render/Buffer.h"

struct FDeferredLightConstants
{
	FMatrix InverseViewProjection;
	FVector4 CameraPosition;
	FVector4 CameraForward;
	FVector4 ViewportSize;
	FAmbientLightRenderData Ambient;
	uint32 LightCounts[4];
	uint32 LightOffsets[4];
	FVector4 SkyColorIntensity;
};

// 뷰별 조명 GPU 리소스. 패스나 드로우는 실행하지 않는다.
class FSceneLightResources
{
public:
	bool Init();
	bool UploadViewLights(const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights,
		uint32 (&Counts)[4], uint32 (&Offsets)[4]);
	FStructuredBuffer* GetSceneLights() const { return SceneLightBuffer.get(); }
	FConstantBuffer* GetForwardConstants() const { return ForwardLightCB.get(); }
	FConstantBuffer* GetDeferredConstants() const { return DeferredLightCB.get(); }
	FForwardLightTileGrid& GetTiles() { return ForwardTileGrid; }

private:
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	FForwardLightTileGrid ForwardTileGrid;
	bool bInitialized = false;
};
