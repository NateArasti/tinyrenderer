module Light;

public static const float PI = 3.14159265359;

// Normal Distribution Function (D) - GGX/Trowbridge-Reitz Distribution
// Describes the statistical distribution of microfacet orientations
public float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness; // Remapping for more perceptual linearity
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2; // Numerator: concentration factor
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom; // Normalization factor

    return nom / denom; // Normalized distribution
}

// Geometry Function (G) - Smith's method with Schlick-GGX approximation
// Models self-shadowing and masking between microfacets
public float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // Direct lighting remapping

    // Geometry obstruction from view direction (masking)
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    // Geometry obstruction from light direction (shadowing)
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

    return ggx1 * ggx2; // Combined masking-shadowing
}

// Fresnel Reflectance (F) - Schlick's approximation
// Models how reflectance changes with viewing angle
public float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
