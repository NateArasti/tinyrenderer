module Math;

public static const float EPS = 1e-6;
public static const float INFINITY = 1.0 / 0.0;
public static const float PI = 3.14159265359;

public float3 rotate(float3 v, float3 axis, float rotation) {
    float3 k = normalize(axis);
    float c = cos(rotation);
    float s = sin(rotation);
    return v * c + cross(v, k) * s + k * dot(k, v) * (1.0 - c);
}

public float2 sphereIntersection(float3 rayStart, float3 rayDir, float3 sphereCenter, float sphereRadius) {
    rayStart -= sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayStart, rayDir);
    float c = dot(rayStart, rayStart) - (sphereRadius * sphereRadius);
    float d = b * b - 4 * a * c;
    if (d < 0)
    {
        return -1;
    }
    else
    {
        d = sqrt(d);
        return float2(-b - d, -b + d) / (2 * a);
    }
}

public float random(float2 st) {
    return fract(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

// 2D Noise based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
public float noise(float2 st) {
    float2 i = floor(st);
    float2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + float2(1.0, 0.0));
    float c = random(i + float2(0.0, 1.0));
    float d = random(i + float2(1.0, 1.0));

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    float2 u = f * f * (3.0 - 2.0 * f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
