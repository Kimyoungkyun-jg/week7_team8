#pragma once

#include "Core/Types.h"
#include "Render/Buffer.h"
#include "Render/LightRenderData.h"
#include <array>

class FRenderer;
class FTexture2D;
struct FSceneView;

// 뷰별 타일 목록과 상수만 소유한다. GPU 명령은 FRenderer에 제출한다.
class FForwardLightTileGrid
{
public:
	bool Init();
	bool PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting,
		const FStructuredBuffer* SceneLights, const FTexture2D* Depth);
	bool ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights);
	void PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded);
	FConstantBuffer* GetPixelConstants() const { return LightingInfoCB.get(); }
	std::array<ID3D11Resource*, 4> GetWriteResources() const;
	std::array<ID3D11ShaderResourceView*, 3> GetPixelResources() const;
	void ResetPixelState() { bPixelTilesValid = false; }

private:
	struct FTileCullingConstants
	{
		FMatrix View;
		FVector4 ProjectionParams;
		uint32 ViewWidth = 0, ViewHeight = 0, TileCountX = 0, IsPerspective = 0;
		uint32 PointLightCount = 0, SpotLightCount = 0, PointIndexCapacity = 0, SpotIndexCapacity = 0;
		uint32 LightOffsets[4]{};
	};
	static_assert(sizeof(FTileCullingConstants) == 128);

	struct FTileBuffer
	{
		ComPtr<ID3D11Buffer> Buffer;
		ComPtr<ID3D11ShaderResourceView> SRV;
		ComPtr<ID3D11UnorderedAccessView> UAV;
		uint32 Capacity = 0;
		bool EnsureCapacity(uint32 Count, uint32 Stride);
	};

	ComPtr<ID3D11ComputeShader> ComputeShader;
	TUniquePtr<FConstantBuffer> CullingCB;
	TUniquePtr<FConstantBuffer> LightingInfoCB;
	FTileBuffer Ranges, PointIndices, SpotIndices, Allocator;
	FTileCullingConstants Constants{};
	uint32 TileCountY = 0;
	bool bPixelTilesValid = false;
};
