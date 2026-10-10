#pragma pack_matrix(row_major)

#define NUM_DIRECTIONAL_LIGHT 32
#define NUM_POINT_LIGHT 4
#define NUM_SPOT_LIGHT 4

// The shader scanner compiles this file without defines. Use Lambert for that
// default variant; explicit variants set exactly one model to 1 at compile time.
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG)
    #define LIGHTING_MODEL_LAMBERT 1
#endif

#if (LIGHTING_MODEL_GOURAUD + LIGHTING_MODEL_LAMBERT + LIGHTING_MODEL_PHONG) != 1
    #error Exactly one lighting model must be enabled.
#endif

struct FAmbientLightInfo
{
    float4 ColorIntensity; // rgb: linear color, a: intensity
};

struct FDirectionalLightInfo
{
    float4 DirectionIntensity; // xyz: direction the light travels, w: intensity
    float4 Color;
};

struct FPointLightInfo
{
    float4 PositionRadius; // xyz: world position, w: radius
    float4 ColorIntensity;
    float4 Falloff; // x: radius falloff exponent
};

struct FSpotLightInfo
{
    float4 PositionRadius;
    float4 DirectionOuterCos; // xyz: direction the light travels, w: cos(outer angle)
    float4 ColorIntensity;
    float4 InnerCosFalloff; // x: cos(inner angle), y: radius falloff exponent
};

cbuffer PerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix NormalMatrix; // inverse transpose of World
};

cbuffer Lighting : register(b1)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo DirectionalLights[NUM_DIRECTIONAL_LIGHT];
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
    float4 CameraPosition;
    uint4 LightCounts; // x: point count, y: spot count, z: directional count
};

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // x: specular strength, y: shininess
    float4 UVTransform; // xy: offset, zw: scale
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
    float3 Reflected = reflect(-L, N);
    return Radiance * Strength * pow(saturate(dot(Reflected, V)), Shininess);
}

void AddLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, bool bIncludeSpecular)
{
    float NoL = saturate(dot(N, L));
    Result.Diffuse += Radiance * NoL;
    if (bIncludeSpecular && NoL > 0.0f)
        Result.Specular += CalculateSpecular(N, V, L, Radiance);
}

void CalculateDirectionalLight(inout FLightResult Result, FDirectionalLightInfo Info, float3 N, float3 V,
    bool bIncludeSpecular)
{
    if (Info.DirectionIntensity.w <= 0.0f)
        return;
    float3 L = normalize(-Info.DirectionIntensity.xyz);
    float3 Radiance = Info.Color.rgb * Info.DirectionIntensity.w;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculatePointLight(inout FLightResult Result, FPointLightInfo Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;
    if (Radius <= 0.0f || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float Fade = pow(saturate(1.0f - Distance / Radius), max(Info.Falloff.x, 0.0f));
    float3 Radiance = Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f) * Fade;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateSpotLight(inout FLightResult Result, FSpotLightInfo Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;
    if (Radius <= 0.0f || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float ConeCos = dot(-L, normalize(Info.DirectionOuterCos.xyz));
    float ConeFade = smoothstep(Info.DirectionOuterCos.w, Info.InnerCosFalloff.x, ConeCos);
    float RadiusFade = pow(saturate(1.0f - Distance / Radius),
        max(Info.InnerCosFalloff.y, 0.0f));
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
    for (uint DirectionalIndex = 0; DirectionalIndex < min(LightCounts.z, NUM_DIRECTIONAL_LIGHT); ++DirectionalIndex)
        CalculateDirectionalLight(Result, DirectionalLights[DirectionalIndex], N, V, bIncludeSpecular);
    [loop]
    for (uint PointIndex = 0; PointIndex < min(LightCounts.x, NUM_POINT_LIGHT); ++PointIndex)
        CalculatePointLight(Result, PointLights[PointIndex], P, N, V, bIncludeSpecular);
    [loop]
    for (uint SpotIndex = 0; SpotIndex < min(LightCounts.y, NUM_SPOT_LIGHT); ++SpotIndex)
        CalculateSpotLight(Result, SpotLights[SpotIndex], P, N, V, bIncludeSpecular);
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

// The current resource manager looks for mainVS/mainPS when it scans .hlsl files.
VS_OUT mainVS(VS_IN Input) { return Uber_VS(Input); }
float4 mainPS(VS_OUT Input) : SV_TARGET { return Uber_PS(Input); }
