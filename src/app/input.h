#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "window.h"

namespace tr::App {
    enum class Key {
        Unknown,
        Tab, Left, Right, Up, Down, PageUp, PageDown, Home, End, Insert, Delete, Backspace, Space, Enter, Escape,
        Apostrophe, Comma, Minus, Period, Slash, Semicolon, Equal, LeftBracket, Backslash, RightBracket, GraveAccent,
        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
        Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Keypad0, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
        KeypadDecimal, KeypadDivide, KeypadMultiply, KeypadSubtract, KeypadAdd, KeypadEnter, KeypadEqual,
        LeftShift, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper, Menu
    };

    enum class MouseButton : uint8_t {
        Left,
        Right,
        Middle
    };

    enum KeyModifier : uint32_t {
        KeyModifierNone = 0,
        KeyModifierControl = 1 << 0,
        KeyModifierShift = 1 << 1,
        KeyModifierAlt = 1 << 2,
        KeyModifierSuper = 1 << 3
    };

    struct KeyEvent {
        Key key;
        bool pressed;
        bool repeated;
        uint32_t modifiers;
    };

    class Input {
    private:
        void* _window = nullptr;
        float _scrollDelta = 0.0f;
        std::vector<KeyEvent> _keyEvents;
        std::vector<char32_t> _textInput;

        static void onKey(void* window, int key, int action, int modifiers);
        static void onCharacter(void* window, unsigned int codepoint);
        static void onScroll(void* window, double xOffset, double yOffset);

    public:
        explicit Input(const Window& window);
        ~Input();

        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;
        Input(Input&&) = delete;
        Input& operator=(Input&&) = delete;

        void beginFrame();

        glm::vec2 mousePosition() const;
        bool isMouseButtonDown(MouseButton button) const;
        bool isKeyPressed(Key key) const;
        bool isKeyJustPressed(Key key) const;
        float scrollDelta() const { return _scrollDelta; }
        std::span<const KeyEvent> keyEvents() const { return _keyEvents; }
        std::span<const char32_t> textInput() const { return _textInput; }
        void captureCursor(bool captured);
    };
}
