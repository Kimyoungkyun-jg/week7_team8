#pragma pack_matrix(row_major)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer PerObject : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float2 TextureFlags; // x: RGB(1), BC5_UNORM(2), BC5_SNORM(3)
};

Texture2D NormalTexture : register(t1);
SamplerState NormalSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD4;
    float4 WorldTangent : TEXCOORD5;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(WorldPosition, VP);

    // inverse-transpose 행렬로 변환해 비균일 스케일에도 대응한다.
    Output.WorldNormal = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;
    float Handedness = Input.Tangent.w * (determinant((float3x3)World) < 0.0f ? -1.0f : 1.0f);
    Output.WorldTangent = float4(T, Handedness);
    Output.UV = Input.UV;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_Target
{
    // 보간 후 정규화. 길이가 0인 노멀도 처리한다.
    float LengthSquared = dot(Input.WorldNormal, Input.WorldNormal);
    float3 N = LengthSquared > 1.0e-8f
        ? Input.WorldNormal * rsqrt(LengthSquared)
        : float3(0.0f, 0.0f, 1.0f);

    if (TextureFlags.x > 0.5f)
    {
        float3 Sample = NormalTexture.Sample(NormalSampler, Input.UV + UVOffset).xyz;
        float3 MapNormal = Sample * 2.0f - 1.0f;
        if (TextureFlags.x > 1.5f)
        {
            float2 XY = TextureFlags.x > 2.5f ? Sample.xy : Sample.xy * 2.0f - 1.0f;
            MapNormal = float3(XY, sqrt(saturate(1.0f - dot(XY, XY))));
        }

        float3 T = Input.WorldTangent.xyz - N * dot(N, Input.WorldTangent.xyz);
        float TangentLengthSquared = dot(T, T);
        if (TangentLengthSquared > 1.0e-8f)
        {
            T *= rsqrt(TangentLengthSquared);
            float3 B = cross(N, T) * Input.WorldTangent.w;
            float3 MappedNormal = T * MapNormal.x + B * MapNormal.y + N * MapNormal.z;
            float MappedLengthSquared = dot(MappedNormal, MappedNormal);
            if (MappedLengthSquared > 1.0e-8f)
                N = MappedNormal * rsqrt(MappedLengthSquared);
        }
    }

    // [-1, 1] → [0, 1]
    return float4(N * 0.5f + 0.5f, 1.0f);
}
