#include "EnginePCH.h"
#include "Render/MeshDrawRenderer.h"
#include "Render/ForwardLightCluster.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Shader.h"
#include "Render/Mesh.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);

namespace
{
	constexpr uint32 ForwardSceneLightSlot = 8;
	constexpr uint32 PerObjectSlotConstants = 16;
	constexpr uint32 MinPerObjectSlots = 1024;
	static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

	float GetNormalTextureEncoding(UTexture2D* Texture)
	{
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()) return 0.0f;
		switch (Texture->GetResource()->GetFormat())
		{
		case DXGI_FORMAT_BC5_UNORM: return 2.0f;
		case DXGI_FORMAT_BC5_SNORM: return 3.0f;
		default: return 1.0f;
		}
	}

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

bool FMeshDrawRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	ForwardObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardObjectConstants));
	ForwardMaterialCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardMaterialConstants));
	return PerObjectCB && ViewCB && ForwardObjectCB && ForwardMaterialCB;
}

bool FMeshDrawRenderer::IsForwardReady() const
{
	return ForwardObjectCB && ForwardMaterialCB;
}

void FMeshDrawRenderer::RenderForwardOpaque(const FSceneView& View, const FRenderQueue& InQueue,
	const FPipelineState& PSO, FConstantBuffer* LightCB,
	FStructuredBuffer* SceneLights, FForwardLightClusterGrid& Clusters)
{
	ActiveForwardLightCB = LightCB;
	ActiveSceneLightBuffer = SceneLights;
	ActiveForwardClusters = &Clusters;
	DrawForwardStaticGroups(View, PSO);
	DrawForwardPackets(View, InQueue, PSO);
	ActiveForwardLightCB = nullptr;
	ActiveSceneLightBuffer = nullptr;
	ActiveForwardClusters = nullptr;
}

void FMeshDrawRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
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

void FMeshDrawRenderer::SetupView(const FSceneView& View)
{
	bCurrentWireframe = View.bIsWireframe;
	RenderCommand::UpdateBufferData(ViewCB.get(), &View.ViewProjectionMatrix);
}

void FMeshDrawRenderer::RenderDeferredOpaque(const FSceneView& View, const FRenderQueue& InQueue, bool bGBufferPass)
{
	SetupView(View);
	DrawStaticGroups(bGBufferPass);
	DrawPackets(InQueue, 0, InQueue.GetFirstTranslucentIndex(), View.ViewProjectionMatrix, bGBufferPass);
}

void FMeshDrawRenderer::BindForwardMaterial(UMaterial* Material, const FSceneView& View, const FPipelineState& PSO)
{
	RenderCommand::BindPipelineState(PSO);
	RenderCommand::BindConstantBuffer(1, ActiveForwardLightCB,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	if (View.bIsWireframe)
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
	UTexture2D* ColorTexture = Material->Textures.IsEmpty() ? nullptr : Material->Textures[0];
	const bool bHasColorTexture = ColorTexture && ColorTexture->GetResource() &&
		ColorTexture->GetResource()->GetSRV();
	UTexture2D* NormalTexture = Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr;
	const float NormalEncoding = GetNormalTextureEncoding(NormalTexture);
	const bool bHasNormalTexture = NormalEncoding > 0.5f;
	RenderCommand::BindShaderResource(0, bHasColorTexture ? ColorTexture : nullptr, EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, bHasNormalTexture ? NormalTexture : nullptr,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	for (size_t Index = 2; Index < Material->Textures.size(); ++Index)
		RenderCommand::BindShaderResource(static_cast<uint32>(Index), Material->Textures[Index], EShaderBindFlagBits::Pixel);
	ActiveForwardClusters->BindPixelShader();
	RenderCommand::BindShaderResource(ForwardSceneLightSlot,
		ActiveSceneLightBuffer && ActiveSceneLightBuffer->GetSRV() ? ActiveSceneLightBuffer : nullptr,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(0, Material->SamplerState,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	FForwardMaterialConstants Params{};
	Params.BaseColor = Material->BaseColor;
	Params.SurfaceParams = FVector4(0.25f, 32.0f, bHasColorTexture ? 1.0f : 0.0f,
		NormalEncoding);
	const FVector2 UVOffset = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
	Params.UVTransform = FVector4(UVOffset.X, UVOffset.Y, 1.0f, 1.0f);
	RenderCommand::UpdateBufferData(ForwardMaterialCB.get(), &Params, sizeof(Params));
	RenderCommand::BindConstantBuffer(2, ForwardMaterialCB.get(),
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
}

void FMeshDrawRenderer::DrawForwardObject(const FSceneView& View, const FMatrix& World,
	uint32 IndexCount, uint32 StartIndex)
{
	FForwardObjectConstants Object{};
	Object.World = World;
	Object.View = View.ViewMatrix;
	Object.Projection = View.ProjectionMatrix;
	Object.NormalMatrix = World.Inverse().GetTransposed();
	RenderCommand::UpdateBufferData(ForwardObjectCB.get(), &Object, sizeof(Object));
	RenderCommand::BindConstantBuffer(0, ForwardObjectCB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::DrawIndexed(IndexCount, StartIndex);
}

void FMeshDrawRenderer::RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue)
{
	SetupView(View);
	DrawPackets(InQueue, InQueue.GetFirstTranslucentIndex(), InQueue.Num(), View.ViewProjectionMatrix);
	StaticGroups.clear();
}

void FMeshDrawRenderer::SortStaticGroups()
{
	std::erase_if(StaticGroups, [](const FStaticDrawGroup* Group) { return Group->Items.empty(); });
	std::sort(StaticGroups.begin(), StaticGroups.end(),
		[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return MakeGroupKey(*A) < MakeGroupKey(*B); });
}

void FMeshDrawRenderer::DrawForwardStaticGroups(const FSceneView& View, const FPipelineState& PSO)
{
	SortStaticGroups();
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	UMaterial* BoundMaterial = nullptr;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindMesh(Group->Mesh, Group->LODIndex);
		if (Group->Material != BoundMaterial)
		{
			BoundMaterial = Group->Material;
			BindForwardMaterial(BoundMaterial, View, PSO);
		}
		for (const FStaticDrawItem& Item : Group->Items)
			DrawForwardObject(View, Item.Proxy->GetLocalToWorld(), Item.IndexCount, Item.StartIndex);
	}
}

void FMeshDrawRenderer::DrawForwardPackets(const FSceneView& View, const FRenderQueue& InQueue,
	const FPipelineState& PSO)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	UStaticMesh* LastMesh = nullptr;
	UMaterial* LastMaterial = nullptr;
	uint8 LastLODIndex = 0;
	for (uint32 Index = 0; Index < InQueue.GetFirstTranslucentIndex(); ++Index)
	{
		const FRenderPacket& Packet = InQueue[Index];
		if (Packet.Mesh != LastMesh || Packet.LODIndex != LastLODIndex)
			RenderCommand::BindMesh(Packet.Mesh, Packet.LODIndex);
		if (Packet.Material != LastMaterial || Packet.MaterialParamData)
			BindForwardMaterial(Packet.Material, View, PSO);
		DrawForwardObject(View, GetPacketWorld(Packet),
			Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount(),
			Packet.StartIndex);
		LastMesh = Packet.Mesh;
		LastMaterial = Packet.Material;
		LastLODIndex = Packet.LODIndex;
	}
}

void FMeshDrawRenderer::DrawStaticGroups(bool bGBufferPass)
{
	if (StaticGroups.empty())
		return;
	SortStaticGroups();

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

void FMeshDrawRenderer::DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End,
	const FMatrix& ViewProjection, bool bGBufferPass)
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
		if (RenderPacket.Material != LastMaterial)
			BindMaterial(RenderPacket.Material, bCurrentWireframe, bGBufferPass);
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

FOcclusionMeasureResult FMeshDrawRenderer::MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue)
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

uint8* FMeshDrawRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FMeshDrawRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB)
	{
		RenderCommand::Unmap(PerObjectSlotCB.get());
	}
}

void FMeshDrawRenderer::BindMaterial(UMaterial* material, bool bInWireframe, bool bGBufferPass)
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
	RenderCommand::BindShaderResource(0,
		material->Textures.Num() > 0 ? material->Textures[0] : nullptr,
		EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1,
		material->Textures.Num() > 1 ? material->Textures[1] : nullptr,
		EShaderBindFlagBits::Pixel);

	for (size_t i = 2; i < material->Textures.size(); ++i)
		RenderCommand::BindShaderResource(static_cast<uint32>(i), material->Textures[i], EShaderBindFlagBits::Pixel);

	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel);
}

void FMeshDrawRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
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
		UTexture2D* NormalTexture = RenderPacket.Material->Textures.Num() > 1
			? RenderPacket.Material->Textures[1] : nullptr;
		Params.TextureFlags.X = GetNormalTextureEncoding(NormalTexture);
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

void FMeshDrawRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection)
{
	// rp.Transform 과 Camera VP 행렬 곱
	// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요

	UpdatePerObjectConstants(GetPacketWorld(RenderPacket));
}

void FMeshDrawRenderer::UpdatePerObjectConstants(const FMatrix& World)
{
	const FPerObjectConstants Constants =
		MakePerObjectConstants(World);

	RenderCommand::UpdateBufferData(
		PerObjectCB.get(),
		&Constants,
		sizeof(Constants));
}

