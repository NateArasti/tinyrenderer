import environment;
import math;
import procedural_sky_data;

struct PushConstants {
    uint face;
};

[[vk::push_constant]] PushConstants constants;
[[vk::binding(0, 0)]] ConstantBuffer<EnvironmentData> environmentData;
[[vk::binding(1, 0)]] ConstantBuffer<ProceduralSkyData> skyData;

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
float4 fragMain(VSOutput input) : SV_TARGET {
    const float3 direction = faceDirection(constants.face, input.coordinate);
    return float4(calculateSky(float3(0.0), direction, environmentData, skyData), 1.0);
}
