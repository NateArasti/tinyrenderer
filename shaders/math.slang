module Math;

public static const float PI = 3.14159265359;

public float3 rotate(float3 v, float3 axis, float rotation) {
    float3 k = normalize(axis);
    float c = cos(rotation);
    float s = sin(rotation);
    return v * c + cross(v, k) * s + k * dot(k, v) * (1.0 - c);
}
