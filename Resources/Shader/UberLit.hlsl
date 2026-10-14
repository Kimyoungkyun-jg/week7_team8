#pragma pack_matrix(row_major)

#define NUM_POINT_LIGHT 4
#define NUM_SPOT_LIGHT 4

struct FAmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct FDirectionLightInfo
{
    float3 Direction;
    float Intensity;
    float3 Color;
    float Pad;
};

struct FPointLightInfo
{
    float3 Position;
    float Intensity;
    float3 Color;
    float Radius;
};

struct FSpotLightInfo
{
    float3 Position;    
    float Range;    
    float3 Color;
    float Intensity;
    float3 Direction;
    float Pad1;
    float2 Angle; // Inner, Outer
    float2 Pad2;
};

cbuffer ViewConstants : register(b0)
{
    matrix VP;
    float3 CamPos;
    float Pad;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float3 SpecularColor;
    float Shininess;
    float2 UVOffset;
    float2 Pad1;
};

cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

cbuffer Lighting : register(b3)
{
    FAmbientLightInfo Ambient;
    FDirectionLightInfo Directional;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLight[NUM_SPOT_LIGHT];
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
};

Texture2D g_txColor : register(t0);
Texture2D g_txNormal : register(t1);
SamplerState g_Sample : register(s0);

float3 BRDF(float3 N, float3 L, float3 V, float3 SurfaceColor)
{
    float3 Result;
    
    float NdotL = saturate(dot(N, L));
    float3 Diffuse = SurfaceColor * NdotL;
    Result = Diffuse;
    
    #if LIGHTING_MODEL_PHONG
    
    if(NdotL > 0)
    {
       float3 H = normalize(L + V);
       float NdotH = saturate(dot(N, H));
       float3 Specular = SpecularColor * pow(NdotH, Shininess);
       Result += Specular;
    }
    
    #endif
    
    return Result;
}

float3 CalculateAmbientLight(FAmbientLightInfo Info, float3 SurfaceColor)
{
    float3 Result = Info.Color * Info.Intensity * SurfaceColor;
    
    return Result;
}

float3 CalculateDirectionalLight(FDirectionLightInfo Info, float3 Normal, float3 ViewDir, float3 SurfaceColor)
{
    float3 L = normalize(-Info.Direction);
    
    float3 Result = BRDF(Normal, L, ViewDir, SurfaceColor) * Info.Color * Info.Intensity;
    
    return Result;
}

float3 CalculatePointLight(FPointLightInfo Info, float3 WorldPos, float3 Normal, float3 ViewDir, float3 SurfaceColor)
{        
    float3 ToLight = Info.Position - WorldPos;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Info.Radius * Info.Radius;
    if(RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }    
    
    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    float3 L = ToLight / Distance;    
    float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
    float Attenuation = RadiusFade * RadiusFade / (1.0f + DistanceSquared);
    
    float3 Result = BRDF(Normal, L, ViewDir, SurfaceColor) * Info.Color * Info.Intensity * Attenuation;
    
    return Result;
}

float3 CalculateSpotLight(FSpotLightInfo Info, float3 WorldPos, float3 Normal, float3 ViewDir, float3 SurfaceColor)
{
    float3 ToLight = Info.Position - WorldPos;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Info.Range * Info.Range;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    
    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    float3 L = ToLight / Distance;
    float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
    float DistanceAttenuation = RadiusFade * RadiusFade / (1.0f + DistanceSquared);
    
    float CosAngle = dot(-L, normalize(Info.Direction));
    float CosInner = cos(radians(Info.Angle.x));
    float CosOuter = cos(radians(Info.Angle.y));
    if(CosAngle <= CosOuter)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    float AngleAttenuation = saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 1.0e-8f));
    
    float3 Result = BRDF(Normal, L, ViewDir, SurfaceColor) * Info.Color * Info.Intensity * DistanceAttenuation * AngleAttenuation;
    
    return Result;
}

float3 CalculateTBN(float3 WorldPos, float2 UV, float3 GeometryNormal)
{
    float3 dPos1 = ddx(WorldPos);
    float3 dPos2 = ddy(WorldPos);
    float2 duv1 = ddx(UV);
    float2 duv2 = ddy(UV);
    
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    
    float invDet = (abs(det) < 1e-6f) ? 0.0f : 1.0f / det;
    if(invDet == 0.0f)
    {
        return GeometryNormal;
    }
        
    float3 Tangent = normalize((dPos1 * duv2.y - dPos2 * duv1.y) * invDet);
    float3 Bitangent = normalize((dPos2 * duv1.x - dPos1 * duv2.x) * invDet);
    
    // Gram-Schmidt orthogonalization to ensure Tangent is orthogonal to GeometryNormal
    Tangent = normalize(Tangent - dot(Tangent, GeometryNormal) * GeometryNormal);
    float3 B_cross = cross(GeometryNormal, Tangent);
    float handedness = (dot(B_cross, Bitangent) < 0.0f) ? -1.0f : 1.0f;
    Bitangent = B_cross * handedness;
    
    float4 NormalMap = g_txNormal.Sample(g_Sample, UV);
    if (dot(NormalMap.rgb, NormalMap.rgb) < 0.5f)
    {
        return GeometryNormal;
    }
    
    float3 NormalTanget = NormalMap.xyz * 2.0f - 1.0f;
    NormalTanget.y = -NormalTanget.y; // Flip Y for DirectX
    
    float3 Normal = normalize(Tangent * NormalTanget.x + Bitangent * NormalTanget.y + GeometryNormal * NormalTanget.z);    
    
    return Normal;
}

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT output;
    
    float4 WorldPos = mul(float4(Input.position, 1.0f), World);
    
    output.WorldPosition = WorldPos.xyz;
    output.position = mul(WorldPos, VP);
    output.normal = normalize(mul(float4(Input.normal, 0.0f), NormalMatrix)).xyz;
    output.color = Input.color;
    
#if LIGHTING_MODEL_GOURAUD
    float3 TotalLight = float3(0.0f, 0.0f, 0.0f);
    
    float3 N = normalize(output.normal);
    
    float3 ToCamera = CamPos - output.WorldPosition;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    
    TotalLight += CalculateAmbientLight(Ambient, float3(1.0f, 1.0f, 1.0f));
    TotalLight += CalculateDirectionalLight(Directional, N, V, float3(1.0f, 1.0f, 1.0f));
    
    [loop]
    for (uint Index1 = 0; Index1 < (uint) NUM_POINT_LIGHT; ++Index1)
    {
        TotalLight += CalculatePointLight(PointLights[Index1], output.WorldPosition, N, V, float3(1.0f, 1.0f, 1.0f));

    }
    [loop]
    for (uint Index2 = 0; Index2 < (uint) NUM_SPOT_LIGHT; ++Index2)
    {
        TotalLight += CalculateSpotLight(SpotLight[Index2], output.WorldPosition, N, V, float3(1.0f, 1.0f, 1.0f));

    }

    output.color = float4(TotalLight, 1.0f);
      
#endif
    
    output.uv = Input.uv + UVOffset;
    
    return output;
}

float4 mainPS(PS_INPUT input) :SV_TARGET
{
    float4 result = input.color;
    
    float4 TexColor = g_txColor.Sample(g_Sample, input.uv);
    float3 Albedo = TexColor.rgb * BaseColor.rgb;
    float Alpha = TexColor.a * BaseColor.a;
    
#if LIGHTING_MODEL_GOURAUD
    return float4(input.color.rgb * Albedo, Alpha);
#else
    float3 TotalLight = float3(0.0f, 0.0f, 0.0f);
    
    float3 N = CalculateTBN(input.WorldPosition, input.uv, normalize(input.normal));    
    
    float3 ToCamera = CamPos - input.WorldPosition;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    
    TotalLight += CalculateAmbientLight(Ambient, Albedo);
    
    TotalLight += CalculateDirectionalLight(Directional, N, V, Albedo);
    
    [loop]
    for (uint Index1 = 0; Index1 < (uint) NUM_POINT_LIGHT; ++Index1)
    {
        TotalLight += CalculatePointLight(PointLights[Index1], input.WorldPosition, N, V, Albedo);
    }
    
    [loop]
    for (uint Index2 = 0; Index2 < (uint) NUM_SPOT_LIGHT; ++Index2)
    {
        TotalLight += CalculateSpotLight(SpotLight[Index2], input.WorldPosition, N, V, Albedo);
    }
    
    return float4(TotalLight, Alpha);
#endif
}