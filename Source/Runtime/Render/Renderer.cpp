#include "EnginePCH.h"
#include "Renderer.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "SceneViewTargets.h"

bool FRenderer::Init()
{
	if (!MeshDrawer.Init() || !Lighting.Init()) return false;
	GPUOcclusion.Init();
	return true;
}

void FRenderer::PrepareSkyLight(const FSkyLightRenderData& SkyLight)
{
	Lighting.PrepareSkyLight(SkyLight);
}

const FSkyLightEnvironment* FRenderer::GetSkyLightEnvironment(const FString& SourcePath) const
{
	return Lighting.FindSkyLightEnvironment(SourcePath);
}

void FRenderer::SetupView(const FSceneView& View)
{
	MeshDrawer.SetupView(View);
}

void FRenderer::RenderDeferredOpaque(const FSceneView& View, const FRenderQueue& InQueue, bool bGBufferPass)
{
	MeshDrawer.RenderDeferredOpaque(View, InQueue, bGBufferPass);
}

void FRenderer::RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue)
{
	MeshDrawer.RenderTranslucent(View, InQueue);
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion(
	const FMatrix& ViewProjection, const FRenderQueue& InQueue)
{
	return MeshDrawer.MeasureOpaqueOcclusion(ViewProjection, InQueue);
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	return MeshDrawer.BeginObjectConstants(MaxSlots);
}

void FRenderer::EndObjectConstants()
{
	MeshDrawer.EndObjectConstants();
}

// 전체 렌더 큐 렌더링
void FRenderer::RenderAll(const FSceneView& View, const FRenderQueue& InQueue)
{
	RenderDeferredOpaque(View, InQueue);
	RenderTranslucent(View, InQueue);
}

bool FRenderer::RenderForwardOpaque(const FSceneView& View, const FRenderQueue& InQueue,
	const FAmbientLightRenderData& AmbientLight,
	const TArray<FDirectionalLightRenderData>& DirectionalLights,
	const TArray<FPointLightRenderData>& PointLights,
	const TArray<FSpotLightRenderData>& SpotLights, ELightingModel LightingModel,
	bool bEnableClusteredLightCulling, const FSkyLightRenderData* SkyLight)
{
	return Lighting.RenderForwardOpaque(View, InQueue, AmbientLight, DirectionalLights,
		PointLights, SpotLights, LightingModel, bEnableClusteredLightCulling, SkyLight,
		MeshDrawer);
}

void FRenderer::DrawDeferredLighting(const FSceneView& View, const FSceneViewTargets& Targets,
	const FAmbientLightRenderData& AmbientLight,
	const TArray<FDirectionalLightRenderData>& DirectionalLights,
	const TArray<FPointLightRenderData>& PointLights,
	const TArray<FSpotLightRenderData>& SpotLights, const FSkyLightRenderData* SkyLight)
{
	Lighting.DrawDeferredLighting(View, Targets, AmbientLight, DirectionalLights, PointLights,
		SpotLights, SkyLight);
}

void FRenderer::DrawToneMap(const FSceneViewTargets& Targets)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::ToneMap);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader)
		return;

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, Targets.LightingHDR.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Targets.Depth.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);

	RenderCommand::UnbindShaderResources(0, 2, EShaderBindFlagBits::Pixel);
}
