[[vk::binding(0, 0)]] Sampler2D sourcePanorama;

struct PushConstants {
    uint face;
};

[[vk::push_constant]] PushConstants constants;

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

float3 faceDirection(uint face, float2 coordinate) {
    switch (face) {
        case 0: return normalize(float3( 1.0, -coordinate.y, -coordinate.x));
        case 1: return normalize(float3(-1.0, -coordinate.y,  coordinate.x));
        case 2: return normalize(float3( coordinate.x,  1.0,  coordinate.y));
        case 3: return normalize(float3( coordinate.x, -1.0, -coordinate.y));
        case 4: return normalize(float3( coordinate.x, -coordinate.y,  1.0));
        default: return normalize(float3(-coordinate.x, -coordinate.y, -1.0));
    }
}

float2 directionToEquirectangular(float3 direction) {
    static const float PI = 3.14159265359;
    return float2(
        atan2(direction.z, direction.x) / (2.0 * PI) + 0.5,
        acos(clamp(direction.y, -1.0, 1.0)) / PI
    );
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    const float3 direction = faceDirection(constants.face, input.coordinate);
    return sourcePanorama.Sample(directionToEquirectangular(direction));
}
