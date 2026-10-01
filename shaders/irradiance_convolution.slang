import environment;
import math;

struct PushConstants {
    uint face;
};

[[vk::push_constant]] PushConstants constants;
[[vk::binding(0, 0)]] SamplerCube environmentMap;

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

static const int AZIMUTH_STEPS = 128;
static const int ZENITH_STEPS = 32;

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    float3 normal = faceDirection(constants.face, input.coordinate);
    float3 irradiance = float3(0.0);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    uint sourceWidth, sourceHeight;
    environmentMap.GetDimensions(sourceWidth, sourceHeight);
    float texelSolidAngle = 4.0 * PI / (6.0 * float(sourceWidth) * float(sourceHeight));
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            float sampleSolidAngle = sampleDelta * sampleDelta * sin(theta);
            float lod = max(0.0, 0.5 * log2(max(sampleSolidAngle / texelSolidAngle, 1.0)));
            irradiance += environmentMap.SampleLevel(sampleVec, lod).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    return float4(irradiance, 1.0);
}
