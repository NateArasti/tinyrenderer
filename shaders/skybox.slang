import environment;
import scene_data;
import math;

[[vk::binding(0, 0)]] ConstantBuffer<EnvironmentData> environmentData;
[[vk::binding(1, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(2, 0)]] SamplerCube cubemap;

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
    float3 viewDirection = normalize(float3(
        input.coordinate.x / sceneData.proj[0][0],
        input.coordinate.y / sceneData.proj[1][1],
        1.0
    ));
    float3 worldDirection = normalize(mul(transpose((float3x3)sceneData.view), viewDirection));
    float3 rotatedDirection = rotate(worldDirection, float3(0, 1, 0), environmentData.rotation);
    float3 color = cubemap.Sample(rotatedDirection).rgb;
    color *= environmentData.tint * environmentData.exposure;
    return float4(color, 1.0);
}
