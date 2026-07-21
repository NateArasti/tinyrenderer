#include "ui_provider.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <type_traits>
#include <utility>

#include <imgui.h>

namespace tr::UI {
    namespace {
        ImGuiKey toImGuiKey(tr::App::Key key) {
            using Key = tr::App::Key;
            if (key >= Key::Digit0 && key <= Key::Digit9) {
                return static_cast<ImGuiKey>(ImGuiKey_0 + static_cast<int>(key) - static_cast<int>(Key::Digit0));
            }
            if (key >= Key::A && key <= Key::Z) {
                return static_cast<ImGuiKey>(ImGuiKey_A + static_cast<int>(key) - static_cast<int>(Key::A));
            }
            if (key >= Key::F1 && key <= Key::F12) {
                return static_cast<ImGuiKey>(ImGuiKey_F1 + static_cast<int>(key) - static_cast<int>(Key::F1));
            }

            switch (key) {
            case Key::Tab: return ImGuiKey_Tab;
            case Key::Left: return ImGuiKey_LeftArrow;
            case Key::Right: return ImGuiKey_RightArrow;
            case Key::Up: return ImGuiKey_UpArrow;
            case Key::Down: return ImGuiKey_DownArrow;
            case Key::PageUp: return ImGuiKey_PageUp;
            case Key::PageDown: return ImGuiKey_PageDown;
            case Key::Home: return ImGuiKey_Home;
            case Key::End: return ImGuiKey_End;
            case Key::Insert: return ImGuiKey_Insert;
            case Key::Delete: return ImGuiKey_Delete;
            case Key::Backspace: return ImGuiKey_Backspace;
            case Key::Space: return ImGuiKey_Space;
            case Key::Enter: return ImGuiKey_Enter;
            case Key::Escape: return ImGuiKey_Escape;
            case Key::LeftShift: return ImGuiKey_LeftShift;
            case Key::LeftControl: return ImGuiKey_LeftCtrl;
            case Key::LeftAlt: return ImGuiKey_LeftAlt;
            case Key::LeftSuper: return ImGuiKey_LeftSuper;
            case Key::RightShift: return ImGuiKey_RightShift;
            case Key::RightControl: return ImGuiKey_RightCtrl;
            case Key::RightAlt: return ImGuiKey_RightAlt;
            case Key::RightSuper: return ImGuiKey_RightSuper;
            default: return ImGuiKey_None;
            }
        }

        void drawButton(UIFrame& frame, UIButton& action) {
            const char* label = action.label.empty() ? action.key.c_str() : action.label.c_str();
            if (ImGui::Button(label)) {
                if (action.onClick) {
                    action.onClick();
                }
            }
        }

        void drawProperty(UIFrame& frame, UIProperty& property) {
            const char* label = property.label.empty() ? property.key.c_str() : property.label.c_str();
            std::visit(
                [&](auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    bool changed = false;

                    if constexpr (std::is_same_v<T, bool>) {
                        changed = ImGui::Checkbox(label, &value);
                    }
                    else if constexpr (std::is_same_v<T, int>) {
                        changed = ImGui::DragInt(label, &value);
                    }
                    else if constexpr (std::is_same_v<T, float>) {
                        changed = ImGui::DragFloat(label, &value, 0.01f);
                    }
                    else if constexpr (std::is_same_v<T, std::string>) {
                        std::array<char, 256> buffer{};
                        std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
                        changed = ImGui::InputText(label, buffer.data(), buffer.size());
                        value = buffer.data();
                    }
                    
                    if (changed) {
                        property.value = value;
                        if (property.onChanged) {
                            property.onChanged(property.value);
                        }
                    }
                },
                property.value
            );
        }
    }

    struct UIProvider::Impl {
        ImGuiContext* context = nullptr;
    };

    UIProvider::UIProvider(tr::App::Window& window, tr::App::Input& input)
        : _window(window), _input(input), _impl(std::make_unique<Impl>())
    {
        _impl->context = ImGui::CreateContext();
        ImGui::SetCurrentContext(_impl->context);
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
    }

    UIProvider::~UIProvider() {
        ImGui::DestroyContext(_impl->context);
    }

    UIFrame UIProvider::update(UIState& state) {
        ImGui::SetCurrentContext(_impl->context);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(
            static_cast<float>(_window.logicalWidth()),
            static_cast<float>(_window.logicalHeight())
        );
        if (_window.logicalWidth() != 0 && _window.logicalHeight() != 0) {
            io.DisplayFramebufferScale = ImVec2(
                static_cast<float>(_window.framebufferWidth()) / _window.logicalWidth(),
                static_cast<float>(_window.framebufferHeight()) / _window.logicalHeight()
            );
        }
        io.DeltaTime = std::max(_window.deltaTime(), 1.0f / 1000.0f);

        const auto mousePosition = _input.mousePosition();
        io.AddMousePosEvent(mousePosition.x, mousePosition.y);
        io.AddMouseButtonEvent(0, _input.isMouseButtonDown(tr::App::MouseButton::Left));
        io.AddMouseButtonEvent(1, _input.isMouseButtonDown(tr::App::MouseButton::Right));
        io.AddMouseButtonEvent(2, _input.isMouseButtonDown(tr::App::MouseButton::Middle));
        io.AddMouseWheelEvent(0.0f, _input.scrollDelta());
        for (const auto& event : _input.keyEvents()) {
            const ImGuiKey key = toImGuiKey(event.key);
            if (key != ImGuiKey_None) io.AddKeyEvent(key, event.pressed);
            io.AddKeyEvent(ImGuiMod_Ctrl, (event.modifiers & tr::App::KeyModifierControl) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (event.modifiers & tr::App::KeyModifierShift) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (event.modifiers & tr::App::KeyModifierAlt) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (event.modifiers & tr::App::KeyModifierSuper) != 0);
        }
        for (char32_t character : _input.textInput()) {
            io.AddInputCharacter(static_cast<unsigned int>(character));
        }

        ImGui::NewFrame();

        UIFrame result;

        const ImGuiWindowFlags panelFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse;

        const float leftPadding = 24.0f;
        const float rightPadding = _window.width() - 24.0f;
        const float topPadding = 24.0f;
        const float spacing = 8.0f;
        float leftY = 0.0f;
        leftY += topPadding;
        float rightY = 0.0f;
        rightY += topPadding;

        for (UIWindow* window : state.windows) {
            float padding = window->leftSide ? leftPadding : rightPadding - window->width;
            float& y = window->leftSide ? leftY : rightY;

            ImGui::SetNextWindowPos(ImVec2(padding, y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(window->width, window->height), ImGuiCond_Always);
            if (ImGui::Begin(window->name.c_str(), nullptr, panelFlags | window->additionalFlags)) {
                y += window->height + spacing;
                for (UIWindow::UILine& line : window->lines) {
                    std::visit(
                        [&](auto& value) {
                            using T = std::decay_t<decltype(value)>;

                            if constexpr (std::is_same_v<T, UISeparator>) {
                                ImGui::Separator();
                            }
                            else if constexpr (std::is_same_v<T, UISameLine>) {
                                ImGui::SameLine();
                            }
                            else if constexpr (std::is_same_v<T, UILabel>) {
                                ImGui::Text(value.label.c_str());
                            }
                            else if constexpr (std::is_same_v<T, UIButton>) {
                                drawButton(result, value);
                            }
                            else if constexpr (std::is_same_v<T, UIProperty>) {
                                drawProperty(result, value);
                            }
                        },
                        line
                    );
                }
            }
            ImGui::End();
        }
        ImGui::Render();

        result.drawData = ImGui::GetDrawData();

        result.wantsMouse = io.WantCaptureMouse;
        result.wantsKeyboard = io.WantCaptureKeyboard;
        return result;
    }
}
