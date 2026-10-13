#pragma once

#include "Core/Types.h"
#include <d3d11.h>
#include <wrl/client.h>

class FStructuredBuffer;
class FRenderer;
struct FSceneView;
struct FForwardLightingConstants;

class FForwardLightClusterGrid
{
public:
	bool Init();
	bool PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting,
		const FStructuredBuffer* SceneLights);
	void ExecuteBuild(FRenderer* Renderer, const FStructuredBuffer* SceneLights);
	ID3D11Resource* GetCountsResource() const { return Counts.Get(); }
	ID3D11Resource* GetIndicesResource() const { return Indices.Get(); }
	void PreparePixelShader(const FSceneView& View,
		const FForwardLightingConstants& Lighting, bool bBuildSucceeded);
	ID3D11Buffer* GetPixelConstants() const { return ConstantsCB.Get(); }
	ID3D11ShaderResourceView* GetCountsSRV() const { return bPixelClustersValid ? CountsSRV.Get() : nullptr; }
	ID3D11ShaderResourceView* GetIndicesSRV() const { return bPixelClustersValid ? IndicesSRV.Get() : nullptr; }
	void ResetPixelState() { bPixelClustersValid = false; }

private:
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantsCB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> Counts;
	Microsoft::WRL::ComPtr<ID3D11Buffer> Indices;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CountsSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> IndicesSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> CountsUAV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> IndicesUAV;
	uint32 Capacity = 0;
	uint32 DispatchX = 0;
	uint32 DispatchY = 0;
	bool bPixelClustersValid = false;
};
