#pragma pack_matrix(row_major)

// 셰이더 스캐너는 매크로 정의 없이 이 파일을 컴파일하므로 기본 변형에는 Lambert를 쓴다.
// 명시적인 변형은 컴파일할 때 조명 모델 하나만 1로 설정한다.
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG)
    #define LIGHTING_MODEL_LAMBERT 1
#endif

#if (LIGHTING_MODEL_GOURAUD + LIGHTING_MODEL_LAMBERT + LIGHTING_MODEL_PHONG) != 1
    #error Exactly one lighting model must be enabled.
#endif

struct FAmbientLightInfo
{
    float4 ColorIntensity; // rgb: 선형 색상, a: 광도
};

struct FSceneLightRenderInfo
{
    float4 PositionRadius;    // 방향광에서는 xyz가 빛의 진행 방향
    float4 DirectionOuterCos; // 스포트: xyz 진행 방향, w 바깥 각도의 코사인
    float4 ColorIntensity;    // rgb: 색상, w: 광도
    float4 Parameters;        // 포인트: x 거리 감쇠, 스포트: x 안쪽 각도의 코사인, y 거리 감쇠
};

cbuffer PerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix NormalMatrix; // World 행렬의 역행렬을 전치한 법선 변환 행렬
};

cbuffer Lighting : register(b1)
{
    FAmbientLightInfo Ambient;
    float4 CameraPosition;
    uint4 LightCounts; // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수
    uint4 LightOffsets; // SceneLights에서 각 종류가 시작되는 위치, 순서는 위와 같음
    float4 SkyColorIntensity; // rgb: Sky Light 색상, a: 세기 (0이면 비활성)
};

StructuredBuffer<FSceneLightRenderInfo> SceneLights : register(t8);
StructuredBuffer<uint> ClusterCounts : register(t9);
StructuredBuffer<uint> ClusterLightIndices : register(t10);
TextureCube<float4> SkyIrradiance : register(t11);
SamplerState SkySampler : register(s1);

// Compute 패스의 b0와 같은 상수 버퍼를 픽셀 셰이더의 b3에도 바인딩한다.
cbuffer ForwardClusterConstants : register(b3)
{
    matrix ClusterInverseViewProjection;
    float4 ClusterCameraPosition;
    float4 ClusterCameraForward;
    uint3 ClusterDimensions;
    uint bOrthographic;
    uint4 ClusterViewportSize;
    uint4 ClusterLightCounts;
    uint4 ClusterLightOffsets;
    float4 ClusterDepthRange;
    uint4 ClusterOptions; // x: 빌드 성공, y: 타일 크기, z: 클러스터당 최대 라이트 수
};

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // x: 정반사 세기, y: 광택 지수, z: 컬러 텍스처, w: 노멀맵 인코딩
    float4 UVTransform; // xy: 이동량, zw: 배율
};

Texture2D ColorTexture : register(t0);
Texture2D NormalTexture : register(t1);
SamplerState ColorSampler : register(s0);

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD4;
    float4 WorldTangent : TEXCOORD5;
#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : TEXCOORD1;
    float3 VertexSpecular : TEXCOORD2;
#else
    float3 WorldPosition : TEXCOORD1;
#endif
};

struct FForwardOutput
{
    float4 LitColor : SV_Target0;
    float4 NormalRoughness : SV_Target1;
};

struct FLightResult
{
    float3 Diffuse;
    float3 Specular;
};

float3 ResolveWorldNormal(float3 WorldNormal, float4 WorldTangent, float3 MapNormal)
{
    float3 N = normalize(WorldNormal);
    float3 T = WorldTangent.xyz - N * dot(N, WorldTangent.xyz);
    T *= rsqrt(max(dot(T, T), 1.0e-8f));
    float3 B = cross(N, T) * WorldTangent.w;
    return normalize(T * MapNormal.x + B * MapNormal.y + N * MapNormal.z);
}

float3 DecodeMapNormal(float3 Sample, float Encoding)
{
    if (Encoding > 1.5f)
    {
        float2 XY = Encoding > 2.5f ? Sample.xy : Sample.xy * 2.0f - 1.0f;
        return float3(XY, sqrt(saturate(1.0f - dot(XY, XY))));
    }
    return Sample * 2.0f - 1.0f;
}

float3 CalculateAmbientLight(FAmbientLightInfo Info)
{
    return Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f);
}

float3 CalculateSkyLight(float3 N)
{
    if (SkyColorIntensity.a <= 0.0f) return 0.0f;
    return SkyIrradiance.SampleLevel(SkySampler, N, 0).rgb *
        SkyColorIntensity.rgb * SkyColorIntensity.a;
}

float3 CalculateSpecular(float3 N, float3 V, float3 L, float3 Radiance)
{
    float Strength = max(SurfaceParams.x, 0.0f);
    float Shininess = max(SurfaceParams.y, 1.0f);
    float3 HalfVector = L + V;
    HalfVector *= rsqrt(max(dot(HalfVector, HalfVector), 1.0e-8f));
    return Radiance * Strength * pow(saturate(dot(N, HalfVector)), Shininess);
}

void AddLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, bool bIncludeSpecular)
{
    float NoL = saturate(dot(N, L));
    Result.Diffuse += Radiance * NoL;
    if (bIncludeSpecular && NoL > 0.0f)
        Result.Specular += CalculateSpecular(N, V, L, Radiance);
}

void CalculateDirectionalLight(inout FLightResult Result, FSceneLightRenderInfo Info, float3 N, float3 V,
    bool bIncludeSpecular)
{
    if (Info.ColorIntensity.w <= 0.0f)
        return;
    float3 L = normalize(-Info.PositionRadius.xyz);
    float3 Radiance = Info.ColorIntensity.rgb * Info.ColorIntensity.w;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculatePointLight(inout FLightResult Result, FSceneLightRenderInfo Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;
    if (Radius <= 0.0f || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float Fade = pow(saturate(1.0f - Distance / Radius), max(Info.Parameters.x, 0.0f));
    float3 Radiance = Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f) * Fade;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateSpotLight(inout FLightResult Result, FSceneLightRenderInfo Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;

    if (Radius <= 0.0f || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float ConeCos = dot(-L, normalize(Info.DirectionOuterCos.xyz));
    float ConeFade = smoothstep(Info.DirectionOuterCos.w, Info.Parameters.x, ConeCos);
    float RadiusFade = pow(saturate(1.0f - Distance / Radius),
        max(Info.Parameters.y, 0.0f));

    float3 Radiance = Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f)
        * ConeFade * RadiusFade;

    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

FLightResult CalculateBaseLighting(float3 N, float3 V, bool bIncludeSpecular)
{
    FLightResult Result;
    Result.Diffuse = CalculateAmbientLight(Ambient) + CalculateSkyLight(N);
    Result.Specular = 0.0f;

    [loop]
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
        CalculateDirectionalLight(Result, SceneLights[LightOffsets.z + DirectionalIndex], N, V, bIncludeSpecular);
    return Result;
}

void AddAllLocalLights(inout FLightResult Result, float3 P, float3 N,
    float3 V, bool bIncludeSpecular)
{
    [loop]
    for (uint PointIndex = 0; PointIndex < LightCounts.x; ++PointIndex)
        CalculatePointLight(Result, SceneLights[LightOffsets.x + PointIndex], P, N, V, bIncludeSpecular);
    [loop]
    for (uint SpotIndex = 0; SpotIndex < LightCounts.y; ++SpotIndex)
        CalculateSpotLight(Result, SceneLights[LightOffsets.y + SpotIndex], P, N, V, bIncludeSpecular);
}

FLightResult CalculateLighting(float3 P, float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 V = normalize(CameraPosition.xyz - P);
    FLightResult Result = CalculateBaseLighting(N, V, bIncludeSpecular);
    AddAllLocalLights(Result, P, N, V, bIncludeSpecular);
    return Result;
}

// 픽셀 위치와 카메라 전방 깊이를 Compute 패스에서 사용한 클러스터 격자로 변환한다.
uint CalculateForwardClusterID(float2 PixelPosition, float3 WorldPosition)
{
    uint2 Pixel = min((uint2)PixelPosition, ClusterViewportSize.xy - 1u);
    uint2 Tile = min(Pixel / ClusterOptions.y, ClusterDimensions.xy - 1u);

    float NearDepth = ClusterDepthRange.x;
    float FarDepth = ClusterDepthRange.y;
    float ViewDepth = max(dot(WorldPosition - ClusterCameraPosition.xyz,
        ClusterCameraForward.xyz), NearDepth);
    float SliceFraction = bOrthographic != 0
        ? (ViewDepth - NearDepth) / (FarDepth - NearDepth)
        : log2(ViewDepth / NearDepth) / log2(max(FarDepth / NearDepth, 1.0f));
    uint DepthSlice = min((uint)(saturate(SliceFraction) * ClusterDimensions.z),
        ClusterDimensions.z - 1u);

    return (DepthSlice * ClusterDimensions.y + Tile.y) * ClusterDimensions.x + Tile.x;
}

FLightResult CalculateClusteredLighting(float2 PixelPosition, float3 P,
    float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 V = normalize(CameraPosition.xyz - P);
    FLightResult Result = CalculateBaseLighting(N, V, bIncludeSpecular);

    if (ClusterOptions.x == 0)
    {
        AddAllLocalLights(Result, P, N, V, bIncludeSpecular);
    }
    else
    {
        uint ClusterID = CalculateForwardClusterID(PixelPosition, P);
        uint ClusterLightCount = ClusterCounts[ClusterID];
        if (ClusterLightCount > ClusterOptions.z)
        {
            AddAllLocalLights(Result, P, N, V, bIncludeSpecular);
        }
        else
        {
            uint FirstSpotIndex = LightOffsets.y;
            [loop]
            for (uint Index = 0; Index < ClusterLightCount; ++Index)
            {
                uint LightIndex = ClusterLightIndices[ClusterID * ClusterOptions.z + Index];
                FSceneLightRenderInfo Light = SceneLights[LightIndex];
                if (LightIndex < FirstSpotIndex)
                    CalculatePointLight(Result, Light, P, N, V, bIncludeSpecular);
                else
                    CalculateSpotLight(Result, Light, P, N, V, bIncludeSpecular);
            }
        }
    }
    return Result;
}

VS_OUT Uber_VS(VS_IN Input)
{
    VS_OUT Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(mul(WorldPosition, View), Projection);
    Output.Color = Input.Color;
    Output.UV = Input.UV * UVTransform.zw + UVTransform.xy;

    float3 N = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;
    Output.WorldNormal = N;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;
    // CPU가 구한 로컬 공간 부호에 월드 변환의 거울 반전만 반영한다.
    float Handedness = Input.Tangent.w * (determinant((float3x3)World) < 0.0f ? -1.0f : 1.0f);
    Output.WorldTangent = float4(T, Handedness);
#if LIGHTING_MODEL_GOURAUD
    float3 LightingNormal = N;
    if (SurfaceParams.w > 0.5f)
    {
        float3 MapNormal = DecodeMapNormal(NormalTexture.SampleLevel(ColorSampler, Output.UV, 0).xyz, SurfaceParams.w);
        LightingNormal = ResolveWorldNormal(N, Output.WorldTangent, MapNormal);
    }
    FLightResult Lit = CalculateLighting(WorldPosition.xyz, LightingNormal, true);
    Output.VertexDiffuse = Lit.Diffuse;
    Output.VertexSpecular = Lit.Specular;
#else
    Output.WorldPosition = WorldPosition.xyz;
#endif
    return Output;
}

FForwardOutput Uber_PS(VS_OUT Input)
{
    float4 SurfaceColor;
    if (SurfaceParams.z > 0.5f)
    {
        float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV);
        SurfaceColor = float4(pow(max(TextureColor.rgb, 0.0f), 2.2f), TextureColor.a);
    }
    else
    {
        SurfaceColor = Input.Color;
    }
    float3 Albedo = SurfaceColor.rgb * BaseColor.rgb;
    float3 ShadingNormal = normalize(Input.WorldNormal);
    if (SurfaceParams.w > 0.5f)
    {
        float3 MapNormal = DecodeMapNormal(NormalTexture.Sample(ColorSampler, Input.UV).xyz, SurfaceParams.w);
        ShadingNormal = ResolveWorldNormal(Input.WorldNormal, Input.WorldTangent, MapNormal);
    }
    float3 Diffuse;
    float3 Specular = 0.0f;

#if LIGHTING_MODEL_GOURAUD
    Diffuse = Input.VertexDiffuse;
    Specular = Input.VertexSpecular;
#elif LIGHTING_MODEL_LAMBERT
    Diffuse = CalculateClusteredLighting(Input.Position.xy, Input.WorldPosition,
        ShadingNormal, false).Diffuse;
#elif LIGHTING_MODEL_PHONG
    FLightResult Lit = CalculateClusteredLighting(Input.Position.xy, Input.WorldPosition,
        ShadingNormal, true);
    Diffuse = Lit.Diffuse;
    Specular = Lit.Specular;
#endif

    FForwardOutput Output;
    Output.LitColor = float4(Albedo * Diffuse + Specular + Emissive.rgb,
        SurfaceColor.a * BaseColor.a);
    Output.NormalRoughness = float4(ShadingNormal, 0.8f);
    return Output;
}

// 현재 리소스 매니저는 .hlsl 파일을 스캔할 때 mainVS/mainPS 진입점을 찾는다.
VS_OUT mainVS(VS_IN Input) { return Uber_VS(Input); }
FForwardOutput mainPS(VS_OUT Input) { return Uber_PS(Input); }
