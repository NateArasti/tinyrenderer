import ibl;

struct VSInput {
    float2 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.position * 0.5 + 0.5;
    return output;
}

float2 integrateBRDF(float NdotV, float roughness) {
    float3 N = float3(0.0, 0.0, 1.0);
    float3 V = float3(
        sqrt(max(1.0 - NdotV * NdotV, 0.0)),
        0.0,
        NdotV
    );

    float A = 0.0;
    float B = 0.0;

    static const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float2 xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importanceSampleGGX(xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL <= 0.0) {
            continue;
        }

        float G = geometrySmithIBL(N, V, L, roughness);
        float GVis = G * VdotH /
            max(NdotH * NdotV, 0.0001);
        float Fc = pow(1.0 - VdotH, 5.0);

        A += (1.0 - Fc) * GVis;
        B += Fc * GVis;
    }

    return float2(A, B) / float(SAMPLE_COUNT);
}

[shader("fragment")]
float2 fragMain(VSOutput input) : SV_TARGET {
    return integrateBRDF(
        clamp(input.uv.x, 0.0001, 1.0),
        clamp(input.uv.y, 0.0, 1.0)
    );
}
