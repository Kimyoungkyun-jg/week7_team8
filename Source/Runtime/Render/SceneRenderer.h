#pragma once

#include "Core/Types.h"
#include "Render/SceneView.h"
#include "Render/RenderPacket.h"
#include "Render/SceneRenderData.h"

class UWorld;
class FScene;
class FRenderer;
class FConstantBuffer;
struct FDeferredViewTargets;

// 씬 렌더링 조율자
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData);
	~FSceneRenderer() = default;
	FSceneRenderer(const FSceneRenderer&) = delete;
	FSceneRenderer& operator=(const FSceneRenderer&) = delete;

	// 가시성 검사 및 렌더 큐 수집
	void InitViews(FRenderer* Renderer);

	// Also used by Benchmark, which owns one persistent view result.
	static void GatherRenderPackets(FScene& Scene, FSceneRenderData& Data, const FViewContext& View,
		const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);
	static void PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data);
	const FRenderStats& GetRenderStats() const { return RenderData.RenderStats; }

	// 불투명 장면은 GBuffer 패스로 렌더링한다.
	bool RenderGBuffer(FRenderer* Renderer, const FDeferredViewTargets& Targets, uint32 Width, uint32 Height);
	void RenderDeferredLighting(FRenderer* Renderer, const FDeferredViewTargets& Targets);
	void RenderToneMap(FRenderer* Renderer, const FDeferredViewTargets& Targets);

	// 반투명 렌더링
	void RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants = nullptr);

	// 전체 렌더 큐 조회
	FRenderQueue& GetRenderQueue() { return RenderData.RenderQueue; }
	const FRenderQueue& GetRenderQueue() const { return RenderData.RenderQueue; }

	// 뷰 정보 조회
	const FSceneView& GetView() const { return View; }

private:
	UWorld* World = nullptr;
	FSceneView View;
	FSceneRenderData& RenderData;
	FViewContext CachedViewContext;
};
