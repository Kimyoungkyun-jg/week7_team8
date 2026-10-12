#pragma once

struct FImageData
{
	FImageData() = default;

	bool IsValid() const { return Width > 0 && Height > 0 && Format != DXGI_FORMAT_UNKNOWN && !Pixels.IsEmpty(); }

	// 행 하나의 바이트 수. FTexture2D가 SysMemPitch로 쓴다.
	uint32 GetRowPitch() const { return MipRowPitches.IsEmpty() ? Width * BytesPerPixel : MipRowPitches[0]; }

	TArray<uint8> Pixels;
	TArray<uint32> MipOffsets;
	TArray<uint32> MipRowPitches;
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 BytesPerPixel = 0;
	uint32 MipLevels = 1;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class ImageLoader
{
public:
	static FImageData Load(const FString& Path);
	static FImageData LoadHDR(const FString& Path);
	static FImageData LoadDDS(const FString& Path);
	static FImageData LoadAuto(const FString& Path);
};
