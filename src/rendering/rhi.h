#pragma once

#include <span>
#include <vector>

#include <imgui.h>

#include "draw_commands.h"
#include "rendering_resources.h"
#include "shader.h"
#include "scene_data.h"
#include "environment.h"

namespace tr::Rendering {
    class RHI {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        RHI(const RHI&) = delete;
        RHI& operator=(const RHI&) = delete;
        RHI(RHI&&) = delete;
        RHI& operator=(RHI&&) = delete;

        virtual std::string getDeviceName() const = 0;
        virtual void resize(uint32_t width, uint32_t height) = 0;
        virtual uint32_t getMsaaSamples() const = 0;
        virtual std::vector<uint32_t> getSupportedMsaaSamples() const = 0;
        virtual void setMsaaSamples(uint32_t samples) = 0;

        virtual RenderingResources& resources() = 0;

        virtual void startFrame(const tr::Rendering::SceneData& sceneData) = 0;
        virtual void renderSkybox(const tr::Data::Environment& environment) = 0;
        virtual void renderShadowPass(std::span<const DrawCommand> commands) = 0;
        virtual void renderColorPass(std::span<const DrawCommand> commands) = 0;
        virtual void prepareUI() = 0;
        virtual void drawUI(ImDrawData* drawData) = 0;
        virtual void endFrame() = 0;
    };
}
