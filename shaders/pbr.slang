import light;

struct SceneData {
    float4x4 view;
    float4x4 proj;
    float3 cameraPos;
    float lightIntensity;
    float3 lightDirection;
    float _pad0;
    float4 lightColor;
    float4x4 lightViewProj;
};

struct PushConstants {
    float4x4 model;
};

struct UniformParams {
    float4 diffuseColor = float4(1, 1, 1, 1);
    float4 ambientColor;
    float4 emissionColor;
    float4 specularColor;
    float metallicFactor = 0;
    float roughnessFactor = 1;
    float normalScale = 1;
    float hasDiffuseMap = 0;
    float hasEmissionMap = 0;
    float hasNormalMap = 0;
    float hasMetallicMap = 0;
    float hasRoughnessMap = 0;
    float hasSpecularMap = 0;
    float hasOpacityMap = 0;
};

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(1, 0)]] Sampler2D shadowMap;
[[vk::binding(0, 1)]] ConstantBuffer<UniformParams> params;
[[vk::binding(1, 1)]] Sampler2D diffuseMap;
[[vk::binding(2, 1)]] Sampler2D emissionMap;
[[vk::binding(3, 1)]] Sampler2D normalMap;
[[vk::binding(4, 1)]] Sampler2D metallicMap;
[[vk::binding(5, 1)]] Sampler2D roughnessMap;
[[vk::binding(6, 1)]] Sampler2D specularMap;
[[vk::binding(7, 1)]] Sampler2D opacityMap;

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position; 
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

float calculateHardShadow(float4 shadowCoord, float NdotL) {
    float3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float closestDepth = shadowMap.Sample(projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - NdotL), 0.0015);
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
    return shadow;
} 

float calculateSoftShadow(float4 shadowCoord, float NdotL) {
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

float3 applyNormalMap(float3 normal, float3 worldPos, float2 uv) {
    float3 positionDx = ddx(worldPos);
    float3 positionDy = ddy(worldPos);
    float2 uvDx = ddx(uv);
    float2 uvDy = ddy(uv);

    float3 tangent = positionDy * uvDx.y - positionDx * uvDy.y;
    float3 bitangent = positionDx * uvDy.x - positionDy * uvDx.x;
    float maxLength = max(dot(tangent, tangent), dot(bitangent, bitangent));
    if (maxLength <= 0.0) {
        return normal;
    }

    float inverseLength = rsqrt(maxLength);
    float3 tangentNormal = normalMap.Sample(uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= params.normalScale;
    float3x3 tangentBasis = float3x3(
        tangent * inverseLength,
        bitangent * inverseLength,
        normal
    );
    return normalize(mul(tangentNormal, tangentBasis));
}

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(pcs.model, float4(input.position, 1.0));
    
    output.position = mul(sceneData.proj, mul(sceneData.view, worldPos));
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul((float3x3)pcs.model, input.normal));
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float4 baseColor = vertIn.color * params.diffuseColor;
    if (params.hasDiffuseMap > 0.5) {
        baseColor *= diffuseMap.Sample(vertIn.uv);
    }
    if (params.hasOpacityMap > 0.5) {
        baseColor.a *= opacityMap.Sample(vertIn.uv).r;
    }

    float3 N = normalize(vertIn.normal);
    if (params.hasNormalMap > 0.5) {
        N = applyNormalMap(N, vertIn.worldPos, vertIn.uv);
    }
    float3 V = normalize(sceneData.cameraPos - vertIn.worldPos);
    float3 R = reflect(-V, N);
    
    float3 L = normalize(-sceneData.lightDirection);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float cosTheta = max(dot(H, V), 0.0);

    float metallic = params.metallicFactor;
    if (params.hasMetallicMap > 0.5) {
        metallic *= metallicMap.Sample(vertIn.uv).r;
    }
    metallic = saturate(metallic);

    float roughness = params.roughnessFactor;
    if (params.hasRoughnessMap > 0.5) {
        roughness *= roughnessMap.Sample(vertIn.uv).r;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    float3 F0 = params.specularColor.rgb;
    if (params.hasSpecularMap > 0.5) {
        F0 = specularMap.Sample(vertIn.uv).rgb;
    }
    F0 = lerp(F0, baseColor.rgb, metallic);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(cosTheta, F0);

    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001; 
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic;

    float shadow = NdotL > 0.0
        ? calculateSoftShadow(mul(sceneData.lightViewProj, float4(vertIn.worldPos, 1)), NdotL)
        : 1.0;

    float3 radiance = sceneData.lightColor.rgb * sceneData.lightIntensity;
    float3 Lo = (kD * baseColor.rgb / PI + specular) * radiance * NdotL * shadow;

    float3 ambient = kD * params.ambientColor.rgb * baseColor.rgb;
    float3 emission = params.emissionColor.rgb;
    if (params.hasEmissionMap > 0.5) {
        emission *= emissionMap.Sample(vertIn.uv).rgb;
    }
    float3 color = ambient + Lo + emission;

    // HDR Tonemapping (Reinhard) and Gamma Correction
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, baseColor.a);
}
