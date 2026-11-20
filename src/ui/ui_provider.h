#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "ui_data.h"
#include "input.h"
#include "window.h"

namespace tr::UI {
    class UIProvider {
    private:
        struct Impl;

        tr::App::Window& _window;
        tr::App::Input& _input;
        std::unique_ptr<Impl> _impl;

        template<typename Draw>
        bool drawProperty(const char* label, Draw draw);

    public:
        float minItemWidth = 80.0f;
        float windowWidth = 300.0f;

        UIProvider(tr::App::Window& window, tr::App::Input& input);
        ~UIProvider();

        UIProvider(const UIProvider&) = delete;
        UIProvider& operator=(const UIProvider&) = delete;
        UIProvider(UIProvider&&) = delete;
        UIProvider& operator=(UIProvider&&) = delete;

        bool dragFloat(
            const char* label,
            float* value,
            float speed = 1.0f,
            float min = 0.0f,
            float max = 0.0f,
            const char* format = "%.3f",
            SliderFlags flags = SliderFlags::None
        );
        bool sliderFloat(
            const char* label,
            float* value,
            float min,
            float max,
            const char* format = "%.3f",
            SliderFlags flags = SliderFlags::None
        );
        bool sliderInt(
            const char* label,
            int* value,
            int min,
            int max,
            const char* format = "%d",
            SliderFlags flags = SliderFlags::None
        );
        bool sliderAngle(
            const char* label,
            float* value,
            float min = -360.0f,
            float max = 360.0f,
            const char* format = "%.0f deg",
            SliderFlags flags = SliderFlags::None
        );
        bool colorEdit3(const char* label, float* value);
        bool inputInt(const char* label, int* value, int step = 1, int stepFast = 100, float width = 0.0f);
        bool combo(const char* label, int* selected, std::span<const char* const> items);
        bool button(const char* label, float width = 0.0f);
        bool checkbox(const char* label, bool* value);
        void progressBar(const char* label, float fraction, const char* overlay = nullptr);
        void text(std::string_view value, float fontScale = 1.0f);
        void spacing();
        void separator();
        void sameLine();
        bool beginRow(const char* id, int columns = 2);
        void nextColumn();
        void endRow();
        void beginDisabled(bool disabled);
        void endDisabled();
        void pushID(const char* id);
        void popID();
        bool collapsingHeader(const char* label);
        void indent();
        void unindent();
        void tooltip(std::string_view message);
        void messagePopup(const char* title, std::string_view message, bool& openRequested);

        UIFrame update(UIState& state);
    };
}
