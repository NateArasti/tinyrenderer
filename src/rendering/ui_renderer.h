#pragma once

#include "ui_data.h"

namespace tr::Rendering {
    class CommandBuffer;

    class UIRenderer {
    public:
        virtual ~UIRenderer() = default;

        virtual void beginFrame() = 0;
        virtual void draw(CommandBuffer& cmd, const UI::UIDrawData* drawData) = 0;
    };
}
