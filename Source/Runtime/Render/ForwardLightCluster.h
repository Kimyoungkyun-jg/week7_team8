#pragma once

#include "Core/Types.h"
#include <d3d11.h>
#include <wrl/client.h>

class FStructuredBuffer;
struct FSceneView;
struct FForwardLightingConstants;

class FForwardLightClusterGrid
{
public:
	bool Init();
	bool Build(const FSceneView& View, const FForwardLightingConstants& Lighting,
		const FStructuredBuffer* SceneLights);
	void PreparePixelShader(const FSceneView& View,
		const FForwardLightingConstants& Lighting, bool bBuildSucceeded);
	void BindPixelShader() const;
	void UnbindPixelShader();

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
	bool bPixelClustersValid = false;
};
