#pragma pack_matrix(row_major)
// Todo: subuv
cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

cbuffer subuv : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
    float FogAdditive;
    float3 SubUVPadding;
};

// 화면 안개 패스와 같은 상수 버퍼를 픽셀별 반투명 안개에 재사용한다.
cbuffer TranslucentFogConstants : register(b3)
{
    float4x4 FogInverseViewProjection;
    float4 FogCameraPosition;
    float4 FogColor;
    float4 FogDensityHeight;
    float4 FogDistanceViewport;
};

Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
};

float FogTransmittance(float3 Position)
{
    if (FogDensityHeight.x <= 0.0f || FogDistanceViewport.x <= 0.0f)
        return 1.0f;

    float3 Ray = Position - FogCameraPosition.xyz;
    float Distance = length(Ray);
    float Start = FogDensityHeight.w;
    if (Distance <= Start ||
        (FogDistanceViewport.y > 0.0f && Distance > FogDistanceViewport.y))
        return 1.0f;

    float Length = Distance - Start;
    float StartHeight = FogCameraPosition.z + Ray.z * (Start / Distance);
    float Falloff = FogDensityHeight.y;
    float A = abs(Falloff * (Position.z - StartHeight));
    float HeightExponent = -Falloff *
        (min(StartHeight, Position.z) - FogDensityHeight.z);
    float G;
    if (A < 0.01f)
        G = 1.0f - A * 0.5f + A * A / 6.0f;
    else
        G = (1.0f - exp(-A)) / A;
    float LogTau = log(FogDensityHeight.x) + log(Length) +
        HeightExponent + log(G);
    if (!isfinite(LogTau))
        return 1.0f;

    float T = LogTau >= 4.382026635f ? 0.0f
            : LogTau <= -80.0f ? 1.0f
            : exp(-exp(LogTau));
    return max(saturate(T), 1.0f - FogDistanceViewport.x);
}

// Todo: VS shader code duplicated
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float4 WorldPosition = mul(float4(input.position, 1.0f), World);
    output.position = mul(WorldPosition, VP);
    output.uv = input.uv;
    output.worldPosition = WorldPosition.xyz;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float atlasCol = fmod(CurrentFrame, AtlasColSize);
    float atlasRow = floor(CurrentFrame / AtlasColSize);

    float2 cellSize = float2(1.0f / AtlasColSize, 1.0f / AtlasRowSize);
    float2 cellUV = input.uv * cellSize;
    
    float2 cellStartOffsetUV = (float2(atlasCol, atlasRow) * cellSize);
    cellUV += cellStartOffsetUV;

    float4 color = AtlasTexture.Sample(AtlasSampler, cellUV);
    color.a *= ParticleAlpha;

    float T = FogTransmittance(input.worldPosition);
    if (FogAdditive > 0.5f)
        color.rgb *= T;
    else
        color.a *= T;
    
    return color;
}
