#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "LightingRenderer.h"
#include "MeshDrawRenderer.h"

class FRenderer
{
public:
	bool Init();
	void PrepareSkyLight(const FSkyLightRenderData& SkyLight);
	const FSkyLightEnvironment* GetSkyLightEnvironment(const FString& SourcePath) const;
	void DrawDeferredLighting(const FSceneView& View, FTexture2D* BaseColor, FTexture2D* Normal,
		FTexture2D* Depth, uint32 ViewportWidth, uint32 ViewportHeight,
		const FAmbientLightRenderData& AmbientLight, const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights, const TArray<FSpotLightRenderData>& SpotLights,
		const FSkyLightRenderData* SkyLight = nullptr);
	void DrawToneMap(FTexture2D* LightingHDR, FTexture2D* Depth);

	// 시점 상수 버퍼 및 렌더링 상태 설정
	void SetupView(const FSceneView& View);

	// ObjViewer는 이 Forward 경로를 사용한다. 에디터와 PIE는 SceneRenderer와 GBuffer를 사용한다.
	void RenderAll(const FSceneView& View, const FRenderQueue& InQueue);

	// 불투명 요소 렌더링
	void RenderDeferredOpaque(const FSceneView& View, const FRenderQueue& InQueue, bool bGBufferPass = false);
	bool RenderForwardOpaque(const FSceneView& View, const FRenderQueue& InQueue,
		const FAmbientLightRenderData& AmbientLight, const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights, ELightingModel LightingModel,
		bool bEnableClusteredLightCulling, const FSkyLightRenderData* SkyLight = nullptr);

	// 반투명 요소 렌더링
	void RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 캐시된 스태틱 메시 경로
	void ResetStaticDrawGroups() { MeshDrawer.ResetStaticDrawGroups(); }
	void AddStaticDrawGroup(const FStaticDrawGroup* Group) { MeshDrawer.AddStaticDrawGroup(Group); }

	// 측정 전용 불투명 오클루전 검사
	FOcclusionMeasureResult MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

private:
	FMeshDrawRenderer MeshDrawer;
	FLightingRenderer Lighting;
	FGPUOcclusion GPUOcclusion;
};
