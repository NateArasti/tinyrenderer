#pragma once

struct ImDrawData;

namespace tr::Rendering {
    class CommandBuffer;

    class UIRenderer {
    public:
        virtual ~UIRenderer() = default;

        virtual void beginFrame() = 0;
        virtual void draw(CommandBuffer& cmd, ImDrawData* drawData) = 0;
    };
}
