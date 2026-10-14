#pragma pack_matrix(row_major)
#include "LightEvaluation.hlsli"

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
StructuredBuffer<uint4> TileLightRanges : register(t9);
StructuredBuffer<uint> TilePointIndices : register(t10);
StructuredBuffer<uint> TileSpotIndices : register(t12);
TextureCube<float4> SkyIrradiance : register(t11);
SamplerState SkySampler : register(s1);

// Compute가 생성한 타일 목록의 주소 계산과 사용 여부만 전달한다.
cbuffer ForwardTileLightingInfo : register(b3)
{
    uint UseTiledLights;
    uint TileCountX;
    uint LightTileSize;
    uint TilePadding;
};

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // xy: 예약, z: 컬러 텍스처 사용, w: 노멀맵 인코딩
    float4 UVTransform; // xy: 이동량, zw: 배율
    float4 SpecularColorShininess; // rgb: 정반사 색상, w: 광택 지수
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

void AddLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, bool bIncludeSpecular)
{
    AccumulateLight(Result, N, V, L, Radiance, SpecularColorShininess.rgb,
        SpecularColorShininess.w, bIncludeSpecular);
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
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Info.PositionRadius.w * Info.PositionRadius.w;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
        return;
    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    // Deferred와 동일하게 Point Light의 RadiusFallOff를 반경 감쇠 지수로 사용한다.
    float Fade = pow(saturate(1.0f - Distance / Info.PositionRadius.w), max(Info.Parameters.x, 0.0f));
    if (Fade <= 0.0f) return;
    float3 L = ToLight / Distance;
    float3 Radiance = Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f) * Fade;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateSpotLight(inout FLightResult Result, FSceneLightRenderInfo Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusFade = CalculateDistanceAttenuation(DistanceSquared, Info.PositionRadius.w * Info.PositionRadius.w);
    if (RadiusFade <= 0.0f) return;
    float3 L = ToLight / sqrt(max(DistanceSquared, 1.0e-6f));
	float ConeCos = dot(-L, normalize(Info.DirectionOuterCos.xyz));
	float ConeFade = CalculateConeAttenuation(ConeCos, Info.Parameters.x, Info.DirectionOuterCos.w);
	ConeFade = pow(ConeFade, max(Info.Parameters.y, 0.0001f));

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
    float3 ToCamera = CameraPosition.xyz - P;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    FLightResult Result = CalculateBaseLighting(N, V, bIncludeSpecular);
    AddAllLocalLights(Result, P, N, V, bIncludeSpecular);
    return Result;
}

// 타일 목록의 인덱스는 SceneLights 안에서 종류별 시작 위치에 더한다.
FLightResult CalculateTiledLighting(float2 PixelPosition, float3 P,
    float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 ToCamera = CameraPosition.xyz - P;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    FLightResult Result = CalculateBaseLighting(N, V, bIncludeSpecular);
    if (UseTiledLights == 0)
    {
        AddAllLocalLights(Result, P, N, V, bIncludeSpecular);
        return Result;
    }

    uint2 Tile = uint2(PixelPosition) / LightTileSize;
    uint4 Range = TileLightRanges[Tile.y * TileCountX + Tile.x];
    bool UsePointList = Range.x != 0xffffffffu;
    bool UseSpotList = Range.z != 0xffffffffu;
    uint PointCount = UsePointList ? Range.y : LightCounts.x;
    uint SpotCount = UseSpotList ? Range.w : LightCounts.y;
    [loop]
    for (uint Index = 0; Index < PointCount; ++Index)
    {
        uint LightIndex = UsePointList ? TilePointIndices[Range.x + Index] : Index;
        CalculatePointLight(Result, SceneLights[LightOffsets.x + LightIndex], P, N, V, bIncludeSpecular);
    }
    [loop]
    for (uint Index = 0; Index < SpotCount; ++Index)
    {
        uint LightIndex = UseSpotList ? TileSpotIndices[Range.z + Index] : Index;
        CalculateSpotLight(Result, SceneLights[LightOffsets.y + LightIndex], P, N, V, bIncludeSpecular);
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
    FLightResult Lit = CalculateLighting(WorldPosition.xyz, N, true);
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
    Diffuse = CalculateTiledLighting(Input.Position.xy, Input.WorldPosition,
        ShadingNormal, false).Diffuse;
#elif LIGHTING_MODEL_PHONG
    FLightResult Lit = CalculateTiledLighting(Input.Position.xy, Input.WorldPosition,
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
