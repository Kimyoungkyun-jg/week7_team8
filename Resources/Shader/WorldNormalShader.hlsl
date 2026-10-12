Texture2D<float4> WorldNormalTexture : register(t0);
Texture2D<float> SceneDepthTexture : register(t1);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    if (SceneDepthTexture.Load(int3(Pixel, 0)) >= 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float3 WorldNormal = WorldNormalTexture.Load(int3(Pixel, 0)).xyz;
    float LengthSquared = dot(WorldNormal, WorldNormal);
    if (LengthSquared < 1.0e-8f)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float3 N = WorldNormal * rsqrt(LengthSquared);
    float3 EncodedNormal = saturate(N * 0.5f + 0.5f);
    return float4(EncodedNormal, 1.0f);
}
