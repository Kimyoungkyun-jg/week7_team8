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
};

StructuredBuffer<FSceneLightRenderInfo> SceneLights : register(t8);

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // x: 정반사 세기, y: 광택 지수
    float4 UVTransform; // xy: 이동량, zw: 배율
};

Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : TEXCOORD1;
    float3 VertexSpecular : TEXCOORD2;
#else
    float3 WorldPosition : TEXCOORD1;
    float3 WorldNormal : TEXCOORD2;
#endif
};

struct FLightResult
{
    float3 Diffuse;
    float3 Specular;
};

float3 CalculateAmbientLight(FAmbientLightInfo Info)
{
    return Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f);
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

FLightResult CalculateLighting(float3 P, float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 V = normalize(CameraPosition.xyz - P);
    FLightResult Result;
    Result.Diffuse = CalculateAmbientLight(Ambient);
    Result.Specular = 0.0f;

    [loop]
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
        CalculateDirectionalLight(Result, SceneLights[LightOffsets.z + DirectionalIndex], N, V, bIncludeSpecular);
    [loop]
    for (uint PointIndex = 0; PointIndex < LightCounts.x; ++PointIndex)
        CalculatePointLight(Result, SceneLights[LightOffsets.x + PointIndex], P, N, V, bIncludeSpecular);
    [loop]
    for (uint SpotIndex = 0; SpotIndex < LightCounts.y; ++SpotIndex)
        CalculateSpotLight(Result, SceneLights[LightOffsets.y + SpotIndex], P, N, V, bIncludeSpecular);
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
#if LIGHTING_MODEL_GOURAUD
    FLightResult Lit = CalculateLighting(WorldPosition.xyz, N, true);
    Output.VertexDiffuse = Lit.Diffuse;
    Output.VertexSpecular = Lit.Specular;
#else
    Output.WorldPosition = WorldPosition.xyz;
    Output.WorldNormal = N;
#endif
    return Output;
}

float4 Uber_PS(VS_OUT Input) : SV_TARGET
{
    float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV);
    float3 Albedo = pow(max(TextureColor.rgb, 0.0f), 2.2f)
        * BaseColor.rgb * Input.Color.rgb;
    float3 Diffuse;
    float3 Specular = 0.0f;

#if LIGHTING_MODEL_GOURAUD
    Diffuse = Input.VertexDiffuse;
    Specular = Input.VertexSpecular;
#elif LIGHTING_MODEL_LAMBERT
    Diffuse = CalculateLighting(Input.WorldPosition, Input.WorldNormal, false).Diffuse;
#elif LIGHTING_MODEL_PHONG
    FLightResult Lit = CalculateLighting(Input.WorldPosition, Input.WorldNormal, true);
    Diffuse = Lit.Diffuse;
    Specular = Lit.Specular;
#endif

    return float4(Albedo * Diffuse + Specular + Emissive.rgb,
        TextureColor.a * BaseColor.a * Input.Color.a);
}

// 현재 리소스 매니저는 .hlsl 파일을 스캔할 때 mainVS/mainPS 진입점을 찾는다.
VS_OUT mainVS(VS_IN Input) { return Uber_VS(Input); }
float4 mainPS(VS_OUT Input) : SV_TARGET { return Uber_PS(Input); }
