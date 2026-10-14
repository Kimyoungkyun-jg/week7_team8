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

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldNormal : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(WorldPosition, VP);

    // inverse-transpose 행렬로 변환해 비균일 스케일에도 대응한다.
    Output.WorldNormal = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_Target
{
    // 보간 후 정규화. 길이가 0인 노멀도 처리한다.
    float LengthSquared = dot(Input.WorldNormal, Input.WorldNormal);
    float3 N = LengthSquared > 1.0e-8f
        ? Input.WorldNormal * rsqrt(LengthSquared)
        : float3(0.0f, 0.0f, 1.0f);

    // [-1, 1] → [0, 1]
    return float4(N * 0.5f + 0.5f, 1.0f);
}