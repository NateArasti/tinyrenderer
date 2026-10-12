#pragma once

#include <vector>
#include <optional>
#include <string>

#include "frame_context.h"
#include "rendering_resources.h"
#include "ui_renderer.h"

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
        virtual float getProjectionYSign() const = 0;
        virtual Format getDepthFormat() const = 0;
        virtual void waitIdle() = 0;

        virtual RenderingResources& resources() = 0;
        virtual UIRenderer& uiRenderer() = 0;

        virtual std::optional<FrameContext> startFrame() = 0;
        virtual void endFrame() = 0;
    };
}
