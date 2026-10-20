struct PushConstants {
    int exposure;
    int tonemapping;
};

[[vk::push_constant]] PushConstants pcs;
[[vk::binding(0, 0)]] Sampler2D colorSource;

struct VSInput {
    float2 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 reinhard(float3 col) {
    return col / (1 + col);
}

float3 filmicBase(float3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) - E / F;
}

float3 filmic(float3 col) {
    return filmicBase(2 * col) / filmicBase(11.2);
}

float3 aces(float3 col) {
    return (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
}

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0); 
    output.uv = input.position * 0.5 + 0.5;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    float3 color = max(colorSource.Sample(input.uv).rgb, 0);
    color *= exp2(pcs.exposure);

    switch (pcs.tonemapping) {
        case 1: color = reinhard(color); break;
        case 2: color = filmic(color); break;
        case 3: color = aces(color); break;
    }
    if (pcs.tonemapping != 0)
        color = saturate(color);
    return float4(color, 1.0);
}
