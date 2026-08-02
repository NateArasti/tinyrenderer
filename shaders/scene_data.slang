module Data;

public struct SceneData {
    public float4x4 view;
    public float4x4 proj;
    public float3 cameraPos;
    public float lightIntensity;
    public float3 lightDirection;
    public float _pad0;
    public float4 lightColor;
    public float4x4 lightViewProj;
};
