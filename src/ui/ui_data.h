#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>

#include <imgui.h>

namespace tr::UI {
    using UIValue = std::variant<bool, int, float, std::string>;

    struct UIProperty {
        std::string key;
        std::string label;
        UIValue value;

        std::function<void(const UIValue&)> onChanged;
    };

    struct UIButton {
        std::string key;
        std::string label;
        
        std::function<void()> onClick;
    };

    struct UILabel {
        std::string key;
        std::string label;
    };

    struct UISeparator { };
    struct UISameLine { };

    struct UIWindow {
        using UILine = std::variant<UIProperty, UIButton, UILabel, UISeparator, UISameLine>;

        bool leftSide = true;

        std::string name;
        float width = 235.0f;
        float height = 100.0f;
        std::vector<UILine> lines;
        ImGuiWindowFlags additionalFlags;

        template<typename T>
        T* get(const std::string_view key) {
            UILine* result = nullptr;
            for (auto& line : lines) {
                std::visit(
                    [&](auto& value) {
                        using ValueType = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<ValueType, T>) {
                            if (value.key == key) {
                                result = &line;
                            }
                        }
                    },
                    line
                );
                if (result != nullptr) {
                    return &std::get<T>(*result);
                }
            }

            return nullptr;
        }

        void setLabel(const std::string_view key, const std::string label) {
            for (auto& line : lines) {
                bool finished = false;
                std::visit(
                    [&](auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, UILabel>) {
                            if (value.key == key) {
                                value.label = label;
                                finished = true;
                            }
                        }
                    },
                    line
                );
                if (finished) {
                    return;
                }
            }
        }

        void setProperty(const std::string_view key, const UIValue& value) {
            for (auto& line : lines) {
                bool finished = false;
                std::visit(
                    [&](auto& lineValue) {
                        using T = std::decay_t<decltype(lineValue)>;
                        if constexpr (std::is_same_v<T, UIProperty>) {
                            if (lineValue.key == key) {
                                lineValue.value = value;
                                finished = true;
                            }
                        }
                    },
                    line
                );
                if (finished) {
                    return;
                }
            }
        }
    };

    struct UIState {
        std::vector<UIWindow*> windows;
    };

    struct UIFrame {
        ImDrawData* drawData;
        bool wantsMouse = false;
        bool wantsKeyboard = false;
    };
}
