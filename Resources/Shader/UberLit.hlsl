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
    float FallOff;
    float3 Pad;
};

struct FSpotLightInfo
{
    float3 Position;    
    float Range;    
    float3 Color;
    float Intensity;
    float3 Direction;
    float FallOff;
    float2 Angle; // Inner, Outer
    float2 Pad;
};

cbuffer PerObjectConstants : register(b2)
{
    matrix World;    
};

cbuffer ViewConstants : register(b0)
{
    matrix View;
    matrix Projection;
    float3 CamPos;
    float Pad;
};

cbuffer Lighting : register(b3)
{
    FAmbientLightInfo Ambient;
    FDirectionLightInfo Directional;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLight[NUM_SPOT_LIGHT];
};

float4 CalculateAbientLight(FAmbientLightInfo info)
{
    float4 result;
    
    return result;
}

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
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT output;
    
#if LIGHTING_MODEL_GOURAUD
    
#elif LIGHTING_MODEL_LAMBERT

#elif LIGHTING_MODEL_PHONG

#endif
  
    
    return output;
}

float4 mainPS(PS_INPUT input) :SV_TARGET
{
    float4 result = input.color;
    
    
#if LIGHTING_MODEL_GOURAUD
    
#elif LIGHTING_MODEL_LAMBERT

#elif LIGHTING_MODEL_PHONG

#endif
    
    return result;
}