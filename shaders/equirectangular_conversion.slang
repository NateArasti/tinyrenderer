import environment;
import math;

struct PushConstants {
    uint face;
};

[[vk::push_constant]] PushConstants constants;
[[vk::binding(0, 0)]] Sampler2D sourcePanorama;

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

float2 directionToEquirectangular(float3 direction) {
    return float2(
        atan2(-direction.z, direction.x) / (2.0 * PI) + 0.5,
        acos(clamp(direction.y, -1.0, 1.0)) / PI
    );
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    const float3 direction = faceDirection(constants.face, input.coordinate);
    return sourcePanorama.Sample(directionToEquirectangular(direction));
}
