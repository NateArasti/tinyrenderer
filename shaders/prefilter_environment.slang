import environment;
import ibl;
import light;
import math;

struct PushConstants {
    uint face;
    float roughness;
};

[[vk::push_constant]] PushConstants constants;
[[vk::binding(0, 0)]] SamplerCube environmentMap;

struct VSInput {
    float2 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 coordinate : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.coordinate = input.position;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    float3 N = faceDirection(constants.face, input.coordinate);
    float3 V = N;
    float3 R = V;

    uint sourceWidth;
    uint sourceHeight;
    environmentMap.GetDimensions(sourceWidth, sourceHeight);
    float sourceTexelSolidAngle = 4.0 * PI / (6.0 * float(sourceWidth * sourceHeight)); 
    
    static const uint SAMPLE_COUNT = 1024u;
    float3 result = float3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float2 xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importanceSampleGGX(xi, N, constants.roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) {
            continue;
        }

        float sourceLod = 0.0;
        if (constants.roughness > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float D = distributionGGX(N, H, constants.roughness);
            float pdf = D * NdotH / max(4.0 * HdotV, 0.0001);
            float sampleSolidAngle = 1.0 /
                                     (float(SAMPLE_COUNT) * pdf + 0.0001);
            sourceLod = max(
                0.0,
                0.5 * log2(sampleSolidAngle / sourceTexelSolidAngle)
            );
        }

        result += environmentMap.SampleLevel(L, sourceLod).rgb * NdotL;
        totalWeight += NdotL;
    }

    result /= max(totalWeight, 0.0001);
    return float4(result, 1.0);
}
