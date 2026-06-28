struct CameraData {
    float4x4 view;
    float4x4 proj;
};

struct PushConstants
{
    float4x4 model;
};

struct UniformParams {
    float4 color = float4(1, 1, 1, 1);
}

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;
[[vk::binding(0, 1)]] ConstantBuffer<UniformParams> params;
[[vk::binding(1, 1)]] Sampler2D albedo;

struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
    float4 inColor;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;
    float2 fragTexCoord;
    float4 fragColor;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.position = mul(cameraData.proj, mul(cameraData.view, mul(pcs.model, float4(input.inPosition, 1.0))));
    output.normal = input.inNormal;
    output.fragTexCoord = input.inTexCoord;
    output.fragColor = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return albedo.Sample(vertIn.fragTexCoord) * vertIn.fragColor * params.color;
}
