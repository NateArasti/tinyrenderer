module Scene;

public struct InstanceData {
    public column_major float4x4 model;
    public column_major float4x4 normal;
    public uint materialIndex;
};

public struct CameraData {
    public float4x4 view;
    public float4x4 proj;
    public float3 cameraPos;
};
