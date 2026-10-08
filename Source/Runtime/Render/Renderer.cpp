#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "Engine/PrimitiveSceneProxy.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "DeferredViewTargets.h"

#include "Camera/CameraComponent.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <bit>

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);

namespace
{
	struct FDeferredLightConstants
	{
		FMatrix InverseViewProjection;
		FVector4 CameraPosition; // W: perspective projection
		FVector4 CameraForward;
		FVector4 ViewportLightCount;
		FPointLightRenderData PointLights[MaxDeferredPointLights];
	};
	static_assert(sizeof(FPointLightRenderData) == 32);
	static_assert(sizeof(FDeferredLightConstants) % 16 == 0);
	// VSSetConstantBuffers1의 오프셋은 16개 상수(256바이트) 단위여야 하므로 오브젝트마다 256바이트 칸을 쓴다.
	constexpr uint32 PerObjectSlotConstants = 16;
	static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

	constexpr uint32 MinPerObjectSlots = 1024;

	const FMatrix& GetPacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	// 불투명 패킷과 같은 규칙의 키 (묶음은 전부 불투명)
	uint64 MakeGroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));

	GPUOcclusion.Init();   // 실패해도 오클루전만 못 쓸 뿐 렌더링은 된다

	return true;
}

// 필요한 칸 수가 용량을 넘을 때만 두 배씩 키워 재할당을 드물게 한다.
void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity)
		return;

	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount)
		NewCapacity *= 2;

	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer ({} slots) creation failed. Falling back to per-draw updates.", NewCapacity);
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

// 시점 상수 버퍼 및 렌더링 상태 설정
void FRenderer::SetupView(const FSceneView& View)
{
	bCurrentWireframe = View.bIsWireframe;
	RenderCommand::UpdateBufferData(ViewCB.get(), &View.ViewProjectionMatrix);
}

// 전체 렌더 큐 렌더링
void FRenderer::RenderAll(const FSceneView& View, const FRenderQueue& InQueue)
{
	RenderOpaque(View, InQueue);
	RenderTranslucent(View, InQueue);
}

// 불투명 요소 렌더링
void FRenderer::RenderOpaque(const FSceneView& View, const FRenderQueue& InQueue, bool bGBufferPass)
{
	SetupView(View);
	DrawStaticGroups(bGBufferPass);
	DrawPackets(InQueue, 0, InQueue.GetFirstTranslucentIndex(), View.ViewProjectionMatrix, bGBufferPass);
}

void FRenderer::DrawDeferredLighting(const FSceneView& View, const FDeferredViewTargets& Targets,
	const TArray<FPointLightRenderData>& PointLights)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::DeferredLighting);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader || !DeferredLightCB)
		return;
	FDeferredLightConstants Constants{};
	Constants.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y,
		View.ViewLocation.Z, View.bIsPerspective ? 1.0f : 0.0f);
	Constants.CameraForward = FVector4(View.ViewForward.X, View.ViewForward.Y, View.ViewForward.Z, 0.0f);
	const uint32 LightCount = std::min<uint32>(static_cast<uint32>(PointLights.Num()), MaxDeferredPointLights);
	Constants.ViewportLightCount = FVector4(
		static_cast<float>(Targets.SceneColor->GetWidth()),
		static_cast<float>(Targets.SceneColor->GetHeight()),
		static_cast<float>(LightCount), 0.0f);
	for (uint32 Index = 0; Index < LightCount; ++Index)
		Constants.PointLights[Index] = PointLights[Index];
	RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindConstantBuffer(0, DeferredLightCB.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, Targets.BaseColorMetallic.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Targets.NormalRoughness.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(2, Targets.Depth.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	if (LightCount > 0)
	{
		FPipelineState* AdditivePSO = FRenderResourceManager::GetPSO(EPSOType::DeferredPointLighting);
		if (AdditivePSO && AdditivePSO->Shader && AdditivePSO->Shader->VertexShader && AdditivePSO->Shader->PixelShader)
		{
			Constants.ViewportLightCount.W = 1.0f;
			RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));
			RenderCommand::BindPipelineState(*AdditivePSO);
			RenderCommand::Draw(3);
		}
	}

	RenderCommand::UnbindShaderResources(0, 3, EShaderBindFlagBits::Pixel);
}

void FRenderer::DrawToneMap(const FDeferredViewTargets& Targets)
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

// 반투명 요소 렌더링
void FRenderer::RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue)
{
	SetupView(View);
	DrawPackets(InQueue, InQueue.GetFirstTranslucentIndex(), InQueue.Num(), View.ViewProjectionMatrix);
	StaticGroups.clear();
}

// 스태틱 메시 묶음을 그린다
void FRenderer::DrawStaticGroups(bool bGBufferPass)
{
	if (StaticGroups.empty())
		return;

	// 스태틱 메시 묶음 정렬
	std::erase_if(StaticGroups, [](const FStaticDrawGroup* Group) { return Group->Items.empty(); });
	std::sort(StaticGroups.begin(), StaticGroups.end(),
		[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return MakeGroupKey(*A) < MakeGroupKey(*B); });

	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);

	UMaterial* BoundMaterial = nullptr;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindMesh(Group->Mesh, Group->LODIndex);
		if (Group->Material != BoundMaterial)
		{
			BoundMaterial = Group->Material;
			BindMaterial(BoundMaterial, bCurrentWireframe, bGBufferPass);
			FRenderPacket MaterialOnly;
			MaterialOnly.Material = BoundMaterial;
			UpdateMaterialParams(MaterialOnly);
		}

		for (const FStaticDrawItem& Item : Group->Items)
		{
			if (bUsePerObjectSlots && Item.Slot != InvalidObjectSlot)
			{
				RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), Item.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
			}
			else
			{
				RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
				UpdatePerObjectConstants(Item.Proxy->GetLocalToWorld());
			}
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}

}

// 정렬된 패킷 중 [Begin, End) 범위를 그린다
void FRenderer::DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End, const FMatrix& ViewProjection, bool bGBufferPass)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	UStaticMesh* LastMesh = nullptr;
	UMaterial* LastMaterial = nullptr;
	uint8 LastLODIndex = 0;

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);

	for (uint32 k = Begin; k < End; ++k)
	{
		const FRenderPacket& RenderPacket = InQueue[k];
		if (RenderPacket.Mesh != LastMesh || RenderPacket.LODIndex != LastLODIndex) {
			RenderCommand::BindMesh(RenderPacket.Mesh, RenderPacket.LODIndex);
		}
		if (RenderPacket.Material != LastMaterial) {
			BindMaterial(RenderPacket.Material, bCurrentWireframe, bGBufferPass);
		}
		if (RenderPacket.Material != LastMaterial || RenderPacket.MaterialParamData)
			UpdateMaterialParams(RenderPacket);
		if (bUsePerObjectSlots && RenderPacket.Slot != InvalidObjectSlot)
		{
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), RenderPacket.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		}
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			UpdatePerObjectConstants(RenderPacket, ViewProjection);
		}

		RenderCommand::DrawIndexed(
			RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.Mesh->GetIndexBuffer(RenderPacket.LODIndex)->GetIndexCount(),
			RenderPacket.StartIndex
		);
		LastMaterial = RenderPacket.Material;
		LastMesh = RenderPacket.Mesh;
		LastLODIndex = RenderPacket.LODIndex;
	}
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue)
{
	FOcclusionMeasureResult Result;

	// 불투명 드로우를 두 경로(스태틱 묶음, 일반 패킷)에서 한 목록으로 모은다. 그린 순서와 같게 묶음 먼저.
	struct FMeasureDraw
	{
		UStaticMesh* Mesh;
		UMaterial* Material;
		uint8 LODIndex;
		uint32 Slot;
		uint32 StartIndex;
		uint32 IndexCount;
		const FMatrix* World;       // 칸이 없을 때만 쓴다
		const void* ObjectKey;      // 같은 물체의 섹션을 한 물체로 센다
		bool bOccludedByGpu;
		const FRenderPacket* Packet;
	};
	std::vector<FMeasureDraw> Draws;
	for (const FStaticDrawGroup* Group : StaticGroups)
		for (const FStaticDrawItem& Item : Group->Items)
			Draws.push_back({ Group->Mesh, Group->Material, Group->LODIndex, Item.Slot, Item.StartIndex, Item.IndexCount,
				&Item.Proxy->GetLocalToWorld(), Item.Proxy, Item.bOccludedByGpu != 0, nullptr });
	for (uint32 k = 0; k < InQueue.GetFirstTranslucentIndex(); ++k)
	{
		const FRenderPacket& Packet = InQueue[k];
		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount();
		// 프록시가 없는 패킷(빌보드 등)은 패킷 자체를 한 물체로 센다.
		const void* Key = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);
		Draws.push_back({ Packet.Mesh, Packet.Material, Packet.LODIndex, Packet.Slot, Packet.StartIndex, IndexCount,
			&GetPacketWorld(Packet), Key, Packet.bOccludedByGpu, &Packet });
	}

	const uint32 Count = static_cast<uint32>(Draws.size());
	if (Count == 0)
		return Result;

	const auto StartTime = std::chrono::high_resolution_clock::now();

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 같은 셰이더·같은 행렬로 다시 그리면 깊이가 비트 단위로 같으므로,
	// LESS_EQUAL이면 최종 깊이 버퍼에서 이 물체가 이긴 픽셀만 통과한다.
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf())))
			return Result;
	}

	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC QueryDesc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&QueryDesc, Query.GetAddressOf())))
			return Result;
		OcclusionQueries.Add(std::move(Query));
	}

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	UStaticMesh* BoundMesh = nullptr;
	UMaterial* BoundMaterial = nullptr;
	uint8 BoundLOD = 0;

	// 1) 드로우마다 쿼리를 걸고 다시 그린다. 본 패스와 같은 바인딩(칸 또는 PerObjectCB)을 쓴다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		if (Draw.Mesh != BoundMesh || Draw.LODIndex != BoundLOD)
			RenderCommand::BindMesh(Draw.Mesh, Draw.LODIndex);
		if (Draw.Material != BoundMaterial)
		{
			BindMaterial(Draw.Material);
			// BindMaterial이 바꾼 상태를 측정용으로 덮어쓴다.
			RenderCommand::SetBlendState(EBlendState::NoColorWrite);
			Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		}
		if (Draw.Packet && (Draw.Material != BoundMaterial || Draw.Packet->MaterialParamData))
			UpdateMaterialParams(*Draw.Packet);

		if (bUsePerObjectSlots && Draw.Slot != InvalidObjectSlot)
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Draw.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			UpdatePerObjectConstants(*Draw.World);
		}

		ID3D11Query* Query = OcclusionQueries[Index].Get();
		Context->Begin(Query);
		RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
		Context->End(Query);

		BoundMesh = Draw.Mesh;
		BoundMaterial = Draw.Material;
		BoundLOD = Draw.LODIndex;
	}

	// 2) 결과를 기다려 모은다. 한 물체가 Section 여러 개로 나뉘면 하나라도 보이면 보이는 것으로 친다.
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		UINT64 Samples = 0;
		while (Context->GetData(OcclusionQueries[Index].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}

		const uint64 Triangles = Draw.IndexCount / 3;
		const bool bVisible = Samples > 0;

		// Cull을 끄고 판정만 한 드로우: 가렸다고 했는데 최종 화면에 픽셀이 남았으면 잘못 가린 것이다.
		if (Draw.bOccludedByGpu)
		{
			++Result.GPUOccludedDraws;
			if (bVisible)
				++Result.FalseCulls;
		}

		++Result.TotalDraws;
		Result.TotalTriangles += Triangles;
		if (bVisible)
		{
			++Result.VisibleDraws;
			Result.VisibleTriangles += Triangles;
		}

		bool& bObjectVisible = ObjectVisible[Draw.ObjectKey];
		bObjectVisible = bObjectVisible || bVisible;
	}

	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;

	// 뒤따르는 Grid·반투명 패스를 위해 상태를 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

	Result.ElapsedMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB)
	{
		RenderCommand::Unmap(PerObjectSlotCB.get());
	}
}

// 머티리얼 파이프라인 상태 및 텍스처 바인딩
void FRenderer::BindMaterial(UMaterial* material, bool bInWireframe, bool bGBufferPass)
{
	EPSOType PSOType = material->PSOType;
	if (bGBufferPass && PSOType == EPSOType::StaticMesh_Opaque)
	{
		PSOType = EPSOType::StaticMesh_GBuffer;
	}
	else if (bInWireframe && PSOType == EPSOType::StaticMesh_Opaque)
	{
		PSOType = EPSOType::StaticMesh_Wireframe;
	}

	if (FPipelineState* PSO = FRenderResourceManager::GetPSO(PSOType))
	{
		RenderCommand::BindPipelineState(*PSO);

		if (bGBufferPass && bInWireframe)
			RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
	}
	// 텍스처와 샘플러 바인딩
	for (size_t i = 0; i < material->Textures.size(); i++)
	{
		RenderCommand::BindShaderResource(static_cast<uint32>(i), material->Textures[i], EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel);
}

// b1 내용 채우고 꽂기
void FRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
{

	if (!RenderPacket.Material || !RenderPacket.Material->ParamBuffer)
	{
		return;
	}

	switch (RenderPacket.Material->PSOType)
	{
	case EPSOType::StaticMesh_Opaque:
	case EPSOType::StaticMesh_Translucent:
	case EPSOType::StaticMesh_Wireframe:
	{
		const float TotalTime = EngineTimer::GetTotalTime();
		FStaticMeshMaterialParams Params{};
		Params.BaseColor = RenderPacket.Material->BaseColor;
		Params.UVOffset = RenderPacket.Material->UVScrollSpeed * TotalTime;
		RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		break;
	}
	case EPSOType::Particle_AlphaBlend:
	case EPSOType::Particle_Additive:
	{
		if (RenderPacket.MaterialParamData != nullptr)
		{
			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		}
		break;
	}
	default:
		break;
	}
}

// b0 MVP 채우고 꽂기
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection)
{
	// rp.Transform 과 Camera VP 행렬 곱
	// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요

	UpdatePerObjectConstants(GetPacketWorld(RenderPacket));
}

void FRenderer::UpdatePerObjectConstants(const FMatrix& World)
{
	const FPerObjectConstants Constants =
		MakePerObjectConstants(World);

	RenderCommand::UpdateBufferData(
		PerObjectCB.get(),
		&Constants,
		sizeof(Constants));
}
