#pragma pack_matrix(row_major)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer WorldConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float2 Padding;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldNormal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct GBUFFER_OUTPUT
{
    float4 BaseColorMetallic : SV_Target0;
    float4 NormalRoughness : SV_Target1;
};

Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);

    Output.Position = mul(WorldPosition, VP);
    Output.WorldNormal = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;
    Output.UV = Input.UV;
    return Output;
}

GBUFFER_OUTPUT mainPS(PS_INPUT Input)
{
    GBUFFER_OUTPUT Output;

    float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV + UVOffset);
    // 색 텍스처는 UNORM으로 읽히므로 sRGB 값을 선형 조명 공간으로 변환한다.
    float3 LinearAlbedo = pow(max(TextureColor.rgb, 0.0f), 2.2f) * BaseColor.rgb;
    float3 Normal = normalize(Input.WorldNormal);

    // 현재 UMaterial에는 Metallic/Roughness 값이 없어 임시 상수를 쓴다.
    Output.BaseColorMetallic = float4(LinearAlbedo, 0.0f);
    Output.NormalRoughness = float4(Normal, 0.8f);
    return Output;
}
