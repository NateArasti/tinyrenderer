module Light;

import math;

public float distributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

public float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

public float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

public float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

public float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

public float calculateHardShadow(Sampler2D shadowMap, float4 shadowCoord, float NdotL) {
    float3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float closestDepth = shadowMap.Sample(projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - NdotL), 0.0015);
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
    return shadow;
}

public float calculateSoftShadow(Sampler2D shadowMap, float4 shadowCoord, float NdotL) {
    float3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float bias = max(0.005 * (1.0 - NdotL), 0.0015);
    float currentDepth = projCoords.z - bias;

    uint width, height;
    shadowMap.GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);

    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float2 offset = float2(x, y) * texelSize;
            shadow += shadowMap.SampleCmpLevelZero(projCoords.xy + offset, currentDepth);
        }
    }
    return shadow / 9.0;
}

public float3 calculateLight(float3 color, float3 N, float3 L, float3 V, float3 F0, float roughness, float metallic) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL > 0.0 && NdotV > 0.0) {
        float3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float cosTheta = max(dot(H, V), 0.0);

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        return (kD * color / PI + specular) * NdotL;
    }
    else {
        return float3(0);
    }
}
