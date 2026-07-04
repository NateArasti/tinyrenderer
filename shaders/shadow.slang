struct PushConstants {
    float4x4 lightViewProj;
    float4x4 model;
};

[[vk::push_constant]] PushConstants pcs;

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
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(pcs.lightViewProj, mul(pcs.model, float4(input.position, 1.0)));
    return output;
}
