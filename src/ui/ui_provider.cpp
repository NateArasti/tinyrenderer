#include "ui_provider.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>

namespace tr::UI {
    namespace {
        ImGuiSliderFlags toImGuiFlags(SliderFlags flags) {
            switch (flags) {
            case SliderFlags::AlwaysClamp: return ImGuiSliderFlags_AlwaysClamp;
            case SliderFlags::WrapAround: return ImGuiSliderFlags_WrapAround;
            default: return ImGuiSliderFlags_None;
            }
        }

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
    }

    struct UIProvider::Impl {
        ImGuiContext* context = nullptr;
        UIDrawData drawData;
    };

    UIProvider::UIProvider(tr::App::Window& window, tr::App::Input& input)
        : _window(window), _input(input), _impl(std::make_unique<Impl>())
    {
        _impl->context = ImGui::CreateContext();
        ImGui::SetCurrentContext(_impl->context);
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowPadding.x = 24.0f;
    }

    UIProvider::~UIProvider() {
        ImGui::DestroyContext(_impl->context);
    }

    template<typename Draw>
    bool UIProvider::drawProperty(const char* label, Draw draw) {
        ImGui::PushID(label);

        const char* labelEnd = std::strstr(label, "##");
        const float labelWidth = ImGui::CalcTextSize(label, labelEnd).x;
        const float available = ImGui::GetContentRegionAvail().x;
        const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
        const bool inlineLabel = labelWidth > 0.0f && available >= minItemWidth + gap + labelWidth;

        const float itemWidth = available - (inlineLabel ? labelWidth + gap : 0.0f);
        ImGui::SetNextItemWidth(std::max(1.0f, itemWidth));
        const bool changed = draw("##value");

        if (labelWidth > 0.0f) {
            if (inlineLabel) {
                ImGui::SameLine(0.0f, gap);
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(label, labelEnd);
            ImGui::PopTextWrapPos();
        }

        ImGui::PopID();
        return changed;
    }

    bool UIProvider::dragFloat(
        const char* label,
        float* value,
        float speed,
        float min,
        float max,
        const char* format,
        SliderFlags flags
    ) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::DragFloat(id, value, speed, min, max, format, toImGuiFlags(flags));
        });
    }

    bool UIProvider::sliderFloat(
        const char* label,
        float* value,
        float min,
        float max,
        const char* format,
        SliderFlags flags
    ) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::SliderFloat(id, value, min, max, format, toImGuiFlags(flags));
        });
    }

    bool UIProvider::sliderInt(
        const char* label,
        int* value,
        int min,
        int max,
        const char* format,
        SliderFlags flags
    ) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::SliderInt(id, value, min, max, format, toImGuiFlags(flags));
        });
    }

    bool UIProvider::sliderAngle(
        const char* label,
        float* value,
        float min,
        float max,
        const char* format,
        SliderFlags flags
    ) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::SliderAngle(id, value, min, max, format, toImGuiFlags(flags));
        });
    }

    bool UIProvider::colorEdit3(const char* label, float* value) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::ColorEdit3(id, value);
        });
    }

    bool UIProvider::inputInt(const char* label, int* value, int step, int stepFast, float width) {
        return drawProperty(label, [&](const char* id) {
            if (width > 0.0f) {
                ImGui::SetNextItemWidth(std::min(width, ImGui::CalcItemWidth()));
            }
            return ImGui::InputInt(id, value, step, stepFast);
        });
    }

    bool UIProvider::combo(const char* label, int* selected, std::span<const char* const> items) {
        return drawProperty(label, [&](const char* id) {
            return ImGui::Combo(id, selected, items.data(), static_cast<int>(items.size()));
        });
    }

    bool UIProvider::button(const char* label, float width) {
        return ImGui::Button(label, ImVec2(width, 0.0f));
    }

    bool UIProvider::checkbox(const char* label, bool* value) {
        return ImGui::Checkbox(label, value);
    }

    void UIProvider::progressBar(const char* label, float fraction, const char* overlay) {
        drawProperty(label, [&](const char*) {
            ImGui::ProgressBar(fraction, ImVec2(ImGui::CalcItemWidth(), 0.0f), overlay);
            return false;
        });
    }

    void UIProvider::text(std::string_view value, float fontScale) {
        const char* begin = value.empty() ? "" : value.data();
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * fontScale);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(begin, begin + value.size());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    }

    void UIProvider::spacing() {
        ImGui::Spacing();
    }

    void UIProvider::separator() {
        ImGui::Separator();
    }

    void UIProvider::sameLine() {
        ImGui::SameLine();
    }

    bool UIProvider::beginRow(const char* id, int columns) {
        if (!ImGui::BeginTable(id, columns,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX)) {
            return false;
        }
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        for (int i = 1; i < columns; ++i) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        return true;
    }

    void UIProvider::nextColumn() {
        ImGui::TableNextColumn();
    }

    void UIProvider::endRow() {
        ImGui::EndTable();
    }

    void UIProvider::beginDisabled(bool disabled) {
        ImGui::BeginDisabled(disabled);
    }

    void UIProvider::endDisabled() {
        ImGui::EndDisabled();
    }

    void UIProvider::pushID(const char* id) {
        ImGui::PushID(id);
    }

    void UIProvider::popID() {
        ImGui::PopID();
    }

    bool UIProvider::collapsingHeader(const char* label) {
        const float offset = std::floor(ImGui::GetStyle().WindowPadding.x * 0.5f);
        ImGui::Indent(offset);
        const bool opened = ImGui::CollapsingHeader(label);
        ImGui::Unindent(offset);
        return opened;
    }

    void UIProvider::indent() {
        ImGui::Indent();
    }

    void UIProvider::unindent() {
        ImGui::Unindent();
    }

    void UIProvider::tooltip(std::string_view message) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::BeginTooltip();
            text(message);
            ImGui::EndTooltip();
        }
    }

    void UIProvider::messagePopup(const char* title, std::string_view message, bool& openRequested) {
        if (openRequested) {
            ImGui::OpenPopup(title);
            openRequested = false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            text(message);
            ImGui::Spacing();
            if (button("OK", 100.0f)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
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
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize;

        const float padding = std::min({ 24.0f, io.DisplaySize.x * 0.25f, io.DisplaySize.y * 0.25f });
        const float panelWidth = std::max(1.0f, std::min(windowWidth, io.DisplaySize.x - 2.0f * padding));
        const float maxHeight = std::max(1.0f, io.DisplaySize.y - 2.0f * padding);
        const auto drawSidePanel = [&](std::vector<UISection>& sections, bool leftSide) {
            const float x = leftSide ? padding : io.DisplaySize.x - padding;
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(panelWidth, 1.0f),
                ImVec2(panelWidth, maxHeight)
            );
            ImGui::SetNextWindowPos(
                ImVec2(x, padding), ImGuiCond_Always, ImVec2(leftSide ? 0.0f : 1.0f, 0.0f)
            );
            if (ImGui::Begin(leftSide ? "Left" : "Right", nullptr, panelFlags)) {
                ImGui::Spacing();
                for (auto& section : sections) {
                    ImGui::PushID(section.name.c_str());
                    ImGui::SetNextItemOpen(section.opened, ImGuiCond_Once);
                    section.opened = ImGui::CollapsingHeader(section.name.c_str());
                    if (section.opened && section.drawCallback) {
                        section.drawCallback();
                    }
                    ImGui::PopID();
                    ImGui::Spacing();
                }
            }
            ImGui::End();
        };

        drawSidePanel(state.leftSections, true);
        drawSidePanel(state.rightSections, false);
        if (state.drawOverlay) {
            state.drawOverlay();
        }
        if (!state.topLabel.empty()) {
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 textSize = ImGui::CalcTextSize(state.topLabel.c_str());
            ImGui::GetForegroundDrawList()->AddText(
                ImVec2(viewport->Pos.x + (io.DisplaySize.x - textSize.x) * 0.5f, viewport->Pos.y + padding),
                ImGui::GetColorU32(ImGuiCol_Text), state.topLabel.c_str()
            );
            ImGui::PopFont();
        }
        ImGui::Render();

        _impl->drawData.native = ImGui::GetDrawData();
        result.drawData = &_impl->drawData;
        result.wantsMouse = io.WantCaptureMouse;
        result.wantsKeyboard = io.WantCaptureKeyboard;
        return result;
    }
}
