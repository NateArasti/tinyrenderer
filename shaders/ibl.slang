module IBL;

import math;

public float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

public float2 hammersley(uint i, uint count) {
    return float2(float(i) / float(count), radicalInverseVdC(i));
}

public float3 importanceSampleGGX(float2 xi, float3 N, float roughness) {
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) /
                          (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    float3 H = float3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );

    float3 up = abs(N.z) < 0.999
                    ? float3(0.0, 0.0, 1.0)
                    : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

public float geometrySchlickGGXIBL(float NdotV, float roughness) {
    float k = roughness * roughness * 0.5;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

public float geometrySmithIBL(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGXIBL(NdotV, roughness) * geometrySchlickGGXIBL(NdotL, roughness);
}
