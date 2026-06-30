import light;

struct SceneData {
    float4x4 view;
    float4x4 proj;
    float3 lightDirection;
    float lightIntensity;
    float4 lightColor;
    float3 cameraPos; 
};

struct PushConstants {
    float4x4 model;
};

struct UniformParams {
    float4 diffuseColor = float4(1, 1, 1, 1);
    float4 ambientColor;
    float4 specularColor;
    float metallicFactor = 0;
    float roughnessFactor = 1;
};

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(0, 1)]] ConstantBuffer<UniformParams> params;
[[vk::binding(1, 1)]] Sampler2D albedo;

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
    float4 baseColor = albedo.Sample(vertIn.uv) * vertIn.color * params.diffuseColor;

    float3 N = normalize(vertIn.normal);
    float3 V = normalize(sceneData.cameraPos - vertIn.worldPos);
    float3 R = reflect(-V, N);
    
    float3 L = normalize(-sceneData.lightDirection);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float cosTheta = max(dot(H, V), 0.0);

    float3 F0 = float3(0.04);
    F0 = lerp(F0, baseColor.rgb, params.metallicFactor);

    float D = DistributionGGX(NdotH, params.roughnessFactor);
    float G = GeometrySmith(NdotV, NdotL, params.roughnessFactor);
    float3 F = FresnelSchlick(cosTheta, F0);

    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001; 
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - params.metallicFactor;

    float3 radiance = sceneData.lightColor.rgb * sceneData.lightIntensity;
    float3 Lo = (kD * baseColor.rgb / PI + specular) * radiance * NdotL;

    float3 ambient = params.ambientColor.rgb * baseColor.rgb;
    float3 color = ambient + Lo;

    // HDR Tonemapping (Reinhard) and Gamma Correction
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, baseColor.a);
}
