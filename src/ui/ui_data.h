#pragma once

#include <functional>
#include <string>
#include <vector>

struct ImDrawData;

namespace tr::UI {
    struct UIDrawData {
        ImDrawData* native = nullptr;
    };

    struct UISection {
        std::string name;
        bool opened = false;
        std::function<void()> drawCallback;
    };

    struct UIState {
        std::string topLabel;
        std::vector<UISection> leftSections;
        std::vector<UISection> rightSections;
        std::function<void()> drawOverlay;
    };

    struct UIFrame {
        const UIDrawData* drawData = nullptr;
        bool wantsMouse = false;
        bool wantsKeyboard = false;
    };

    enum class SliderFlags {
        None,
        AlwaysClamp,
        WrapAround
    };
}
