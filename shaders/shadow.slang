import shared_structs;

struct PushConstants {
    float4x4 lightViewProj;
};

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] StructuredBuffer<InstanceData> instances;

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input, uint instanceIndex: SV_VulkanInstanceID) {
    VSOutput output;
    InstanceData instance = instances[instanceIndex];
    output.position = mul(pcs.lightViewProj, mul(instance.model, float4(input.position, 1.0)));
    return output;
}
