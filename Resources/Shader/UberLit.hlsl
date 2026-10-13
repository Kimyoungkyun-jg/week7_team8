#pragma pack_matrix(row_major)

#define NUM_POINT_LIGHT 32
#define NUM_SPOT_LIGHT 32

// 네 모델 중 하나만 1로 지정. 정의가 없으면 Lambert.
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && !defined(LIGHTING_MODEL_UNLIT)
#define LIGHTING_MODEL_LAMBERT 1
#endif

#if (LIGHTING_MODEL_GOURAUD + LIGHTING_MODEL_LAMBERT + LIGHTING_MODEL_PHONG + LIGHTING_MODEL_UNLIT) != 1
#error Exactly one lighting model must be enabled.
#endif

struct FDirectionalLightInfo
{
    float4 Direction;
    float4 ColorIntensity;
};

struct FPointLightInfo
{
    float4 PositionRadius;
    float4 ColorIntensity;
    float4 FalloffPadding;
};

struct FSpotLightInfo
{
    float4 PositionRadius;
    float4 ColorIntensity;
    float4 DirectionInnerOuterCos; // xyz: 진행 방향, w: cosOuter
    float4 InnerCosFalloff; // x: cosInner, y: Falloff
};

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float SpecularIntensity;
    float Shineness;
};

cbuffer PerObject : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

cbuffer Lighting : register(b4)
{
    float4 CameraPosition; // w: Perspective 여부
    float4 CameraForward;
    float4 AmbientLightColor;
    float4 ViewportLightCount; // x: Point 개수, y: Spot 개수

    FDirectionalLightInfo DirectionalLight;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

float3 SafeNormalize(float3 X)
{
    return X * rsqrt(max(dot(X, X), 1.0e-8f));
}

float3 CalculateAmbientLight()
{
    return AmbientLightColor.rgb; // CPU에서 색 × 강도 합산.
}

float3 CalculateDiffuseLight(float3 N, float3 L, float3 Radiance)
{
    return Radiance * saturate(dot(N, L));
}

#if LIGHTING_MODEL_GOURAUD || LIGHTING_MODEL_PHONG
float3 CalculateSpecularLight(float3 N, float3 V, float3 L, float3 Radiance)
{
    if (dot(N, L) <= 0.0f || dot(N, V) <= 0.0f)
        return 0.0f;

    float3 H = SafeNormalize(L + V);
    return Radiance * max(SpecularIntensity, 0.0f)
        * pow(saturate(dot(N, H)), max(Shineness, 1.0f));
}
#endif

void AddLight(inout float3 Diffuse, inout float3 Specular, float3 N, float3 V, float3 L, float3 Radiance)
{
    Diffuse += CalculateDiffuseLight(N, L, Radiance);

#if LIGHTING_MODEL_GOURAUD || LIGHTING_MODEL_PHONG
    Specular += CalculateSpecularLight(N, V, L, Radiance);
#endif
}

float CalculateAttenuation(float Distance, float Radius, float Falloff)
{
    if (Radius <= 0.0f || Distance >= Radius)
        return 0.0f;

    return Falloff <= 0.0f ? 1.0f
        : pow(saturate(1.0f - Distance / Radius), Falloff);
}

void CalculateDirectionalLight(FDirectionalLightInfo Info, float3 N, float3 V,
    inout float3 Diffuse, inout float3 Specular)
{
    if (Info.ColorIntensity.w <= 0.0f)
        return;

    float3 L = SafeNormalize(-Info.Direction.xyz);
    float3 Radiance = Info.ColorIntensity.rgb * Info.ColorIntensity.w;
    AddLight(Diffuse, Specular, N, V, L, Radiance);
}

void CalculatePointLight(FPointLightInfo Info, float3 P, float3 N, float3 V,
    inout float3 Diffuse, inout float3 Specular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;

    if (Info.ColorIntensity.w <= 0.0f || Radius <= 0.0f
        || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float Fade = CalculateAttenuation(Distance, Radius, Info.FalloffPadding.x);
    float3 Radiance = Info.ColorIntensity.rgb * Info.ColorIntensity.w * Fade;

    AddLight(Diffuse, Specular, N, V, L, Radiance);
}

void CalculateSpotLight(FSpotLightInfo Info, float3 P, float3 N, float3 V,  inout float3 Diffuse, inout float3 Specular)
{
    float3 ToLight = Info.PositionRadius.xyz - P;
    float Distance = length(ToLight);
    float Radius = Info.PositionRadius.w;

    if (Info.ColorIntensity.w <= 0.0f || Radius <= 0.0f
        || Distance >= Radius || Distance < 1.0e-5f)
        return;

    float3 L = ToLight / Distance;
    float3 Direction = Info.DirectionInnerOuterCos.xyz;

    if (dot(Direction, Direction) < 1.0e-8f)
        return;

    float ConeCos = dot(-L, SafeNormalize(Direction));
    float OuterCos = Info.DirectionInnerOuterCos.w;
    float InnerCos = Info.InnerCosFalloff.x;

    float ConeFade = InnerCos - OuterCos > 1.0e-6f
        ? smoothstep(OuterCos, InnerCos, ConeCos)
        : step(OuterCos, ConeCos);

    float RadiusFade = CalculateAttenuation(
        Distance, Radius, Info.InnerCosFalloff.y);

    float3 Radiance = Info.ColorIntensity.rgb * Info.ColorIntensity.w
        * ConeFade * RadiusFade;

    AddLight(Diffuse, Specular, N, V, L, Radiance);
}

// N은 호출하는 VS/PS에서 정규화하여 전달.
void CalculateLighting(float3 P, float3 N,
    out float3 Diffuse, out float3 Specular)
{
    float3 V = 0.0f;

#if LIGHTING_MODEL_GOURAUD || LIGHTING_MODEL_PHONG
    V = CameraPosition.w > 0.5f
        ? SafeNormalize(CameraPosition.xyz - P)
        : SafeNormalize(-CameraForward.xyz);
#endif

    Diffuse = CalculateAmbientLight();
    Specular = 0.0f;

    CalculateDirectionalLight(DirectionalLight, N, V, Diffuse, Specular);

    uint PointCount = (uint) clamp(
        ViewportLightCount.x, 0.0f, (float) NUM_POINT_LIGHT);

    [loop]
    for (uint I = 0; I < PointCount; ++I)
        CalculatePointLight(PointLights[I], P, N, V, Diffuse, Specular);

    uint SpotCount = (uint) clamp(
        ViewportLightCount.y, 0.0f, (float) NUM_SPOT_LIGHT);

    [loop]
    for (uint J = 0; J < SpotCount; ++J)
        CalculateSpotLight(SpotLights[J], P, N, V, Diffuse, Specular);
}

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;

#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : TEXCOORD1;
    float3 VertexSpecular : TEXCOORD2;
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    float3 WorldPosition : TEXCOORD1;
    float3 WorldNormal : TEXCOORD2;
#endif
};

VS_OUT Uber_VS(VS_IN Input)
{
    VS_OUT Output;
    float4 P = mul(float4(Input.Position, 1.0f), World);

    Output.Position = mul(P, VP);
    Output.Color = Input.Color;
    Output.UV = Input.UV + UVOffset;

#if LIGHTING_MODEL_GOURAUD || LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    float3 N = SafeNormalize( mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz);
#endif

#if LIGHTING_MODEL_GOURAUD
    CalculateLighting(P.xyz, N, Output.VertexDiffuse, Output.VertexSpecular);
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    Output.WorldPosition = P.xyz;
    Output.WorldNormal = N;
#endif

    return Output;
}

float4 Uber_PS(VS_OUT Input) : SV_TARGET
{
    float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV);

    // 일반 UNORM 컬러 텍스처를 linear로 변환.
    // 이후 ToneMap에서 화면 감마 적용.
    float3 Albedo = pow(max(TextureColor.rgb, 0.0f), 2.2f) * BaseColor.rgb ;

#if LIGHTING_MODEL_UNLIT
    float3 Color = Albedo;
#else
    float3 Diffuse, Specular;

#if LIGHTING_MODEL_GOURAUD
    Diffuse = Input.VertexDiffuse;
    Specular = Input.VertexSpecular;
#else
    CalculateLighting(Input.WorldPosition,SafeNormalize(Input.WorldNormal), Diffuse, Specular);
#endif
    float3 Color = Albedo * Diffuse + Specular;
#endif

    return float4(Color, TextureColor.a * BaseColor.a);
}

VS_OUT mainVS(VS_IN Input)
{
    return Uber_VS(Input);
}
float4 mainPS(VS_OUT Input) : SV_TARGET
{
    return Uber_PS(Input);
}