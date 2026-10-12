#include "EnginePCH.h"
#include "LightingRenderer.h"
#include "MeshDrawRenderer.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "SceneView.h"
#include "SceneViewTargets.h"
#include <algorithm>
#include <cstring>

namespace
{
	constexpr uint32 ForwardSceneLightSlot = 8;
	constexpr uint32 ForwardSkyIrradianceSlot = 11;
	constexpr uint32 DeferredSceneLightSlot = 3;
	constexpr uint32 DeferredSkyIrradianceSlot = 4;
	template <typename T>
	bool UploadLightBuffer(TUniquePtr<FStructuredBuffer>& Buffer, const TArray<T>& Lights)
	{
		if (Lights.IsEmpty()) return true;
		if (static_cast<uint64>(Lights.Num()) > UINT32_MAX / sizeof(T)) return false;

		const uint32 Count = static_cast<uint32>(Lights.Num());
		if (!Buffer || Buffer->GetCapacity() < Count)
		{
			const uint32 PreviousCapacity = Buffer ? Buffer->GetCapacity() : 0;
			const uint32 MaxCapacity = UINT32_MAX / sizeof(T);
			const uint32 DoubledCapacity = PreviousCapacity <= MaxCapacity / 2
				? std::max(1u, PreviousCapacity * 2) : MaxCapacity;
			auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(
				std::max(Count, DoubledCapacity), sizeof(T));
			if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
			Buffer = std::move(NewBuffer);
		}

		void* Destination = RenderCommand::MapWriteDiscard(Buffer.get());
		if (!Destination) return false;
		std::memcpy(Destination, Lights.GetData(), Count * sizeof(T));
		RenderCommand::Unmap(Buffer.get());
		return true;
	}

	struct FDeferredLightConstants
	{
		FMatrix InverseViewProjection;
		FVector4 CameraPosition; // W: 원근 투영 여부
		FVector4 CameraForward;
		FVector4 ViewportPass;
		FAmbientLightRenderData Ambient;
		uint32 LightCounts[4]; // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수
		uint32 LightOffsets[4];
		FVector4 SkyColorIntensity;
	};
	static_assert(sizeof(FPointLightRenderData) == 48);
	static_assert(offsetof(FDeferredLightConstants, ViewportPass) == 96);
	static_assert(offsetof(FDeferredLightConstants, Ambient) == 112);
	static_assert(offsetof(FDeferredLightConstants, LightCounts) == 128);
	static_assert(offsetof(FDeferredLightConstants, LightOffsets) == 144);
	static_assert(offsetof(FDeferredLightConstants, SkyColorIntensity) == 160);
	static_assert(sizeof(FDeferredLightConstants) == 176);
}

bool FLightingRenderer::Init()
{
	ForwardLightCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardLightingConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	if (!ForwardClusterGrid.Init())
		HTR_LOG(Warning, "Forward cluster compute shader unavailable");
	return ForwardLightCB && DeferredLightCB;
}

void FLightingRenderer::PrepareSkyLight(const FSkyLightRenderData& SkyLight)
{
	if (SkyLight.SourcePath.empty() || Environments.contains(SkyLight.SourcePath))
		return;
	auto Environment = MakeUnique<FSkyLightEnvironment>();
	Environment->Build(SkyLight.SourcePath);
	Environments.emplace(SkyLight.SourcePath, std::move(Environment));
}

const FSkyLightEnvironment* FLightingRenderer::FindSkyLightEnvironment(const FString& SourcePath) const
{
	const auto It = Environments.find(SourcePath);
	return It != Environments.end() && It->second->IsValid() ? It->second.get() : nullptr;
}

bool FLightingRenderer::UpdateSceneLights(
	const TArray<FDirectionalLightRenderData>& DirectionalLights,
	const TArray<FPointLightRenderData>& PointLights,
	const TArray<FSpotLightRenderData>& SpotLights,
	uint32 (&Counts)[4], uint32 (&Offsets)[4])
{
	const uint64 DirectionalCount = DirectionalLights.Num();
	const uint64 PointCount = PointLights.Num();
	const uint64 SpotCount = SpotLights.Num();
	const uint64 TotalCount = DirectionalCount + PointCount + SpotCount;
	if (TotalCount > UINT32_MAX / sizeof(FSceneLightRenderInfo)) return false;

	Counts[0] = static_cast<uint32>(PointCount);
	Counts[1] = static_cast<uint32>(SpotCount);
	Counts[2] = static_cast<uint32>(DirectionalCount);
	Offsets[2] = 0;
	Offsets[0] = Counts[2];
	Offsets[1] = Counts[2] + Counts[0];

	TArray<FSceneLightRenderInfo> PackedLights;
	PackedLights.Reserve(static_cast<uint32>(TotalCount));
	for (const FDirectionalLightRenderData& Light : DirectionalLights)
	{
		FSceneLightRenderInfo Info{};
		Info.PositionRadius = Light.DirectionIntensity;
		Info.ColorIntensity = FVector4(Light.Color.X, Light.Color.Y,
			Light.Color.Z, Light.DirectionIntensity.W);
		PackedLights.Add(Info);
	}
	for (const FPointLightRenderData& Light : PointLights)
	{
		FSceneLightRenderInfo Info{};
		Info.PositionRadius = Light.PositionRadius;
		Info.ColorIntensity = Light.ColorIntensity;
		Info.Parameters = Light.Falloff;
		PackedLights.Add(Info);
	}
	for (const FSpotLightRenderData& Light : SpotLights)
	{
		FSceneLightRenderInfo Info{};
		Info.PositionRadius = Light.PositionRadius;
		Info.DirectionOuterCos = Light.DirectionOuterCos;
		Info.ColorIntensity = Light.ColorIntensity;
		Info.Parameters = Light.InnerCosFalloff;
		PackedLights.Add(Info);
	}
	return UploadLightBuffer(SceneLightBuffer, PackedLights);
}

bool FLightingRenderer::RenderForwardOpaque(const FSceneView& View, const FRenderQueue& InQueue,
	const FAmbientLightRenderData& AmbientLight,
	const TArray<FDirectionalLightRenderData>& DirectionalLights,
	const TArray<FPointLightRenderData>& PointLights,
	const TArray<FSpotLightRenderData>& SpotLights, ELightingModel LightingModel,
	bool bEnableClusteredLightCulling, const FSkyLightRenderData* SkyLight, FMeshDrawRenderer& MeshDrawer)
{
	FPipelineState PSO;
	FForwardLightingConstants Lighting{};
	FPipelineState* BasePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Forward);
	FShaderProgram* Program = FRenderResourceManager::GetForwardShaderProgram(LightingModel);
	if (!BasePSO || !Program || !ForwardLightCB || !MeshDrawer.IsForwardReady()) return false;
	PSO = *BasePSO;
	PSO.Shader = Program;
	const bool bUseSkyLight = SkyLight && FindSkyLightEnvironment(SkyLight->SourcePath);
	Lighting.Ambient = bUseSkyLight ? FAmbientLightRenderData{ FVector4(0.0f) } : AmbientLight;
	if (bUseSkyLight) Lighting.SkyColorIntensity = SkyLight->ColorIntensity;
	Lighting.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y, View.ViewLocation.Z, 1.0f);
	if (!UpdateSceneLights(DirectionalLights, PointLights, SpotLights,
		Lighting.LightCounts, Lighting.LightOffsets)) return false;
	RenderCommand::UpdateBufferData(ForwardLightCB.get(), &Lighting, sizeof(Lighting));
	MeshDrawer.SetupView(View);
	const FSkyLightEnvironment* SkyEnvironment = SkyLight
		? FindSkyLightEnvironment(SkyLight->SourcePath) : nullptr;
	RenderCommand::BindShaderResource(ForwardSkyIrradianceSlot,
		SkyEnvironment ? SkyEnvironment->GetIrradianceCube() : nullptr,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(1, ESamplerState::LinearClamp,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	bool bClustersBuilt = false;
	if (bEnableClusteredLightCulling && LightingModel != ELightingModel::Gouraud &&
		(Lighting.LightCounts[0] || Lighting.LightCounts[1]))
		bClustersBuilt = ForwardClusterGrid.Build(View, Lighting, SceneLightBuffer.get());
	ForwardClusterGrid.PreparePixelShader(View, Lighting, bClustersBuilt);

	MeshDrawer.RenderForwardOpaque(View, InQueue, PSO, ForwardLightCB.get(),
		SceneLightBuffer.get(), ForwardClusterGrid);
	ForwardClusterGrid.UnbindPixelShader();
	RenderCommand::UnbindShaderResources(ForwardSceneLightSlot, 1,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::UnbindShaderResources(ForwardSkyIrradianceSlot, 1,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	return true;
}


void FLightingRenderer::DrawDeferredLighting(const FSceneView& View, const FSceneViewTargets& Targets,
	const FAmbientLightRenderData& AmbientLight,
	const TArray<FDirectionalLightRenderData>& DirectionalLights,
	const TArray<FPointLightRenderData>& PointLights,
	const TArray<FSpotLightRenderData>& SpotLights, const FSkyLightRenderData* SkyLight)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::DeferredLighting);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader || !DeferredLightCB)
		return;
	FDeferredLightConstants Constants{};
	Constants.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y,
		View.ViewLocation.Z, View.bIsPerspective ? 1.0f : 0.0f);
	Constants.CameraForward = FVector4(View.ViewForward.X, View.ViewForward.Y, View.ViewForward.Z, 0.0f);
	const FSkyLightEnvironment* SkyEnvironment = SkyLight
		? FindSkyLightEnvironment(SkyLight->SourcePath) : nullptr;
	Constants.Ambient = SkyEnvironment
		? FAmbientLightRenderData{ FVector4(0.0f) } : AmbientLight;
	if (SkyEnvironment)
		Constants.SkyColorIntensity = SkyLight->ColorIntensity;
	if (!UpdateSceneLights(DirectionalLights, PointLights,
		SpotLights, Constants.LightCounts, Constants.LightOffsets))
	{
		HTR_LOG(Warning, "Deferred light buffer upload failed");
		return;
	}
	Constants.ViewportPass = FVector4(
		static_cast<float>(Targets.SceneColor->GetWidth()),
		static_cast<float>(Targets.SceneColor->GetHeight()),
		0.0f, 0.0f);
	RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindConstantBuffer(0, DeferredLightCB.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, Targets.BaseColorMetallic.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Targets.NormalRoughness.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(2, Targets.Depth.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(DeferredSceneLightSlot,
		(PointLights.IsEmpty() && DirectionalLights.IsEmpty() && SpotLights.IsEmpty())
			? nullptr : SceneLightBuffer.get(),
		EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(DeferredSkyIrradianceSlot,
		SkyEnvironment ? SkyEnvironment->GetIrradianceCube() : nullptr,
		EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(1, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	if (!PointLights.IsEmpty() || !SpotLights.IsEmpty())
	{
		FPipelineState* AdditivePSO = FRenderResourceManager::GetPSO(EPSOType::DeferredPointLighting);
		if (AdditivePSO && AdditivePSO->Shader && AdditivePSO->Shader->VertexShader && AdditivePSO->Shader->PixelShader)
		{
			Constants.ViewportPass.W = 1.0f;
			RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));
			RenderCommand::BindPipelineState(*AdditivePSO);
			RenderCommand::Draw(3);
		}
	}

	RenderCommand::UnbindShaderResources(0, 5, EShaderBindFlagBits::Pixel);
}

