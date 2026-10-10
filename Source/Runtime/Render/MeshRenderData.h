#pragma once

#include "Container/Array.h"
#include "Math/Box.h"
#include "Math/Sphere.h"

class UMaterial;

struct FCachedMeshSection
{
    UMaterial* Material;
    uint32 StartIndex;
    uint32 IndexCount;
};

struct FCachedMeshLOD
{
    uint32 FirstSection = 0;
    uint32 NumSections = 0;
};

// 메시와 확정된 머티리얼 슬롯이 같은 객체끼리 공유한다.
// 이 값들은 에셋 속성이며, 가시성이나 LOD 판정 결과를 저장하지 않는다.
struct FMeshRenderState
{
    TArray<FCachedMeshSection> Sections;
    FCachedMeshLOD LODs[4];
    float LODThresholdSq[3]{};
    uint8 LODCount = 1;
};

struct FLODSelectionInput
{
    FLODSphere Sphere;
    const FMeshRenderState* State = nullptr;
};
