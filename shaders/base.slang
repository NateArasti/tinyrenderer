struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
};

struct PushConstants
{
    float4x4 model;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;
[[vk::push_constant]] PushConstants pcs;

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
    output.position = mul(ubo.proj, mul(ubo.view, mul(pcs.model, float4(input.inPosition, 1.0))));
    output.normal = input.inNormal;
    output.fragTexCoord = input.inTexCoord;
    output.fragColor = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return vertIn.fragColor;
}
