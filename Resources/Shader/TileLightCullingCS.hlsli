#pragma pack_matrix(row_major)

#define LIGHT_TILE_SIZE 32
#define THREADS_PER_TILE (LIGHT_TILE_SIZE * LIGHT_TILE_SIZE)

// CPU 렌더 데이터의 크기와 필드 순서를 유지한다.
struct FSceneLightRenderInfo
{
    float4 PositionRadius;
    float4 DirectionOuterCos;
    float4 ColorIntensity;
    float4 Parameters;
};

cbuffer TileCullingConstants : register(b0)
{
    matrix View;
    float4 ProjectionParams; // xy: 역 투영 스케일, zw: 깊이 복원 A·B.
    uint ViewWidth;
    uint ViewHeight;
    uint TileCountX;
    uint IsPerspective;
    uint PointLightCount;
    uint SpotLightCount;
    uint PointIndexCapacity;
    uint SpotIndexCapacity;
    uint4 LightOffsets;
};

Texture2D<float> SceneDepth : register(t0);
StructuredBuffer<FSceneLightRenderInfo> SceneLights : register(t1);

// x/y: Point 시작·개수, z/w: Spot 시작·개수.
RWStructuredBuffer<uint4> TileLightRanges : register(u0);
RWStructuredBuffer<uint> TilePointIndices : register(u1);
RWStructuredBuffer<uint> TileSpotIndices : register(u2);

// 0번 원소는 Point, 1번 원소는 Spot의 다음 예약 위치다.
RWStructuredBuffer<uint> TileListAllocator : register(u3);
#define INVALID_LIGHT_LIST 0xffffffffu

// 그룹이 공유하는 변수들
groupshared uint MinDepthBits;
groupshared uint MaxDepthBits;
groupshared uint VisiblePointCount;
groupshared uint VisibleSpotCount;
groupshared float3 TileBoundsMin;
groupshared float3 TileBoundsMax;
groupshared uint PointListOffset;
groupshared uint SpotListOffset;
groupshared uint PointWriteCount;
groupshared uint SpotWriteCount;

float ConvertFromDeviceZ(float DeviceZ)
{
    // 기존 SceneDepthShader와 같은 +X 선형 뷰 깊이를 복원한다.
    return IsPerspective != 0 ? ProjectionParams.w / (DeviceZ - ProjectionParams.z) : (DeviceZ - ProjectionParams.w) / ProjectionParams.z;
}

float3 ReconstructViewCorner(float2 PixelCorner, float ViewDepth)
{
    // 픽셀 중심이 아닌 타일 경계를 현재 투영 규칙으로 복원한다.
    float2 NDC = PixelCorner / float2(ViewWidth, ViewHeight) * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float ProjectionW = IsPerspective != 0 ? ViewDepth : 1.0f;
    return float3(ViewDepth, NDC * ProjectionParams.xy * ProjectionW);
}

bool IntersectsTile(float4 PositionRadius)
{
    // 카메라 View는 강체 변환이므로 광원의 반경은 변하지 않는다.
    float3 Center = mul(float4(PositionRadius.xyz, 1.0f), View).xyz;
    float3 Delta = Center - clamp(Center, TileBoundsMin, TileBoundsMax);
    return dot(Delta, Delta) <= PositionRadius.w * PositionRadius.w;
}

bool IntersectsSpotTile(FSceneLightRenderInfo Light)
{
    // 조명 계산에서 사용하는 최소 거리 반경과 일치시킨다.
    float Radius = max(Light.PositionRadius.w, 0.001f);

    // 이동한 포함 구가 거리 제한 밖으로 뻗는 부분도 있으므로 기존 거리 검사를 유지한다.
    if (!IntersectsTile(float4(Light.PositionRadius.xyz, Radius)))
        return false;

    float CosOuter = saturate(Light.DirectionOuterCos.w);
    float Offset;
    float SphereRadius;
    if (CosOuter >= 0.7071067812f)
    {
        // 반각 45도 이하에서는 광원 시작점과 바깥 테두리에 접하는 구를 만든다.
        Offset = Radius / (2.0f * CosOuter);
        SphereRadius = Offset;
    }
    else
    {
        // 넓은 Spot은 거리 제한 경계의 바깥 테두리를 중심으로 감싼다.
        Offset = Radius * CosOuter;
        SphereRadius = Radius * sqrt(saturate(1.0f - CosOuter * CosOuter));
    }

    // 실제 광원 위치는 유지하고 컬링에 사용할 구 중심만 앞쪽으로 이동한다.
    float3 Direction = normalize(Light.DirectionOuterCos.xyz);
    float3 Center = Light.PositionRadius.xyz + Direction * Offset;
    return IntersectsTile(float4(Center, SphereRadius));
}

uint ReserveTileList(uint Kind, uint Count, uint Capacity)
{
    // 빈 목록은 공간을 예약하지 않는다.
    if (Count == 0)
        return 0;

    uint Start;
    InterlockedAdd(TileListAllocator[Kind], 0, Start);

    // 카운터가 용량 안에 머물도록 비교 후 교환으로 구간을 예약한다.
    [allow_uav_condition]
    while (true)
    {
        if (Count > Capacity - Start)
            return INVALID_LIGHT_LIST;

        uint Previous;
        InterlockedCompareExchange(TileListAllocator[Kind], Start, Start + Count, Previous);
        if (Previous == Start)
            return Start;

        // 다른 그룹이 먼저 예약했다면 변경된 위치에서 다시 시도한다.
        Start = Previous;
    }
}

[numthreads(LIGHT_TILE_SIZE, LIGHT_TILE_SIZE, 1)]
void mainCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{
    uint TileIndex = GroupID.y * TileCountX + GroupID.x;
    if (ThreadIndex == 0)
    {
        MinDepthBits = asuint(1.0f);
        MaxDepthBits = asuint(0.0f);
        VisiblePointCount = 0;
        VisibleSpotCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();  // 동기화

    uint2 Pixel = GroupID.xy * LIGHT_TILE_SIZE + GroupThreadID.xy;
    if (Pixel.x < ViewWidth && Pixel.y < ViewHeight)
    {
        float DeviceZ = SceneDepth.Load(int3(Pixel, 0));
        if (DeviceZ < 1.0f)
        {
            // 비음수 float의 비트 순서를 이용해 uint 원자 연산으로 최소·최대를 구한다.
            InterlockedMin(MinDepthBits, asuint(DeviceZ));
            InterlockedMax(MaxDepthBits, asuint(DeviceZ));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 배경뿐인 타일도 이번 프레임의 빈 메타데이터를 기록한다.
    if (MinDepthBits == asuint(1.0f))
    {
        if (ThreadIndex == 0)
            TileLightRanges[TileIndex] = uint4(0, 0, 0, 0);
        return;
    }

    if (ThreadIndex == 0)
    {
        // 현재 D24 깊이의 양자화 오차를 포함하고 양끝 두 값만 선형화한다.
        const float DepthQuantum = 1.0f / 16777215.0f;
        float MinDepth = ConvertFromDeviceZ(max(asfloat(MinDepthBits) - DepthQuantum, 0.0f));
        float MaxDepth = ConvertFromDeviceZ(min(asfloat(MaxDepthBits) + DepthQuantum, 1.0f));

        float2 PixelMin = float2(GroupID.xy * LIGHT_TILE_SIZE);
        float2 PixelMax = min(PixelMin + LIGHT_TILE_SIZE, float2(ViewWidth, ViewHeight));
        TileBoundsMin = ReconstructViewCorner(PixelMin, MinDepth);
        TileBoundsMax = TileBoundsMin;

        // 깊이 양끝의 네 모서리로 원근·직교 카메라에 공통인 AABB를 만든다.
        [unroll]
        for (uint CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            float2 Corner = float2((CornerIndex & 1) != 0 ? PixelMax.x : PixelMin.x, (CornerIndex & 2) != 0 ? PixelMax.y : PixelMin.y);
            float Depth = (CornerIndex & 4) != 0 ? MaxDepth : MinDepth;
            float3 Position = ReconstructViewCorner(Corner, Depth);
            TileBoundsMin = min(TileBoundsMin, Position);
            TileBoundsMax = max(TileBoundsMax, Position);
        }
        TileBoundsMin -= 1e-4f;
        TileBoundsMax += 1e-4f;
    }
    GroupMemoryBarrierWithGroupSync();

    // 첫 번째 검사에서는 실제 후보 개수만 센다.
    for (uint PointIndex = ThreadIndex; PointIndex < PointLightCount; PointIndex += THREADS_PER_TILE)
        if (IntersectsTile(SceneLights[LightOffsets.x + PointIndex].PositionRadius))
            InterlockedAdd(VisiblePointCount, 1);

    for (uint SpotIndex = ThreadIndex; SpotIndex < SpotLightCount; SpotIndex += THREADS_PER_TILE)
        if (IntersectsSpotTile(SceneLights[LightOffsets.y + SpotIndex]))
            InterlockedAdd(VisibleSpotCount, 1);
    GroupMemoryBarrierWithGroupSync();

    if (ThreadIndex == 0)
    {
        // 실제 후보 개수만큼 서로 겹치지 않는 연속 구간을 예약한다.
        PointListOffset = ReserveTileList(0, VisiblePointCount, PointIndexCapacity);
        SpotListOffset = ReserveTileList(1, VisibleSpotCount, SpotIndexCapacity);
        PointWriteCount = 0;
        SpotWriteCount = 0;

        // 예약 실패 시에도 실제 개수는 남겨 조명 fallback과 시각화에 사용한다.
        TileLightRanges[TileIndex] =
            uint4(PointListOffset, VisiblePointCount, SpotListOffset, VisibleSpotCount);
    }
    GroupMemoryBarrierWithGroupSync();

    // 두 번째 검사에서 Point의 원본 배열 인덱스를 예약 구간에 기록한다.
    if (PointListOffset != INVALID_LIGHT_LIST)
    {
        for (uint PointIndex = ThreadIndex; PointIndex < PointLightCount; PointIndex += THREADS_PER_TILE)
        {
            if (IntersectsTile(SceneLights[LightOffsets.x + PointIndex].PositionRadius))
            {
                uint Slot;
                InterlockedAdd(PointWriteCount, 1, Slot);
                TilePointIndices[PointListOffset + Slot] = PointIndex;
            }
        }
    }

    // Spot도 자기 버퍼에서 별도로 예약한 구간에 기록한다.
    if (SpotListOffset != INVALID_LIGHT_LIST)
    {
        for (uint SpotIndex = ThreadIndex; SpotIndex < SpotLightCount; SpotIndex += THREADS_PER_TILE)
        {
            if (IntersectsSpotTile(SceneLights[LightOffsets.y + SpotIndex]))
            {
                uint Slot;
                InterlockedAdd(SpotWriteCount, 1, Slot);
                TileSpotIndices[SpotListOffset + Slot] = SpotIndex;
            }
        }
    }
}
