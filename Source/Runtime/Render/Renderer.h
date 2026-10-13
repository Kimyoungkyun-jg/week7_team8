#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "MeshPassProcessor.h"
#include <span>

class FRenderer
{
public:
	bool Init();

	// 패스의 의미를 해석하지 않고 준비된 GPU 상태와 드로우만 제출한다.
	void UploadViewMatrix(const FMatrix& ViewProjection);
	void DrawMeshes(std::span<const FMeshDrawBatch> Batches);
	void DrawFullscreen(const FPipelineState& Pipeline,
		std::span<FTexture2D* const> Textures);
	void BindPixelConstant(uint32 Slot, FConstantBuffer* Buffer);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 측정 대상 드로우를 동일한 GPU 상태로 다시 제출한다.
	FOcclusionMeasureResult MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

private:
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void BindBatch(const FMeshDrawBatch& Batch);
	void BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item);
	TUniquePtr<FConstantBuffer> ObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> MaterialCB;
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;
	FGPUOcclusion GPUOcclusion;
};
