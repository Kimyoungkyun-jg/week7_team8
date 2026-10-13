#include "EnginePCH.h"
#include "ForwardLightCluster.h"

#include "Buffer.h"
#include "LightRenderData.h"
#include "RenderCommand.h"
#include "Renderer.h"
#include "SceneView.h"

#include <cstring>

namespace
{
	constexpr uint32 ClusterTileSize = 32;
	constexpr uint32 ClusterDepthSlices = 16;
	constexpr uint32 ClusterLightCapacity = 64;

	// Compute 셰이더의 b0와 UberLit 픽셀 셰이더의 b3가 공유한다.
	struct FForwardClusterConstants
	{
		FMatrix InverseViewProjection;
		FVector4 CameraPosition;
		FVector4 CameraForward;
		uint32 ClusterDimensions[3]; // 가로 타일, 세로 타일, 깊이 구간
		uint32 bOrthographic;
		uint32 ViewportSize[4];
		uint32 LightCounts[4];
		uint32 LightOffsets[4];
		FVector4 DepthRange;
		uint32 ClusterOptions[4]; // x: 빌드 성공, y: 타일 크기, z: 클러스터당 최대 라이트 수
	};
	static_assert(sizeof(FForwardClusterConstants) == 192);
	static_assert(offsetof(FForwardClusterConstants, ClusterOptions) == 176);

	FForwardClusterConstants MakeClusterConstants(const FSceneView& View,
		const FForwardLightingConstants& Lighting, bool bClusterValid)
	{
		const uint32 Width = static_cast<uint32>(View.ViewRect.Width);
		const uint32 Height = static_cast<uint32>(View.ViewRect.Height);
		FForwardClusterConstants Constants{};
		Constants.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
		Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y, View.ViewLocation.Z, 1.0f);
		Constants.CameraForward = FVector4(View.ViewForward.X, View.ViewForward.Y, View.ViewForward.Z, 0.0f);
		Constants.ClusterDimensions[0] = (Width + ClusterTileSize - 1) / ClusterTileSize;
		Constants.ClusterDimensions[1] = (Height + ClusterTileSize - 1) / ClusterTileSize;
		Constants.ClusterDimensions[2] = ClusterDepthSlices;
		Constants.bOrthographic = View.bIsPerspective ? 0u : 1u;
		Constants.ViewportSize[0] = Width;
		Constants.ViewportSize[1] = Height;
		std::memcpy(Constants.LightCounts, Lighting.LightCounts, sizeof(Constants.LightCounts));
		std::memcpy(Constants.LightOffsets, Lighting.LightOffsets, sizeof(Constants.LightOffsets));
		Constants.DepthRange = FVector4(View.NearClip, View.FarClip, 0.0f, 0.0f);
		Constants.ClusterOptions[0] = bClusterValid ? 1u : 0u;
		Constants.ClusterOptions[1] = ClusterTileSize;
		Constants.ClusterOptions[2] = ClusterLightCapacity;
		return Constants;
	}

	bool CreateClusterBuffer(ID3D11Device* Device, uint32 ElementCount,
		ComPtr<ID3D11Buffer>& OutBuffer, ComPtr<ID3D11ShaderResourceView>& OutSRV,
		ComPtr<ID3D11UnorderedAccessView>& OutUAV)
	{
		if (!ElementCount || ElementCount > UINT32_MAX / sizeof(uint32)) return false;
		D3D11_BUFFER_DESC Desc{};
		Desc.ByteWidth = ElementCount * sizeof(uint32);
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = sizeof(uint32);
		if (FAILED(Device->CreateBuffer(&Desc, nullptr, OutBuffer.GetAddressOf()))) return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.NumElements = ElementCount;
		if (FAILED(Device->CreateShaderResourceView(OutBuffer.Get(), &SRVDesc, OutSRV.GetAddressOf()))) return false;

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVDesc.Buffer.NumElements = ElementCount;
		return SUCCEEDED(Device->CreateUnorderedAccessView(OutBuffer.Get(), &UAVDesc, OutUAV.GetAddressOf()));
	}
}

bool FForwardLightClusterGrid::Init()
{
	ID3D11Device* Device = RenderCommand::GetDevice();
	D3D11_BUFFER_DESC CBDesc{};
	CBDesc.ByteWidth = sizeof(FForwardClusterConstants);
	CBDesc.Usage = D3D11_USAGE_DEFAULT;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(Device->CreateBuffer(&CBDesc, nullptr, ConstantsCB.GetAddressOf())))
		return false;

	ComPtr<ID3DBlob> ClusterCode;
	ComPtr<ID3DBlob> ClusterErrors;
	const HRESULT CompileResult = D3DCompileFromFile(
		L"Resources/Shader/ForwardLightClusterCS.hlsli", nullptr, nullptr,
		"BuildForwardLightClusters", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		ClusterCode.GetAddressOf(), ClusterErrors.GetAddressOf());
	if (FAILED(CompileResult))
	{
		if (ClusterErrors)
			OutputDebugStringA(static_cast<const char*>(ClusterErrors->GetBufferPointer()));
		return false;
	}

	if (FAILED(Device->CreateComputeShader(ClusterCode->GetBufferPointer(),
		ClusterCode->GetBufferSize(), nullptr, ComputeShader.GetAddressOf())))
		return false;
	return true;
}

bool FForwardLightClusterGrid::PrepareBuild(const FSceneView& View,
	const FForwardLightingConstants& Lighting, const FStructuredBuffer* SceneLights)
{
	if (!ComputeShader || !ConstantsCB || !SceneLights || !SceneLights->GetSRV())
		return false;
	const uint32 Width = static_cast<uint32>(View.ViewRect.Width);
	const uint32 Height = static_cast<uint32>(View.ViewRect.Height);
	if (!Width || !Height || View.NearClip <= 0.0f || View.FarClip <= View.NearClip)
		return false;

	const uint32 TilesX = (Width + ClusterTileSize - 1) / ClusterTileSize;
	const uint32 TilesY = (Height + ClusterTileSize - 1) / ClusterTileSize;
	const uint64 RequiredClusters = static_cast<uint64>(TilesX) * TilesY * ClusterDepthSlices;
	if (RequiredClusters > UINT32_MAX / (ClusterLightCapacity * sizeof(uint32)))
		return false;
	const uint32 ClusterCount = static_cast<uint32>(RequiredClusters);
	if (ClusterCount > Capacity)
	{
		ComPtr<ID3D11Buffer> NewCounts, NewIndices;
		ComPtr<ID3D11ShaderResourceView> NewCountsSRV, NewIndicesSRV;
		ComPtr<ID3D11UnorderedAccessView> NewCountsUAV, NewIndicesUAV;
		ID3D11Device* Device = RenderCommand::GetDevice();
		if (!CreateClusterBuffer(Device, ClusterCount, NewCounts, NewCountsSRV, NewCountsUAV) ||
			!CreateClusterBuffer(Device, ClusterCount * ClusterLightCapacity,
				NewIndices, NewIndicesSRV, NewIndicesUAV))
			return false;
		Counts = std::move(NewCounts);
		Indices = std::move(NewIndices);
		CountsSRV = std::move(NewCountsSRV);
		IndicesSRV = std::move(NewIndicesSRV);
		CountsUAV = std::move(NewCountsUAV);
		IndicesUAV = std::move(NewIndicesUAV);
		Capacity = ClusterCount;
	}

	const FForwardClusterConstants Constants = MakeClusterConstants(View, Lighting, true);

	RenderCommand::GetContext()->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &Constants, 0, 0);
	DispatchX = TilesX;
	DispatchY = TilesY;
	return true;
}

void FForwardLightClusterGrid::ExecuteBuild(FRenderer* Renderer, const FStructuredBuffer* SceneLights)
{
	if (!Renderer || !SceneLights || !DispatchX || !DispatchY) return;
	ID3D11UnorderedAccessView* UAVs[] = { CountsUAV.Get(), IndicesUAV.Get() };
	Renderer->DispatchCompute(ComputeShader.Get(), ConstantsCB.Get(), 0, SceneLights->GetSRV(),
		UAVs, DispatchX, DispatchY, ClusterDepthSlices);
}

void FForwardLightClusterGrid::PreparePixelShader(const FSceneView& View,
	const FForwardLightingConstants& Lighting, bool bBuildSucceeded)
{
	bPixelClustersValid = ConstantsCB && bBuildSucceeded && CountsSRV && IndicesSRV;
	if (!ConstantsCB || bPixelClustersValid)
		return;

	const FForwardClusterConstants Constants = MakeClusterConstants(View, Lighting, false);
	RenderCommand::GetContext()->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &Constants, 0, 0);
}
