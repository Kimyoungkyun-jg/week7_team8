#include "EnginePCH.h"
#include "ImageLoader.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>

#define STB_IMAGE_IMPLEMENTATION   // 이미 다른 곳에 있으면 여기선 빼세요
#include "stb_image.h"

namespace
{
	constexpr uint32 FourCC(char A, char B, char C, char D)
	{
		return uint32(uint8(A)) | (uint32(uint8(B)) << 8) |
			(uint32(uint8(C)) << 16) | (uint32(uint8(D)) << 24);
	}

	struct FDdsPixelFormat
	{
		uint32 Size, Flags, FourCC, RGBBitCount, RMask, GMask, BMask, AMask;
	};
	struct FDdsHeader
	{
		uint32 Size, Flags, Height, Width, PitchOrLinearSize, Depth, MipMapCount;
		uint32 Reserved[11];
		FDdsPixelFormat PixelFormat;
		uint32 Caps, Caps2, Caps3, Caps4, Reserved2;
	};
	struct FDdsHeaderDX10
	{
		uint32 Format, Dimension, MiscFlag, ArraySize, MiscFlags2;
	};
	static_assert(sizeof(FDdsHeader) == 124);
	static_assert(sizeof(FDdsHeaderDX10) == 20);

	bool GetDDSFormat(const FDdsHeader& Header, const uint8* FileData, size_t FileSize,
		DXGI_FORMAT& OutFormat, uint32& OutBlockBytes, uint32& OutBytesPerPixel, size_t& OutDataOffset)
	{
		OutDataOffset = 4 + sizeof(FDdsHeader);
		const uint32 Code = Header.PixelFormat.FourCC;
		if ((Header.PixelFormat.Flags & 0x4u) != 0)
		{
			if (Code == FourCC('D', 'X', '1', '0'))
			{
				if (FileSize < OutDataOffset + sizeof(FDdsHeaderDX10)) return false;
				FDdsHeaderDX10 DX10{};
				std::memcpy(&DX10, FileData + OutDataOffset, sizeof(DX10));
				OutDataOffset += sizeof(DX10);
				if (DX10.Dimension != 3 || DX10.ArraySize != 1 || (DX10.MiscFlag & 0x4u) != 0)
					return false;
				OutFormat = static_cast<DXGI_FORMAT>(DX10.Format);
			}
			else if (Code == FourCC('D', 'X', 'T', '1')) OutFormat = DXGI_FORMAT_BC1_UNORM;
			else if (Code == FourCC('D', 'X', 'T', '3')) OutFormat = DXGI_FORMAT_BC2_UNORM;
			else if (Code == FourCC('D', 'X', 'T', '5')) OutFormat = DXGI_FORMAT_BC3_UNORM;
			else if (Code == FourCC('A', 'T', 'I', '1') || Code == FourCC('B', 'C', '4', 'U')) OutFormat = DXGI_FORMAT_BC4_UNORM;
			else if (Code == FourCC('A', 'T', 'I', '2') || Code == FourCC('B', 'C', '5', 'U')) OutFormat = DXGI_FORMAT_BC5_UNORM;
			else return false;
		}
		else if (Header.PixelFormat.RGBBitCount == 32 &&
			Header.PixelFormat.RMask == 0x00ff0000 && Header.PixelFormat.GMask == 0x0000ff00 &&
			Header.PixelFormat.BMask == 0x000000ff)
			OutFormat = Header.PixelFormat.AMask == 0xff000000 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM;
		else if (Header.PixelFormat.RGBBitCount == 32 &&
			Header.PixelFormat.RMask == 0x000000ff && Header.PixelFormat.GMask == 0x0000ff00 &&
			Header.PixelFormat.BMask == 0x00ff0000 && Header.PixelFormat.AMask == 0xff000000)
			OutFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		else return false;

		switch (OutFormat)
		{
		case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
			OutBlockBytes = 8; return true;
		case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
			OutBlockBytes = 16; return true;
		case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
			OutBytesPerPixel = 4; return true;
		default: return false;
		}
	}
}

//int Width, Height, Channels;
//float* Pixels = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 4);
//if (!Pixels) { HTR_LOG(Error, "[HDR] load failed: {}", Path); return nullptr; }
//
//D3D11_TEXTURE2D_DESC Desc{};
//Desc.Width = Width;
//Desc.Height = Height;
//Desc.MipLevels = 1;  
//Desc.ArraySize = 1;
//Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
//Desc.SampleDesc.Count = 1;
//Desc.Usage = D3D11_USAGE_DEFAULT;
//Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//
//PanoramaTexture = RenderCommand::CreateTexture2D(Desc, Pixels);
//stbi_image_free(Pixels);

FImageData ImageLoader::Load(const FString& Path)
{
	FImageData Result;

	int Width = 0, Height = 0, Channels = 0;
	unsigned char* Pixels = stbi_load(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		OutputDebugStringA(("[Texture] stbi_load failed: " + Path + " (" + stbi_failure_reason() + ")\n").c_str());
		return FImageData();
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4;
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);

	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 4;
	Result.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	return Result;
}

FImageData ImageLoader::LoadHDR(const FString& Path)
{
	FImageData Result;

	stbi_set_flip_vertically_on_load(false);
	int Width = 0, Height = 0, Channels = 0;
	float* Pixels = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		HTR_LOG(Error, "[Image] HDR load failed: {} ({})", Path, stbi_failure_reason());
		return Result;
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4 * sizeof(float);
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);
	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 16;
	Result.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	return Result;
}

FImageData ImageLoader::LoadDDS(const FString& Path)
{
	std::ifstream In(Path, std::ios::binary | std::ios::ate);
	if (!In) return {};
	const std::streamoff FileSize = In.tellg();
	if (FileSize < std::streamoff(4 + sizeof(FDdsHeader)) ||
		FileSize > std::numeric_limits<int32>::max()) return {};
	TArray<uint8> File;
	File.SetNum(static_cast<int32>(FileSize));
	In.seekg(0);
	if (!In.read(reinterpret_cast<char*>(File.GetData()), FileSize)) return {};
	if (std::memcmp(File.GetData(), "DDS ", 4) != 0) return {};
	FDdsHeader Header{};
	std::memcpy(&Header, File.GetData() + 4, sizeof(Header));
	if (Header.Size != 124 || Header.PixelFormat.Size != 32 || Header.Width == 0 ||
		Header.Height == 0 || Header.Width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		Header.Height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		(Header.Caps2 & (0x200u | 0x200000u)) != 0 || Header.Depth > 1)
		return {};

	FImageData Result;
	uint32 BlockBytes = 0;
	size_t DataOffset = 0;
	if (!GetDDSFormat(Header, File.GetData(), static_cast<size_t>(FileSize),
		Result.Format, BlockBytes, Result.BytesPerPixel, DataOffset)) return {};
	Result.Width = Header.Width;
	Result.Height = Header.Height;
	Result.MipLevels = Header.MipMapCount == 0 ? 1 : Header.MipMapCount;
	if (Result.MipLevels > D3D11_REQ_MIP_LEVELS) return {};
	uint32 Width = Result.Width, Height = Result.Height;
	size_t Offset = DataOffset;
	for (uint32 Level = 0; Level < Result.MipLevels; ++Level)
	{
		const size_t RowPitch = BlockBytes
			? size_t(std::max(1u, (Width + 3) / 4)) * BlockBytes
			: size_t(Width) * Result.BytesPerPixel;
		const size_t Rows = BlockBytes ? std::max(1u, (Height + 3) / 4) : Height;
		const size_t ByteCount = RowPitch * Rows;
		if (RowPitch > std::numeric_limits<uint32>::max() || ByteCount > size_t(FileSize) - Offset ||
			Offset - DataOffset > std::numeric_limits<uint32>::max()) return {};
		Result.MipOffsets.Add(static_cast<uint32>(Offset - DataOffset));
		Result.MipRowPitches.Add(static_cast<uint32>(RowPitch));
		Offset += ByteCount;
		Width = std::max(1u, Width / 2);
		Height = std::max(1u, Height / 2);
	}
	Result.Pixels.SetNum(static_cast<int32>(Offset - DataOffset));
	std::memcpy(Result.Pixels.GetData(), File.GetData() + DataOffset, Result.Pixels.Num());
	return Result;
}

FImageData ImageLoader::LoadAuto(const FString& Path)
{
	FString Ext = fs::path(Path).extension().string();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char C) { return char(std::tolower(C)); });
	if (Ext == ".hdr") return LoadHDR(Path);
	if (Ext == ".dds")
	{
		FImageData Image = LoadDDS(Path);
		if (!Image.IsValid()) HTR_LOG(Warning, "[Image] unsupported or invalid DDS: {}", Path);
		return Image;
	}
	return Load(Path);
}
