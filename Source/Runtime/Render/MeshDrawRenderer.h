#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Render/SceneView.h"
#include "Render/Buffer.h"

class FForwardLightClusterGrid;
struct FPipelineState;

// 최종 깊이 버퍼에 실제로 남은 메시를 측정하는 디버그 결과.
struct FOcclusionMeasureResult
{
	bool bValid = false;
	uint32 TotalObjects = 0;
	uint32 VisibleObjects = 0;
	uint32 TotalDraws = 0;
	uint32 VisibleDraws = 0;
	uint64 TotalTriangles = 0;
	uint64 VisibleTriangles = 0;
	double ElapsedMs = 0.0;
	uint32 GPUOccludedDraws = 0;
	uint32 FalseCulls = 0;
};

// 메시 제출, 머티리얼 바인딩, 오브젝트 상수 버퍼를 소유한다.
class FMeshDrawRenderer
{
public:
	bool Init();
	bool IsForwardReady() const;
	void SetupView(const FSceneView& View);
	void RenderDeferredOpaque(const FSceneView& View, const FRenderQueue& InQueue, bool bGBufferPass);
	void RenderForwardOpaque(const FSceneView& View, const FRenderQueue& InQueue,
		const FPipelineState& PSO, FConstantBuffer* LightCB,
		FStructuredBuffer* SceneLights, FForwardLightClusterGrid& Clusters);
	void RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue);
	void ResetStaticDrawGroups() { StaticGroups.clear(); }
	void AddStaticDrawGroup(const FStaticDrawGroup* Group) { StaticGroups.push_back(Group); }
	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();
	FOcclusionMeasureResult MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue);

private:
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End,
		const FMatrix& ViewProjection, bool bGBufferPass = false);
	void DrawStaticGroups(bool bGBufferPass = false);
	void SortStaticGroups();
	void DrawForwardStaticGroups(const FSceneView& View, const FPipelineState& PSO);
	void DrawForwardPackets(const FSceneView& View, const FRenderQueue& InQueue, const FPipelineState& PSO);
	void BindForwardMaterial(UMaterial* Material, const FSceneView& View, const FPipelineState& PSO);
	void DrawForwardObject(const FSceneView& View, const FMatrix& World, uint32 IndexCount, uint32 StartIndex);
	void BindMaterial(UMaterial* Material, bool bInWireframe = false, bool bGBufferPass = false);
	void UpdateMaterialParams(const FRenderPacket& RenderPacket);
	void UpdatePerObjectConstants(const FMatrix& World);
	void UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection);

	TUniquePtr<FConstantBuffer> PerObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> ForwardObjectCB;
	TUniquePtr<FConstantBuffer> ForwardMaterialCB;
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	bool bCurrentWireframe = false;
	std::vector<const FStaticDrawGroup*> StaticGroups;

	// Forward 패스 동안만 빌려 쓰는 조명 자원.
	FConstantBuffer* ActiveForwardLightCB = nullptr;
	FStructuredBuffer* ActiveSceneLightBuffer = nullptr;
	FForwardLightClusterGrid* ActiveForwardClusters = nullptr;

	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;
};
