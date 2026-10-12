#pragma once

#include <d3d11.h>

struct FVertex
{
	FVector Position = FVector();
	FVector2 UV = FVector2();
	FVector4 Color = FVector4();
};

struct FTextVertex
{
	FVector Position;
	FVector2 UV;
};

// Todo: subuv
struct FParticleVertex
{
	FVector Position;
	FVector2 UV;
};

// StaticMesh 정점: 기존 입력 순서를 유지하고 접선을 마지막에 둔다.
struct FVertexPNCT
{
	FVector Position = FVector();
	FVector Normal = FVector();
	FVector4 Color = FVector4();
	FVector2 UV = FVector2();
	FVector4 Tangent = FVector4(); // xyz: 접선, w: bitangent 방향 부호
};
