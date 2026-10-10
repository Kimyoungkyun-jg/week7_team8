#pragma pack_matrix(row_major)

#define CLUSTER_TILE_SIZE 32
#define CLUSTER_LIGHT_CAPACITY 64

struct FSceneLightRenderInfo
{
    float4 PositionRadius;
    float4 DirectionOuterCos;
    float4 ColorIntensity;
    float4 Parameters;
};

cbuffer ClusterConstants : register(b0)
{
    matrix InverseViewProjection;
    float4 CameraPosition;
    float4 CameraForward;
    uint3 ClusterDimensions; // x: 가로 타일 수, y: 세로 타일 수, z: 깊이 구간 수
    uint bOrthographic;
    uint4 ViewportSize; // xy: 뷰포트 크기
    uint4 LightCounts;
    uint4 LightOffsets;
    float4 DepthRange; // x: 근거리, y: 원거리
    uint4 ClusterOptions; // x: 빌드 성공, y: 타일 크기, z: 클러스터당 최대 라이트 수
};

StructuredBuffer<FSceneLightRenderInfo> SceneLights : register(t0);
RWStructuredBuffer<uint> ClusterCounts : register(u0);
RWStructuredBuffer<uint> ClusterLightIndices : register(u1);

groupshared float4 ClusterPlanes[6];

float3 Unproject(float2 NDC, float Depth)
{
    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    return WorldH.xyz / WorldH.w;
}

float3 CornerAtDepth(float2 NDC, float ViewDepth)
{
    float3 A = Unproject(NDC, 0.0f);
    float3 B = Unproject(NDC, 0.5f);
    float3 Ray = B - A;
    float RayDepth = dot(Ray, CameraForward.xyz);
    float StartDepth = dot(A - CameraPosition.xyz, CameraForward.xyz);
    return A + Ray * ((ViewDepth - StartDepth) / RayDepth);
}

float4 MakePlane(float3 A, float3 B, float3 C, float3 Inside)
{
    float3 N = normalize(cross(B - A, C - A));
    if (dot(N, Inside - A) < 0.0f)
        N = -N;
    return float4(N, -dot(N, A));
}

bool SphereIntersectsCluster(float3 Center, float Radius)
{
    [unroll]
    for (uint PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
    {
        float4 Plane = ClusterPlanes[PlaneIndex];
        if (dot(Plane.xyz, Center) + Plane.w < -Radius)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void BuildForwardLightClusters(uint3 Cell : SV_GroupID, uint Lane : SV_GroupIndex)
{
    if (Cell.x >= ClusterDimensions.x || Cell.y >= ClusterDimensions.y || Cell.z >= ClusterDimensions.z)
        return;

    uint ClusterID = (Cell.z * ClusterDimensions.y + Cell.y) * ClusterDimensions.x + Cell.x;
    if (Lane == 0)
    {
        ClusterCounts[ClusterID] = 0;

        float2 UVMin = float2(Cell.xy * CLUSTER_TILE_SIZE) / ViewportSize.xy;
        float2 UVMax = min(float2((Cell.xy + 1) * CLUSTER_TILE_SIZE), ViewportSize.xy) / ViewportSize.xy;
        float2 TL = float2(UVMin.x * 2.0f - 1.0f, 1.0f - UVMin.y * 2.0f);
        float2 TR = float2(UVMax.x * 2.0f - 1.0f, TL.y);
        float2 BL = float2(TL.x, 1.0f - UVMax.y * 2.0f);
        float2 BR = float2(TR.x, BL.y);

        float SliceNear = (float)Cell.z / ClusterDimensions.z;
        float SliceFar = (float)(Cell.z + 1) / ClusterDimensions.z;
        float LogDepthRange = log2(max(DepthRange.y / DepthRange.x, 1.0f));
        float NearDepth = bOrthographic != 0
            ? lerp(DepthRange.x, DepthRange.y, SliceNear)
            : DepthRange.x * exp2(LogDepthRange * SliceNear);
        float FarDepth = bOrthographic != 0
            ? lerp(DepthRange.x, DepthRange.y, SliceFar)
            : DepthRange.x * exp2(LogDepthRange * SliceFar);

        float3 P0 = CornerAtDepth(TL, NearDepth);
        float3 P1 = CornerAtDepth(TR, NearDepth);
        float3 P2 = CornerAtDepth(BL, NearDepth);
        float3 P3 = CornerAtDepth(BR, NearDepth);
        float3 P4 = CornerAtDepth(TL, FarDepth);
        float3 P5 = CornerAtDepth(TR, FarDepth);
        float3 P6 = CornerAtDepth(BL, FarDepth);
        float3 P7 = CornerAtDepth(BR, FarDepth);
        float3 Inside = (P0 + P1 + P2 + P3 + P4 + P5 + P6 + P7) * 0.125f;

        ClusterPlanes[0] = MakePlane(P0, P2, P6, Inside); // 왼쪽
        ClusterPlanes[1] = MakePlane(P1, P5, P7, Inside); // 오른쪽
        ClusterPlanes[2] = MakePlane(P0, P4, P5, Inside); // 위쪽
        ClusterPlanes[3] = MakePlane(P2, P3, P7, Inside); // 아래쪽
        ClusterPlanes[4] = MakePlane(P0, P1, P3, Inside); // 앞쪽
        ClusterPlanes[5] = MakePlane(P4, P6, P7, Inside); // 뒤쪽
    }
    AllMemoryBarrierWithGroupSync();

    uint LocalLightCount = LightCounts.x + LightCounts.y;
    [loop]
    for (uint LocalIndex = Lane; LocalIndex < LocalLightCount; LocalIndex += 64)
    {
        uint LightIndex = LightOffsets.x + LocalIndex;
        FSceneLightRenderInfo Light = SceneLights[LightIndex];
        float Radius = Light.PositionRadius.w;
        if (Radius <= 0.0f || !SphereIntersectsCluster(Light.PositionRadius.xyz, Radius))
            continue;

        uint Slot;
        InterlockedAdd(ClusterCounts[ClusterID], 1, Slot);
        if (Slot < CLUSTER_LIGHT_CAPACITY)
            ClusterLightIndices[ClusterID * CLUSTER_LIGHT_CAPACITY + Slot] = LightIndex;
    }
}
