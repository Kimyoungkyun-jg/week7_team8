#include "EnginePCH.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/ImageLoader.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"

namespace
{
	constexpr uint32 EnvironmentSize = 128, IrradianceSize = 32, IntegrationSamples = 512;
	struct FSkyConstants
	{
		uint32 OutputSize, PanoramaHeight, bSourceIsLinear, SampleCount;
	};
	static_assert(sizeof(FSkyConstants) == 16);

	bool CompileCompute(ID3D11Device* Device, const char* Entry, ComPtr<ID3D11ComputeShader>& Shader)
	{
		ComPtr<ID3DBlob> Code, Errors;
		HRESULT Hr = D3DCompileFromFile(L"Resources/Shader/SkyLightConvolutionCS.hlsli",
			nullptr, nullptr, Entry, "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
			Code.GetAddressOf(), Errors.GetAddressOf());
		if (FAILED(Hr))
		{
			if (Errors) HTR_LOG(Error, "[SkyLight] {}: {}", Entry,
				static_cast<const char*>(Errors->GetBufferPointer()));
			else HTR_LOG(Error, "[SkyLight] {} compile failed: 0x{:08X}", Entry, uint32(Hr));
			return false;
		}
		return SUCCEEDED(Device->CreateComputeShader(Code->GetBufferPointer(),
			Code->GetBufferSize(), nullptr, Shader.GetAddressOf()));
	}

	TUniquePtr<FTextureCube> CreateCube(uint32 Size)
	{
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Desc.Height = Size;
		Desc.MipLevels = 1;
		Desc.ArraySize = FTextureCube::FaceCount;
		Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		auto Cube = RenderCommand::CreateTextureCube(Desc, {});
		return Cube && Cube->GetSRV() && Cube->GetUAV() ? std::move(Cube) : nullptr;
	}
}

bool FSkyLightEnvironment::Build(const FString& InSourcePath)
{
	FImageData Image = ImageLoader::LoadAuto(InSourcePath);
	if (!Image.IsValid() || (Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		Image.Format != DXGI_FORMAT_R32G32B32A32_FLOAT))
	{
		HTR_LOG(Warning, "[SkyLight] unsupported panorama: {}", InSourcePath);
		return false;
	}

	D3D11_TEXTURE2D_DESC PanoramaDesc{};
	PanoramaDesc.Width = Image.Width;
	PanoramaDesc.Height = Image.Height;
	PanoramaDesc.MipLevels = Image.MipLevels;
	PanoramaDesc.ArraySize = 1;
	PanoramaDesc.Format = Image.Format;
	PanoramaDesc.SampleDesc.Count = 1;
	PanoramaDesc.Usage = D3D11_USAGE_IMMUTABLE;
	PanoramaDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	auto Panorama = RenderCommand::CreateTexture2D(PanoramaDesc, Image);
	auto NewEnvironment = CreateCube(EnvironmentSize);
	auto NewIrradiance = CreateCube(IrradianceSize);
	if (!Panorama || !Panorama->GetSRV() || !NewEnvironment || !NewIrradiance)
	{
		HTR_LOG(Warning, "[SkyLight] GPU texture creation failed: {}", InSourcePath);
		return false;
	}

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	ComPtr<ID3D11ComputeShader> EnvironmentCS, IrradianceCS;
	if (!CompileCompute(Device, "BuildEnvironmentCube", EnvironmentCS) ||
		!CompileCompute(Device, "BuildIrradianceCube", IrradianceCS)) return false;

	D3D11_BUFFER_DESC CBDesc{};
	CBDesc.ByteWidth = sizeof(FSkyConstants);
	CBDesc.Usage = D3D11_USAGE_DEFAULT;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ComPtr<ID3D11Buffer> ConstantsCB;
	if (FAILED(Device->CreateBuffer(&CBDesc, nullptr, ConstantsCB.GetAddressOf()))) return false;

	D3D11_SAMPLER_DESC SamplerDesc{};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> Sampler;
	if (FAILED(Device->CreateSamplerState(&SamplerDesc, Sampler.GetAddressOf()))) return false;

	ID3D11Buffer* CB = ConstantsCB.Get();
	ID3D11SamplerState* SamplerPtr = Sampler.Get();
	Context->CSSetConstantBuffers(0, 1, &CB);
	Context->CSSetSamplers(0, 1, &SamplerPtr);

	// 파노라마를 환경 큐브맵 여섯 면에 직접 기록한다.
	const FSkyConstants EnvConstants{ EnvironmentSize, Image.Height,
		Image.Format == DXGI_FORMAT_R8G8B8A8_UNORM ? 0u : 1u, IntegrationSamples };
	Context->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &EnvConstants, 0, 0);
	ID3D11ShaderResourceView* PanoramaSRV = Panorama->GetSRV();
	ID3D11UnorderedAccessView* EnvironmentUAV = NewEnvironment->GetUAV();
	Context->CSSetShader(EnvironmentCS.Get(), nullptr, 0);
	Context->CSSetShaderResources(0, 1, &PanoramaSRV);
	Context->CSSetUnorderedAccessViews(0, 1, &EnvironmentUAV, nullptr);
	Context->Dispatch(EnvironmentSize / 8, EnvironmentSize / 8, FTextureCube::FaceCount);

	// 읽기 전 UAV 바인딩을 풀어 동일 리소스의 SRV/UAV 충돌을 막는다.
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	Context->CSSetShaderResources(0, 1, &NullSRV);
	Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);

	const FSkyConstants IrrConstants{ IrradianceSize, Image.Height, 1u, IntegrationSamples };
	Context->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &IrrConstants, 0, 0);
	ID3D11ShaderResourceView* EnvironmentSRV = NewEnvironment->GetSRV();
	ID3D11UnorderedAccessView* IrradianceUAV = NewIrradiance->GetUAV();
	Context->CSSetShader(IrradianceCS.Get(), nullptr, 0);
	Context->CSSetShaderResources(1, 1, &EnvironmentSRV);
	Context->CSSetUnorderedAccessViews(0, 1, &IrradianceUAV, nullptr);
	Context->Dispatch(IrradianceSize / 8, IrradianceSize / 8, FTextureCube::FaceCount);

	Context->CSSetShaderResources(1, 1, &NullSRV);
	Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
	ID3D11Buffer* NullCB = nullptr;
	ID3D11SamplerState* NullSampler = nullptr;
	Context->CSSetConstantBuffers(0, 1, &NullCB);
	Context->CSSetSamplers(0, 1, &NullSampler);
	Context->CSSetShader(nullptr, nullptr, 0);

	EnvironmentCube = std::move(NewEnvironment);
	IrradianceCube = std::move(NewIrradiance);
	SourcePath = InSourcePath;
	HTR_LOG(Info, "[SkyLight] GPU environment {}x{}, irradiance {}x{}: {}",
		EnvironmentSize, EnvironmentSize, IrradianceSize, IrradianceSize, SourcePath);
	return true;
}
