#pragma once

#include "camera_data.h"
#include "environment_data.h"
#include "command_buffer.h"
#include "rendering_structs.h"

namespace tr::Rendering {
    struct FrameContext {
        CommandBuffer& cmd;

        uint32_t slot;
        glm::vec2 extent;
        Handle<GPUTexture> backbuffer;
        Handle<GPUTexture> hdrTarget;
        Handle<GPUTexture> colorTarget;
        Handle<GPUTexture> depthTarget;
        Format colorFormat;
        TextureState backbufferInitialState;

        CameraData cameraData;
        Handle<GPUBuffer> cameraDataBuffer;
        EnvironmentData environmentData;
        Handle<GPUBuffer> environmentDataBuffer;
        Handle<GPUBuffer> instanceBuffer;
    };
}
