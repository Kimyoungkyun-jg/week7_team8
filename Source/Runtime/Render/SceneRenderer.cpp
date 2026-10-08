#include "EnginePCH.h"
#include "Render/SceneRenderer.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include "Render/DeferredViewTargets.h"
#include "Engine/World.h"
#include "Tasks/Tasks.h"
#include "Core/Stats/LightweightStats.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include <cmath>
#include <cstring>

DECLARE_CYCLE_STAT("Frustum Cull", STAT_FrustumCull);
DECLARE_CYCLE_STAT("Gather Elements", STAT_GatherElements);

// 생성자
FSceneRenderer::FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData)
	: World(InWorld), View(InView), RenderData(InRenderData)
{
	CachedViewContext = View.ToViewContext();
}

// 가시성 검사 및 렌더 큐 수집
void FSceneRenderer::InitViews(FRenderer* Renderer)
{
	if (!World)
	{
		RenderData.ResetFrame();
		return;
	}

	// 월드 프리미티브 수집 및 절두체 컬링
	GatherRenderPackets(World->GetScene(), RenderData, CachedViewContext, &View.Frustum, Renderer);

	// 렌더 큐 자체 정렬
	RenderData.RenderQueue.Sort();

	for (const FLightSceneProxy* Light : World->GetScene().GetLights()) // 월드에 있는 모든 라이트 정보 가져오기
	{
		if (!Light->IsEnabled() || !Light->IsShown(View.bGameView)) continue;
		const FPointLightRenderData& Data = Light->GetRenderData();
		const float Radius = Data.PositionRadius.W;
		bool bInView = true;
		for (const FPlane& Plane : View.Frustum.Planes)
		{
			const float Distance = Plane.Normal.X * Data.PositionRadius.X +
				Plane.Normal.Y * Data.PositionRadius.Y +
				Plane.Normal.Z * Data.PositionRadius.Z + Plane.Distance;
			const float NormalLength = std::sqrt(Plane.Normal.X * Plane.Normal.X +
				Plane.Normal.Y * Plane.Normal.Y + Plane.Normal.Z * Plane.Normal.Z);
			if (Distance < -Radius * NormalLength)
			{
				bInView = false;
				break;
			}
		}
		if (!bInView) continue;
		if (RenderData.VisiblePointLights.Num() >= MaxDeferredPointLights) break;
		RenderData.VisiblePointLights.Add(Data);
	}
}

bool FSceneRenderer::RenderGBuffer(FRenderer* Renderer, const FDeferredViewTargets& Targets, uint32 Width, uint32 Height)
{
	if (!Renderer || !RenderCommand::BeginGBufferPass(Targets, Width, Height))
		return false;

	PrepareDrawData(Renderer, RenderData);
	Renderer->RenderOpaque(View, RenderData.RenderQueue, true);
	RenderCommand::EndRenderPass();
	return true;
}

void FSceneRenderer::RenderDeferredLighting(FRenderer* Renderer, const FDeferredViewTargets& Targets)
{
	if (Renderer)
		Renderer->DrawDeferredLighting(View, Targets, RenderData.VisiblePointLights);
}

void FSceneRenderer::RenderToneMap(FRenderer* Renderer, const FDeferredViewTargets& Targets)
{
	if (Renderer)
		Renderer->DrawToneMap(Targets);
}

// 반투명 렌더링
void FSceneRenderer::RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants)
{
	if (!Renderer)
	{
		return;
	}

	RenderCommand::BindConstantBuffer(3, FogConstants, EShaderBindFlagBits::Pixel);
	PrepareDrawData(Renderer, RenderData);
	Renderer->RenderTranslucent(View, RenderData.RenderQueue);
	RenderCommand::BindConstantBuffer(3, nullptr, EShaderBindFlagBits::Pixel);
}

void FSceneRenderer::GatherRenderPackets(FScene& Scene, FSceneRenderData& Data,
	const FViewContext& View, const FFrustumPlanes* Frustum, FRenderer* Renderer)
{
	Data.ResetFrame();
	FRenderQueue& RenderQueue = Data.RenderQueue;
	FRenderStats& RenderStats = Data.RenderStats;
	auto& VisibleProxies = Data.VisibleProxies;
	auto& LODInputs = Data.LODInputs;
	auto& SelectedLODs = Data.SelectedLODs;
	auto& GatherChunks = Data.GatherChunks;
	using FGatherChunk = FSceneRenderData::FGatherChunk;

	{
		SCOPE_CYCLE_COUNTER(STAT_FrustumCull);
		// 표시 상태는 프록시 캐시로 판정해 오클루전과 드로우 수집 전에 제외한다.
		const auto Visit = [&](FPrimitiveSceneProxy* Proxy)
		{
			if (Proxy->IsShown(View.bGameView)) VisibleProxies.Add(Proxy);
		};

		if (Frustum)
		{
			Scene.BVH.QueryCull(
				FrustumAllPlanesMask,
				[Frustum](const FBox& Bounds, uint32& Mask)
				{
					return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, *Frustum, Mask));
				},
				Visit);
		}
		else
		{
			Scene.BVH.QueryCull(0, [](const FBox&, uint32&) { return EBVHCullResult::Inside; }, Visit);
		}
	}

	RenderStats.TotalPrimitives = Scene.Proxies.Num();

	// GPU 오클루전: 프러스텀을 통과한 물체를 GPU에서 가림 판정하고 결과를 이번 프레임에 받아 온다.
	// Cull이 켜져 있으면 가려진 물체를 목록에서 빼서 이후 Gather·정렬·드로우를 모두 건너뛴다.
	// 꺼져 있으면(검증 모드) 목록은 그대로 두고 패킷에 판정만 표시한다.
	const uint8* OccludedMask = nullptr;
	if (Renderer && Renderer->GetGPUOcclusion().GetSettings().bEnabled)
	{
		FGPUOcclusion& Occlusion = Renderer->GetGPUOcclusion();
		if (Occlusion.Run(VisibleProxies.GetData(), VisibleProxies.Num(), View))
		{
			const std::vector<uint8>& Occluded = Occlusion.GetOccluded();
			if (Occlusion.GetSettings().bCull)
			{
				uint32 Kept = 0;
				for (uint32 i = 0; i < VisibleProxies.Num(); ++i)
					if (!Occluded[i])
						VisibleProxies[Kept++] = VisibleProxies[i];
				VisibleProxies.SetNum(Kept);
			}
			else
			{
				OccludedMask = Occluded.data();
			}
		}
	}

	RenderStats.VisiblePrimitives = VisibleProxies.Num();

	constexpr uint32 ExtraSlots = 4096;
	const uint32 VisibleCount = VisibleProxies.Num();
	const uint32 MaxSlots = VisibleCount + ExtraSlots;
	if (Renderer) Data.ObjectConstants.SetNum(MaxSlots);
	const bool bPrepareObjectSlots = Renderer != nullptr;
	uint32 NextExtraSlot = VisibleCount;

	// 조각 수 = 스레드 수 × 4. 잘게 나눠야 먼저 끝난 스레드가 남은 조각을 가져가서 부하가 고르게 된다.
	const uint32 NumThreads = Tasks::FTaskScheduler::Get().GetNumWorkers() + 1;
	const uint32 ChunkCount = FMath::Clamp(VisibleCount, 1u, NumThreads * 4);

	if (GatherChunks.Num() < ChunkCount)
		GatherChunks.SetNum(ChunkCount);     // 늘릴 때만. 줄이지 않아야 배열 용량이 계속 재사용된다.

	// Renderer가 있으면 불투명 스태틱 메시는 패킷 대신 조각별 묶음에 작은 항목으로 넣는다.
	// 동일 바인딩의 개별 드로우를 묶어 패킷 복사와 항목별 정렬을 줄인다.
	const bool bStaticGroups = Renderer != nullptr;

	{
		SCOPE_CYCLE_COUNTER(STAT_GatherElements);

		LODInputs.Reset();
		LODInputs.Reserve(VisibleProxies.Num());
		for (const FPrimitiveSceneProxy* Proxy : VisibleProxies)
			LODInputs.Add({Proxy->GetLODSphere(), Proxy->GetRenderState()});
		SelectLODs(LODInputs, View, SelectedLODs);
		Tasks::ParallelForChunks(VisibleCount, ChunkCount, [&](uint32 Begin, uint32 End, uint32 ChunkIndex)
			{
				FGatherChunk& Out = GatherChunks[ChunkIndex];      // 이 조각 전용. 다른 스레드는 절대 안 건드림
				// (머티리얼, 메시, LOD) 묶음 찾기. 조합이 몇 개뿐이라 선형 탐색이면 충분하고, 바로 전 묶음을 먼저 본다.
				const auto FindGroup = [&Out](UMaterial* Material, UStaticMesh* Mesh, uint8 LOD) -> FStaticDrawGroup&
					{
						if (Out.LastGroup < Out.Groups.size())
						{
							FStaticDrawGroup& Last = Out.Groups[Out.LastGroup];
							if (Last.Material == Material && Last.Mesh == Mesh && Last.LODIndex == LOD)
								return Last;
						}
						for (uint32 g = 0; g < Out.Groups.size(); ++g)
						{
							FStaticDrawGroup& Group = Out.Groups[g];
							if (Group.Material == Material && Group.Mesh == Mesh && Group.LODIndex == LOD)
							{
								Out.LastGroup = g;
								return Group;
							}
						}
						Out.LastGroup = static_cast<uint32>(Out.Groups.size());
						FStaticDrawGroup& Group = Out.Groups.emplace_back();
						Group.Material = Material;
						Group.Mesh = Mesh;
						Group.LODIndex = LOD;
						return Group;
					};

				for (uint32 VisibleIndex = Begin; VisibleIndex < End; ++VisibleIndex)
				{
					FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];   // 읽기만
					if (!Proxy->IsVisible()) continue;

					UStaticMesh* Mesh = Proxy->GetMesh();
					if (!Mesh)
					{
						Out.SlowPathIndices.Add(VisibleIndex);     // 컴포넌트를 건드리는 경로는 메인이 나중에
						continue;
					}

					const uint32 LOD = SelectedLODs[VisibleIndex];
					const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);
					++Out.LODCounts[LOD];                          // RenderStats 대신 조각 전용 통계

					uint32 Slot = InvalidObjectSlot;
					if (bPrepareObjectSlots)
					{
						// 칸 VisibleIndex는 이 반복만 쓴다 → 스레드끼리 겹치지 않음
						const FPerObjectConstants Constants =
							MakePerObjectConstants(Proxy->GetLocalToWorld());

						Data.ObjectConstants[VisibleIndex] = Constants;

						Slot = VisibleIndex;
					}

					const bool bOccludedByGpu = OccludedMask && OccludedMask[VisibleIndex];
					for (uint32 i = 0; i < CachedLOD.NumSections; ++i)
					{
						const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + i);
						Out.LODTriangles[LOD] += Section.IndexCount / 3;

						if (bStaticGroups && Section.Material && Section.Material->PSOType == EPSOType::StaticMesh_Opaque)
						{
							FStaticDrawGroup& Group = FindGroup(Section.Material, Mesh, static_cast<uint8>(LOD));
							Group.Items.push_back({ Proxy, Slot, Section.StartIndex, Section.IndexCount, bOccludedByGpu ? 1u : 0u });
							++Out.StaticDrawCount;
							continue;
						}

						FRenderPacket& Packet = Out.Packets.AddDefaulted_GetRef();   // 조각 전용 배열에 추가
						Packet.Proxy = Proxy;
						Packet.Mesh = Mesh;
						Packet.Material = Section.Material;
						Packet.StartIndex = Section.StartIndex;
						Packet.IndexCount = Section.IndexCount;
						Packet.LODIndex = static_cast<uint8>(LOD);
						Packet.Slot = Slot;
						Packet.bOccludedByGpu = bOccludedByGpu;
					}
				}
			});

		// Count this view's static draws. Bind its groups only when it is rendered.
		uint32 StaticDraws = 0;
		if (Renderer)
		{
			for (uint32 c = 0; c < ChunkCount; ++c)
			{
				StaticDraws += GatherChunks[c].StaticDrawCount;
			}
		}

		uint32 TotalPackets = 0;
		for (uint32 c = 0; c < ChunkCount; ++c)
			TotalPackets += GatherChunks[c].Packets.Num();

		RenderQueue.Reserve(TotalPackets + 256);             // 느린 경로 몫 약간 여유
		for (uint32 c = 0; c < ChunkCount; ++c)
			if (GatherChunks[c].Packets.Num() > 0)
				RenderQueue.Append(GatherChunks[c].Packets);  // 조각 순서대로 이어 붙이기

		for (uint32 c = 0; c < ChunkCount; ++c)
		{
			for (uint32 VisibleIndex : GatherChunks[c].SlowPathIndices)
			{
				FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];
				UPrimitiveComponent* Primitive = Proxy->GetComponent();
				if (!Primitive || !Primitive->IsVisible())
					continue;

				const uint32 FirstNew = RenderQueue.Num();

				// 프록시 캐시가 없는 스태틱 메시는 기존처럼 LOD를 골라 제출하고, 그 외는 컴포넌트에 맡긴다.
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
				if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
				{
					const uint32 LOD = SelectedLODs[VisibleIndex];
					StaticMeshComponent->SubmitToRenderQueue(RenderQueue, LOD);
				}
				else
				{
					Primitive->SubmitToRenderQueue(RenderQueue, View);
				}

				// continue 없이 항상 여기까지 와서 새 패킷에 여유 칸을 배정한다.
				for (uint32 p = FirstNew; p < RenderQueue.Num(); ++p)
				{
					if (!bPrepareObjectSlots || NextExtraSlot >= MaxSlots) break;
					FRenderPacket& Packet = RenderQueue[p];
					const FMatrix& Model = Packet.Proxy ? Packet.Proxy->GetLocalToWorld() : Packet.Model ? *Packet.Model : FMatrix::Identity;
					Data.ObjectConstants[NextExtraSlot] = MakePerObjectConstants(Model);
					Packet.Slot = NextExtraSlot++;
				}
			}
		}

		for (uint32 c = 0; c < ChunkCount; ++c)
			for (uint32 L = 0; L < 4; ++L)
			{
				RenderStats.LODCounts[L] += GatherChunks[c].LODCounts[L];
				RenderStats.LODTriangles[L] += GatherChunks[c].LODTriangles[L];
			}

		if (bPrepareObjectSlots) Data.ObjectConstants.SetNum(NextExtraSlot);
		RenderStats.DrawCalls = RenderQueue.Num() + StaticDraws;
		for (uint64 T : RenderStats.LODTriangles) RenderStats.Triangles += T;
	}
}


void FSceneRenderer::PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data)
{
	if (!Renderer) return;
	Renderer->ResetStaticDrawGroups();
	for (const FSceneRenderData::FGatherChunk& Chunk : Data.GatherChunks)
		for (const FStaticDrawGroup& Group : Chunk.Groups)
			if (!Group.Items.empty()) Renderer->AddStaticDrawGroup(&Group);

	if (uint8* Destination = Renderer->BeginObjectConstants(static_cast<uint32>(Data.ObjectConstants.Num())))
	{
		for (int32 Index = 0; Index < Data.ObjectConstants.Num(); ++Index)
			std::memcpy(Destination + static_cast<size_t>(Index) * ObjectSlotBytes,
				&Data.ObjectConstants[Index], sizeof(FPerObjectConstants));
		Renderer->EndObjectConstants();
	}
}
