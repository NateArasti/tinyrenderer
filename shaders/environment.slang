module Environment;

import math;

public struct EnvironmentData {
    public float3 clearColor;
    public float rotation;
    public float3 tint;
    public float exposure;
    float3 _pad;
    public float hasSkybox;
};

public float3 faceDirection(uint face, float2 coordinate) {
    switch (face) {
    case 0: return normalize(float3(1.0, -coordinate.y, -coordinate.x));
    case 1: return normalize(float3(-1.0, -coordinate.y, coordinate.x));
    case 2: return normalize(float3(coordinate.x, 1.0, coordinate.y));
    case 3: return normalize(float3(coordinate.x, -1.0, -coordinate.y));
    case 4: return normalize(float3(coordinate.x, -coordinate.y, 1.0));
    default: return normalize(float3(-coordinate.x, -coordinate.y, -1.0));
    }
}

public float2 directionToEquirectangular(float3 direction) {
    return float2(
        atan2(-direction.z, direction.x) / (2.0 * PI) + 0.5,
        acos(clamp(direction.y, -1.0, 1.0)) / PI
    );
}
