#pragma once

#include "Core/Types.h"
#include "Render/SceneView.h"
#include "Render/RenderPacket.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneRenderOptions.h"
#include "Render/RenderGraph/RenderGraph.h"

class UWorld;
class FScene;
class FRenderer;
class FConstantBuffer;
class FSkyboxRenderer;
struct FSceneViewTargets;

// 씬 렌더링 조율자
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData);
	~FSceneRenderer() = default;
	FSceneRenderer(const FSceneRenderer&) = delete;
	FSceneRenderer& operator=(const FSceneRenderer&) = delete;

	// 가시성 검사 및 렌더 큐 수집
	void InitViews(FRenderer* Renderer, const FAmbientLightRenderData& AmbientLight, bool bUseSkyLight = true);

	// 뷰 결과를 하나 유지하는 벤치마크에서도 사용한다.
	static void GatherRenderPackets(FScene& Scene, FSceneRenderData& Data, const FViewContext& View,
		const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);
	static void PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data);
	const FRenderStats& GetRenderStats() const { return RenderData.RenderStats; }

	// 한 뷰의 핵심 패스(GBuffer/Forward, 조명, 하늘, 톤매핑)를 그래프에 등록한다.
	bool AddCorePasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox);
	bool DidCorePassSucceed() const { return bCorePassSucceeded; }
	FRenderGraphTextureRef GetNormalRoughnessRef() const { return NormalRoughnessRef; }

	// 반투명 렌더링
	void RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants = nullptr);

	// 전체 렌더 큐 조회
	FRenderQueue& GetRenderQueue() { return RenderData.RenderQueue; }
	const FRenderQueue& GetRenderQueue() const { return RenderData.RenderQueue; }

	// 뷰 정보 조회
	const FSceneView& GetView() const { return View; }
	
private:
	bool IntersectSphereFrustum(const FVector4& PositionRadius, const FPlane (&Planes)[6]) const;

	UWorld* World = nullptr;
	FSceneView View;
	FSceneRenderData& RenderData;
	FViewContext CachedViewContext;
	FRenderGraphTextureRef NormalRoughnessRef;
	bool bCorePassSucceeded = true;
};
