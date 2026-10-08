#pragma pack_matrix(row_major)

Texture2D<float4> BaseColorMetallic : register(t0);
Texture2D<float4> NormalRoughness : register(t1);
Texture2D<float> SceneDepth : register(t2);

struct FPointLightData
{
    float4 PositionRadius;
    float4 ColorIntensity;
};

cbuffer DeferredLightConstants : register(b0)
{
    matrix InverseViewProjection;
    float4 CameraPosition; // W: perspective projection
    float4 CameraForward;
    float4 ViewportLightCount;
    FPointLightData PointLights[32];
};

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

// GGX microfacet specular + energy-conserving Lambert diffuse.
float3 EvaluateSurfaceBRDF(float3 Albedo, float Metallic, float Roughness,
    float3 N, float3 V, float3 L)
{
    const float PI = 3.14159265359f;
    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    if (NoL <= 0.0f || NoV <= 0.0f) return 0.0f;

    float3 HalfVector = V + L;
    float HalfLengthSquared = dot(HalfVector, HalfVector);
    if (HalfLengthSquared < 1.0e-8f) return 0.0f;
    float3 H = HalfVector * rsqrt(HalfLengthSquared);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));
    Metallic = saturate(Metallic);
    Roughness = clamp(Roughness, 0.045f, 1.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, Metallic);
    float3 F = F0 + (1.0f - F0) * pow(1.0f - VoH, 5.0f);

    float Alpha = Roughness * Roughness;
    float AlphaSquared = Alpha * Alpha;
    float Denominator = NoH * NoH * (AlphaSquared - 1.0f) + 1.0f;
    float D = AlphaSquared / max(PI * Denominator * Denominator, 1.0e-12f);
    float GL = 2.0f * NoL /
        (NoL + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoL * NoL));
    float GV = 2.0f * NoV /
        (NoV + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoV * NoV));
    float3 Specular = D * GL * GV / max(4.0f * NoL * NoV, 1.0e-6f) * F;
    float3 Diffuse = (1.0f - F) * (1.0f - Metallic) * Albedo / PI;
    return (Diffuse + Specular) * NoL;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float Depth = SceneDepth.Load(int3(Pixel, 0));
    if (Depth >= 1.0f)
        discard; // 배경은 먼저 그린 스카이박스를 유지한다.

    float4 MaterialData = BaseColorMetallic.Load(int3(Pixel, 0));
    float4 NormalData = NormalRoughness.Load(int3(Pixel, 0));
    float3 Albedo = MaterialData.rgb;
    float Metallic = MaterialData.a;
    float Roughness = NormalData.a;
    float3 N = NormalData.xyz * rsqrt(max(dot(NormalData.xyz, NormalData.xyz), 1.0e-8f));

    // 첫 드로우는 기본 조명을 HDR에 쓰고, 두 번째 드로우는 포인트 라이트만 더한다.
    if (ViewportLightCount.w < 0.5f)
    {
        const float3 LightDir = normalize(float3(0.5f, 0.5f, -1.0f));
        const float3 AmbientColor = float3(0.5f, 0.5f, 0.5f);
        const float3 LightColor = float3(0.5f, 0.5f, 0.5f);
        float NdotL = saturate(dot(N, -LightDir));
        return float4(Albedo * (AmbientColor + LightColor * NdotL), 1.0f);
    }

    float2 UV = (float2(Pixel) + 0.5f) / ViewportLightCount.xy;
    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    if (!isfinite(WorldH.w) || abs(WorldH.w) < 1.0e-6f) return 0.0f;
    float3 WorldPosition = WorldH.xyz / WorldH.w;
    // Orthographic rays are parallel; their viewing direction does not depend on position.
    float3 ToCamera = CameraPosition.w > 0.5f
        ? CameraPosition.xyz - WorldPosition : -CameraForward.xyz;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));

    float3 PointLighting = 0.0f;

    [loop]
    for (uint Index = 0; Index < (uint)ViewportLightCount.z; ++Index)
    {
        FPointLightData Light = PointLights[Index];
        float3 ToLight = Light.PositionRadius.xyz - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.PositionRadius.w * Light.PositionRadius.w;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f) continue;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;
        float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
        float Attenuation = RadiusFade * RadiusFade / (1.0f + DistanceSquared);
        float3 SurfaceResponse = EvaluateSurfaceBRDF(Albedo, Metallic, Roughness, N, V, L);
        float3 LightRadiance = Light.ColorIntensity.rgb * Light.ColorIntensity.a * Attenuation;
        PointLighting += SurfaceResponse * LightRadiance;
    }

    return float4(PointLighting, 0.0f);
}
