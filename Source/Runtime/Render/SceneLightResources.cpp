#include "EnginePCH.h"
#include "Render/SceneLightResources.h"
#include "Render/RenderCommand.h"
#include <algorithm>
#include <cstring>

namespace
{
	static_assert(sizeof(FPointLightRenderData) == 48);
	static_assert(offsetof(FDeferredLightConstants, ViewportSize) == 96);
	static_assert(offsetof(FDeferredLightConstants, Ambient) == 112);
	static_assert(offsetof(FDeferredLightConstants, LightCounts) == 128);
	static_assert(offsetof(FDeferredLightConstants, LightOffsets) == 144);
	static_assert(offsetof(FDeferredLightConstants, SkyColorIntensity) == 160);
	static_assert(sizeof(FDeferredLightConstants) == 176);
}

bool FSceneLightResources::Init()
{
	if (bInitialized) return true;
	ForwardLightCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardLightingConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	if (!ForwardTileGrid.Init())
		HTR_LOG(Warning, "Forward tile culling compute shader unavailable");
	bInitialized = ForwardLightCB && DeferredLightCB;
	return bInitialized;
}

bool FSceneLightResources::UploadViewLights(
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
	Counts[3] = 0;
	Offsets[2] = 0;
	Offsets[0] = Counts[2];
	Offsets[1] = Counts[2] + Counts[0];
	Offsets[3] = 0;
	if (!TotalCount) return true;

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
	const uint32 Count = static_cast<uint32>(PackedLights.Num());
	if (!SceneLightBuffer || SceneLightBuffer->GetCapacity() < Count)
	{
		const uint32 PreviousCapacity = SceneLightBuffer ? SceneLightBuffer->GetCapacity() : 0;
		const uint32 Maximum = UINT32_MAX / sizeof(FSceneLightRenderInfo);
		const uint32 Doubled = PreviousCapacity <= Maximum / 2
			? std::max(1u, PreviousCapacity * 2) : Maximum;
		auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(
			std::max(Count, Doubled), sizeof(FSceneLightRenderInfo));
		if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
		SceneLightBuffer = std::move(NewBuffer);
	}
	void* Destination = RenderCommand::MapWriteDiscard(SceneLightBuffer.get());
	if (!Destination) return false;
	std::memcpy(Destination, PackedLights.GetData(), Count * sizeof(FSceneLightRenderInfo));
	RenderCommand::Unmap(SceneLightBuffer.get());
	return true;
}
