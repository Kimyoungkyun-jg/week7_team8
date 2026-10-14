#include "EnginePCH.h"
#include "ForwardLightTiles.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/SceneView.h"
#include "Render/Texture2D.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr uint32 LightTileSize = 32;
	constexpr uint32 AverageLightsPerTile = 64;
}

bool FForwardLightTileGrid::FTileBuffer::EnsureCapacity(uint32 Count, uint32 Stride)
{
	if (Buffer && Count <= Capacity) return true;
	if (!Count || Count > UINT32_MAX / Stride) return false;
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = Count * Stride;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;
	ID3D11Device* Device = RenderCommand::GetDevice();
	ComPtr<ID3D11Buffer> NewBuffer;
	ComPtr<ID3D11ShaderResourceView> NewSRV;
	ComPtr<ID3D11UnorderedAccessView> NewUAV;
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, NewBuffer.GetAddressOf()))) return false;
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.NumElements = Count;
	if (FAILED(Device->CreateShaderResourceView(NewBuffer.Get(), &SRVDesc, NewSRV.GetAddressOf()))) return false;
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.NumElements = Count;
	if (FAILED(Device->CreateUnorderedAccessView(NewBuffer.Get(), &UAVDesc, NewUAV.GetAddressOf()))) return false;
	Buffer = std::move(NewBuffer);
	SRV = std::move(NewSRV);
	UAV = std::move(NewUAV);
	Capacity = Count;
	return true;
}

bool FForwardLightTileGrid::Init()
{
	CullingCB = RenderCommand::CreateConstantBuffer(sizeof(FTileCullingConstants));
	LightingInfoCB = RenderCommand::CreateConstantBuffer(sizeof(uint32) * 4);
	if (!CullingCB || !LightingInfoCB || !Allocator.EnsureCapacity(2, sizeof(uint32))) return false;
	ComPtr<ID3DBlob> Code, Errors;
	if (FAILED(D3DCompileFromFile(L"Resources/Shader/TileLightCullingCS.hlsli", nullptr, nullptr,
		"mainCS", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, Code.GetAddressOf(), Errors.GetAddressOf())))
	{
		if (Errors) OutputDebugStringA(static_cast<const char*>(Errors->GetBufferPointer()));
		return false;
	}
	return SUCCEEDED(RenderCommand::GetDevice()->CreateComputeShader(Code->GetBufferPointer(),
		Code->GetBufferSize(), nullptr, ComputeShader.GetAddressOf()));
}

bool FForwardLightTileGrid::PrepareBuild(const FSceneView& View,
	const FForwardLightingConstants& Lighting, const FStructuredBuffer* SceneLights, const FTexture2D* Depth)
{
	bPixelTilesValid = false;
	if (!ComputeShader || !CullingCB || !LightingInfoCB || !SceneLights || !SceneLights->GetSRV() ||
		!Depth || !Depth->GetSRV() || !Depth->GetWidth() || !Depth->GetHeight()) return false;
	const FMatrix& Projection = View.ProjectionMatrix;
	if (!std::isfinite(Projection.M[1][0]) || std::abs(Projection.M[1][0]) < 1.0e-8f ||
		!std::isfinite(Projection.M[2][1]) || std::abs(Projection.M[2][1]) < 1.0e-8f ||
		!std::isfinite(Projection.M[0][2]) || !std::isfinite(Projection.M[3][2]) ||
		(View.bIsPerspective ? std::abs(Projection.M[3][2]) : std::abs(Projection.M[0][2])) < 1.0e-8f)
		return false;
	Constants = {};
	Constants.View = View.ViewMatrix;
	Constants.ProjectionParams = FVector4(1.0f / Projection.M[1][0], 1.0f / Projection.M[2][1],
		Projection.M[0][2], Projection.M[3][2]);
	Constants.ViewWidth = Depth->GetWidth();
	Constants.ViewHeight = Depth->GetHeight();
	Constants.TileCountX = (Constants.ViewWidth - 1) / LightTileSize + 1;
	TileCountY = (Constants.ViewHeight - 1) / LightTileSize + 1;
	const uint64 TileCount = static_cast<uint64>(Constants.TileCountX) * TileCountY;
	if (TileCount > UINT32_MAX / (AverageLightsPerTile * sizeof(uint32))) return false;
	const uint32 IndexBudget = static_cast<uint32>(TileCount) * AverageLightsPerTile;
	Constants.IsPerspective = View.bIsPerspective ? 1u : 0u;
	Constants.PointLightCount = Lighting.LightCounts[0];
	Constants.SpotLightCount = Lighting.LightCounts[1];
	std::memcpy(Constants.LightOffsets, Lighting.LightOffsets, sizeof(Constants.LightOffsets));
	if (!Ranges.EnsureCapacity(static_cast<uint32>(TileCount), sizeof(uint32) * 4) ||
		!PointIndices.EnsureCapacity(std::max({IndexBudget, Constants.PointLightCount, 1u}), sizeof(uint32)) ||
		!SpotIndices.EnsureCapacity(std::max({IndexBudget, Constants.SpotLightCount, 1u}), sizeof(uint32)))
		return false;
	Constants.PointIndexCapacity = PointIndices.Capacity;
	Constants.SpotIndexCapacity = SpotIndices.Capacity;
	return true;
}

bool FForwardLightTileGrid::ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights)
{
	if (!Renderer || !Depth || !Depth->GetSRV() || !SceneLights || !SceneLights->GetSRV() ||
		!ComputeShader || !Ranges.UAV || !PointIndices.UAV || !SpotIndices.UAV || !Allocator.UAV)
		return false;
	Renderer->UpdateConstant(CullingCB.get(), &Constants, sizeof(Constants));
	Renderer->ClearUnorderedAccess(Allocator.UAV.Get());
	const std::array<ID3D11ShaderResourceView*, 2> Inputs = {Depth->GetSRV(), SceneLights->GetSRV()};
	const std::array<ID3D11UnorderedAccessView*, 4> Outputs =
		{Ranges.UAV.Get(), PointIndices.UAV.Get(), SpotIndices.UAV.Get(), Allocator.UAV.Get()};
	Renderer->DispatchCompute(ComputeShader.Get(), CullingCB->GetBuffer(), 0, Inputs, Outputs,
		Constants.TileCountX, TileCountY, 1);
	return true;
}

void FForwardLightTileGrid::PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded)
{
	bPixelTilesValid = bBuildSucceeded && Ranges.SRV && PointIndices.SRV && SpotIndices.SRV;
	if (!Renderer || !LightingInfoCB) return;
	const uint32 TileInfo[4] = {bPixelTilesValid ? 1u : 0u, Constants.TileCountX, LightTileSize, 0};
	Renderer->UpdateConstant(LightingInfoCB.get(), TileInfo, sizeof(TileInfo));
}

std::array<ID3D11Resource*, 4> FForwardLightTileGrid::GetWriteResources() const
{
	return {Ranges.Buffer.Get(), PointIndices.Buffer.Get(), SpotIndices.Buffer.Get(), Allocator.Buffer.Get()};
}

std::array<ID3D11ShaderResourceView*, 3> FForwardLightTileGrid::GetPixelResources() const
{
	return bPixelTilesValid ? std::array<ID3D11ShaderResourceView*, 3>{Ranges.SRV.Get(), PointIndices.SRV.Get(), SpotIndices.SRV.Get()}
		: std::array<ID3D11ShaderResourceView*, 3>{};
}
