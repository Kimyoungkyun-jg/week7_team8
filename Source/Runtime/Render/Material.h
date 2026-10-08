#pragma once

#include "MaterialInterface.h"
#include "RenderResourceManager.h"
#include "Render/Texture2D.h"

class FShader;

struct FStaticMeshMaterialParams
{
	FVector4 BaseColor;
	FVector2 UVOffset;
	FVector2 Padding;
};

class UMaterial : public UMaterialInterface
{
	DECLARE_CLASS(UMaterial, UMaterialInterface)

public:
	UMaterial() :SortID(NextSortID++) {};
	virtual ~UMaterial() override = default;

	TArray<UTexture2D*> Textures;
	ESamplerState SamplerState = ESamplerState::LinearClamp;
	
	TUniquePtr<FConstantBuffer> ParamBuffer;

	EPSOType PSOType = EPSOType::StaticMesh_Opaque;



	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FVector2 UVScrollSpeed = FVector2(0.0f, 0.0f);

	bool bIsInstance = false;

	// 인스턴스가 복제된 원본. 인스턴스는 경로가 없어서, 저장할 때 원본을 따라가 기준 에셋을 찾는다
	const UMaterial* Parent = nullptr;

	// Source의 내용을 복사한 편집용 복제본을 만든다
	static UMaterial* CreateInstance(const UMaterial* Source);

	// 경로가 있는(=에셋으로 등록된) 가장 가까운 원본. 자신이 에셋이면 자신
	const UMaterial* GetBaseAsset() const;

	static json SaveMaterial(const UMaterial* Material);
	static UMaterial* LoadMaterial(const json& In);

	const uint16 SortID;
private:
	inline static uint16 NextSortID = 0;
};