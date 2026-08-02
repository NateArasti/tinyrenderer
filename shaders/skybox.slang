import scene_data;

struct PushConstants {
    float3 color;
    float rotation;
};

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(0, 1)]] SamplerCube cubemap;

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
        -1.0
    ));
    float3 worldDirection = normalize(mul(transpose((float3x3)sceneData.view), viewDirection));

    float c = cos(pcs.rotation);
    float s = sin(pcs.rotation);
    float3 rotatedDirection = float3(
        c * worldDirection.x - s * worldDirection.z,
        worldDirection.y,
        s * worldDirection.x + c * worldDirection.z
    );
    return float4(cubemap.Sample(rotatedDirection).rgb * pcs.color, 1.0);
}
