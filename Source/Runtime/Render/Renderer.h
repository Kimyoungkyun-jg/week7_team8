#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "ForwardLightCluster.h"

// 오클루전 컬링의 효과 상한을 재기 위한 측정 결과 (디버그 전용)
struct FOcclusionMeasureResult
{
	bool bValid = false;
	uint32 TotalObjects = 0;      // 불투명 패스에 제출된 물체 수
	uint32 VisibleObjects = 0;    // 최종 깊이 버퍼에 픽셀을 남긴 물체 수
	uint32 TotalDraws = 0;
	uint32 VisibleDraws = 0;
	uint64 TotalTriangles = 0;
	uint64 VisibleTriangles = 0;
	double ElapsedMs = 0.0;       // 측정 자체에 걸린 시간
	// GPU 오클루전 검증
	uint32 GPUOccludedDraws = 0;  // GPU가 가렸다고 판정한 드로우 수
	uint32 FalseCulls = 0;        // 그중 실제로는 픽셀이 보인 드로우 수
};

class FRenderer
{
public:
	bool Init();
	void DrawDeferredLighting(const FSceneView& View, const struct FSceneViewTargets& Targets,
		const FAmbientLightRenderData& AmbientLight, const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights, const TArray<FSpotLightRenderData>& SpotLights);
	void DrawToneMap(const struct FSceneViewTargets& Targets);

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
		bool bEnableClusteredLightCulling);

	// 반투명 요소 렌더링
	void RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 캐시된 스태틱 메시 경로
	void ResetStaticDrawGroups() { StaticGroups.clear(); }
	void AddStaticDrawGroup(const FStaticDrawGroup* Group) { StaticGroups.push_back(Group); }

	// 측정 전용 불투명 오클루전 검사
	FOcclusionMeasureResult MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

private:
	TUniquePtr<FConstantBuffer> PerObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardObjectCB;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> ForwardMaterialCB;
	FForwardLightClusterGrid ForwardClusterGrid;

	// 오브젝트 상수 버퍼
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	bool bCurrentWireframe = false;

	// 측정 전용 자원
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;

	FGPUOcclusion GPUOcclusion;

	void DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End,
		const FMatrix& ViewProjection, bool bGBufferPass = false);

	void DrawStaticGroups(bool bGBufferPass = false);
	void SortStaticGroups();
	void DrawForwardStaticGroups(const FSceneView& View, const FPipelineState& PSO);
	void DrawForwardPackets(const FSceneView& View, const FRenderQueue& InQueue, const FPipelineState& PSO);
	bool SetupForwardPass(const FSceneView& View, const FAmbientLightRenderData& AmbientLight,
		const TArray<FDirectionalLightRenderData>& DirectionalLights,
		const TArray<FPointLightRenderData>& PointLights,
		const TArray<FSpotLightRenderData>& SpotLights,
		ELightingModel LightingModel, FPipelineState& OutPSO, FForwardLightingConstants& OutLighting);
	void BindForwardMaterial(UMaterial* Material, const FSceneView& View, const FPipelineState& PSO);
	void DrawForwardObject(const FSceneView& View, const FMatrix& World, uint32 IndexCount, uint32 StartIndex);

	void UpdatePerObjectConstants(const FMatrix& World);

	// 스태틱 메시 묶음
	std::vector<const FStaticDrawGroup*> StaticGroups;
	// 머티리얼 바인딩
	void BindMaterial(UMaterial* material, bool bInWireframe = false, bool bGBufferPass = false);
	void UpdateMaterialParams(const FRenderPacket& RenderPacket);
	void UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection);
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);


};
