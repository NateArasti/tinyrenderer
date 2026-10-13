import light;
import environment;
import camera_data;
import math;

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
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;
[[vk::binding(1, 0)]] Sampler2D shadowMap;

[[vk::binding(0, 1)]] ConstantBuffer<UniformParams> params;
[[vk::binding(1, 1)]] Sampler2D diffuseMap;
[[vk::binding(2, 1)]] Sampler2D emissionMap;
[[vk::binding(3, 1)]] Sampler2D normalMap;
[[vk::binding(4, 1)]] Sampler2D metallicMap;
[[vk::binding(5, 1)]] Sampler2D roughnessMap;
[[vk::binding(6, 1)]] Sampler2D specularMap;
[[vk::binding(7, 1)]] Sampler2D opacityMap;

[[vk::binding(0, 2)]] ConstantBuffer<EnvironmentData> environmentData;
[[vk::binding(1, 2)]] SamplerCube irradianceMap;
[[vk::binding(2, 2)]] SamplerCube prefilterMap;
[[vk::binding(3, 2)]] Sampler2D brdfLUT;

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

    output.position = mul(cameraData.proj, mul(cameraData.view, worldPos));
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul((float3x3)pcs.model, input.normal));
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn, bool isFrontFace: SV_IsFrontFace) : SV_TARGET {
    float4 baseColor = vertIn.color * params.diffuseColor;
    if (params.hasDiffuseMap > 0.5) {
        baseColor *= diffuseMap.Sample(vertIn.uv);
    }
    if (params.hasOpacityMap > 0.5) {
        baseColor.a *= opacityMap.Sample(vertIn.uv).r;
    }

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

    float3 N = normalize(vertIn.normal);
    if (params.hasNormalMap > 0.5) {
        N = applyNormalMap(N, vertIn.worldPos, vertIn.uv);
    }

    float3 Lo = float3(0);
    float3 V = normalize(cameraData.cameraPos - vertIn.worldPos);
    float3 L = normalize(-environmentData.lightDirection);

    float NdotL = max(dot(N, L), 0.0);

    float4 shadowCoord = mul(environmentData.lightViewProj, float4(vertIn.worldPos, 1));
    float shadow = isFrontFace && NdotL > 0.0
                       ? calculateSoftShadow(shadowMap, shadowCoord, NdotL)
                       : 1.0;
    float3 radiance = environmentData.lightColor.rgb * environmentData.lightIntensity;

    Lo += calculateLight(baseColor.rgb, N, L, V, F0, roughness, metallic) * radiance * shadow;

    float3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float NdotV = max(dot(N, V), 0.0);
    float3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 rotatedN = rotate(N, float3(0.0, 1.0, 0.0), environmentData.rotation);

    float3 R = reflect(-V, N);
    float3 rotatedR = rotate(R, float3(0.0, 1.0, 0.0), environmentData.rotation);

    static const float MAX_REFLECTION_LOD = 4.0;

    float3 irradiance;
    float3 prefiltered;
    if (environmentData.hasSkybox > 0.5) {
        irradiance = irradianceMap.Sample(rotatedN).rgb;
        prefiltered = prefilterMap.SampleLevel(rotatedR, roughness * MAX_REFLECTION_LOD).rgb;
    }
    else {
        // A constant environment remains constant after convolution.
        irradiance = environmentData.clearColor;
        prefiltered = environmentData.clearColor;
    }

    float3 environmentScale = environmentData.tint * environmentData.exposure;
    irradiance *= environmentScale;
    prefiltered *= environmentScale;

    float2 brdf = brdfLUT.Sample(float2(NdotV, roughness)).rg;

    float3 diffuseIBL = kD * irradiance * baseColor.rgb;
    float3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y);
    float3 ambient = diffuseIBL + specularIBL;

    float3 emission = params.emissionColor.rgb;
    if (params.hasEmissionMap > 0.5) {
        emission *= emissionMap.Sample(vertIn.uv).rgb;
    }

    float3 color = ambient + Lo + emission;

    return float4(color, baseColor.a);
}
