#pragma once

#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

namespace tr::UI {
    struct UIWindow {
        std::string name;
        float width = 235.0f;
        float height = 100.0f;
        std::function<void()> drawCallback;
        ImGuiWindowFlags additionalFlags = ImGuiWindowFlags_None;
    };

    struct UIState {
        std::vector<UIWindow*> leftWindows;
        std::vector<UIWindow*> rightWindows;
    };

    struct UIFrame {
        ImDrawData* drawData;
        bool wantsMouse = false;
        bool wantsKeyboard = false;
    };
}
