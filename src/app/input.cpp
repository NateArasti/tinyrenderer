#include "input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <unordered_map>

namespace tr::App {
    namespace {
        std::unordered_map<void*, Input*> inputs;

        Key toKey(int key) {
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
                return static_cast<Key>(static_cast<int>(Key::Digit0) + key - GLFW_KEY_0);
            }
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                return static_cast<Key>(static_cast<int>(Key::A) + key - GLFW_KEY_A);
            }
            if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
                return static_cast<Key>(static_cast<int>(Key::F1) + key - GLFW_KEY_F1);
            }
            if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
                return static_cast<Key>(static_cast<int>(Key::Keypad0) + key - GLFW_KEY_KP_0);
            }

            switch (key) {
            case GLFW_KEY_TAB: return Key::Tab;
            case GLFW_KEY_LEFT: return Key::Left;
            case GLFW_KEY_RIGHT: return Key::Right;
            case GLFW_KEY_UP: return Key::Up;
            case GLFW_KEY_DOWN: return Key::Down;
            case GLFW_KEY_PAGE_UP: return Key::PageUp;
            case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
            case GLFW_KEY_HOME: return Key::Home;
            case GLFW_KEY_END: return Key::End;
            case GLFW_KEY_INSERT: return Key::Insert;
            case GLFW_KEY_DELETE: return Key::Delete;
            case GLFW_KEY_BACKSPACE: return Key::Backspace;
            case GLFW_KEY_SPACE: return Key::Space;
            case GLFW_KEY_ENTER: return Key::Enter;
            case GLFW_KEY_ESCAPE: return Key::Escape;
            case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
            case GLFW_KEY_COMMA: return Key::Comma;
            case GLFW_KEY_MINUS: return Key::Minus;
            case GLFW_KEY_PERIOD: return Key::Period;
            case GLFW_KEY_SLASH: return Key::Slash;
            case GLFW_KEY_SEMICOLON: return Key::Semicolon;
            case GLFW_KEY_EQUAL: return Key::Equal;
            case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
            case GLFW_KEY_BACKSLASH: return Key::Backslash;
            case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
            case GLFW_KEY_GRAVE_ACCENT: return Key::GraveAccent;
            case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
            case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
            case GLFW_KEY_NUM_LOCK: return Key::NumLock;
            case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
            case GLFW_KEY_PAUSE: return Key::Pause;
            case GLFW_KEY_KP_DECIMAL: return Key::KeypadDecimal;
            case GLFW_KEY_KP_DIVIDE: return Key::KeypadDivide;
            case GLFW_KEY_KP_MULTIPLY: return Key::KeypadMultiply;
            case GLFW_KEY_KP_SUBTRACT: return Key::KeypadSubtract;
            case GLFW_KEY_KP_ADD: return Key::KeypadAdd;
            case GLFW_KEY_KP_ENTER: return Key::KeypadEnter;
            case GLFW_KEY_KP_EQUAL: return Key::KeypadEqual;
            case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
            case GLFW_KEY_LEFT_CONTROL: return Key::LeftControl;
            case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
            case GLFW_KEY_LEFT_SUPER: return Key::LeftSuper;
            case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
            case GLFW_KEY_RIGHT_CONTROL: return Key::RightControl;
            case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
            case GLFW_KEY_RIGHT_SUPER: return Key::RightSuper;
            case GLFW_KEY_MENU: return Key::Menu;
            default: return Key::Unknown;
            }
        }

        int toGlfwKey(Key key) {
            if (key >= Key::Digit0 && key <= Key::Digit9) {
                return GLFW_KEY_0 + static_cast<int>(key) - static_cast<int>(Key::Digit0);
            }
            if (key >= Key::A && key <= Key::Z) {
                return GLFW_KEY_A + static_cast<int>(key) - static_cast<int>(Key::A);
            }

            switch (key) {
            case Key::Q: return GLFW_KEY_Q;
            case Key::W: return GLFW_KEY_W;
            case Key::E: return GLFW_KEY_E;
            case Key::A: return GLFW_KEY_A;
            case Key::S: return GLFW_KEY_S;
            case Key::D: return GLFW_KEY_D;
            default: return GLFW_KEY_UNKNOWN;
            }
        }

        uint32_t toModifiers(int modifiers) {
            uint32_t result = KeyModifierNone;
            if (modifiers & GLFW_MOD_CONTROL) result |= KeyModifierControl;
            if (modifiers & GLFW_MOD_SHIFT) result |= KeyModifierShift;
            if (modifiers & GLFW_MOD_ALT) result |= KeyModifierAlt;
            if (modifiers & GLFW_MOD_SUPER) result |= KeyModifierSuper;
            return result;
        }
    }

    Input::Input(const Window& window) : _window(window.nativeHandle()) {
        inputs.emplace(_window, this);
        auto* glfwWindow = static_cast<GLFWwindow*>(_window);
        glfwSetKeyCallback(glfwWindow, [](GLFWwindow* window, int key, int, int action, int modifiers) {
            onKey(window, key, action, modifiers);
        });
        glfwSetCharCallback(glfwWindow, [](GLFWwindow* window, unsigned int codepoint) {
            onCharacter(window, codepoint);
        });
        glfwSetScrollCallback(glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset) {
            onScroll(window, xOffset, yOffset);
        });
    }

    Input::~Input() {
        inputs.erase(_window);
    }

    void Input::beginFrame() {
        _scrollDelta = 0.0f;
        _keyEvents.clear();
        _textInput.clear();
    }

    glm::vec2 Input::mousePosition() const {
        double x;
        double y;
        glfwGetCursorPos(static_cast<GLFWwindow*>(_window), &x, &y);
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    bool Input::isMouseButtonDown(MouseButton button) const {
        return glfwGetMouseButton(
            static_cast<GLFWwindow*>(_window),
            static_cast<int>(button)
        ) == GLFW_PRESS;
    }

    bool Input::isKeyPressed(Key key) const {
        const int glfwKey = toGlfwKey(key);
        return glfwKey != GLFW_KEY_UNKNOWN
            && glfwGetKey(static_cast<GLFWwindow*>(_window), glfwKey) == GLFW_PRESS;
    }

    bool Input::isKeyJustPressed(Key key) const {
        for (const KeyEvent& event : keyEvents()) {
            if (event.key == Key::F1 && event.pressed && !event.repeated) {
                return true;
            }
        }

        return false;
    }

    void Input::captureCursor(bool captured) {
        glfwSetInputMode(
            static_cast<GLFWwindow*>(_window),
            GLFW_CURSOR,
            captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );
    }

    void Input::onKey(void* window, int key, int action, int modifiers) {
        auto it = inputs.find(window);
        if (it == inputs.end()) return;

        const Key inputKey = toKey(key);
        if (inputKey == Key::Unknown) return;

        it->second->_keyEvents.push_back({
            .key = inputKey,
            .pressed = action != GLFW_RELEASE,
            .repeated = action == GLFW_REPEAT,
            .modifiers = toModifiers(modifiers)
        });
    }

    void Input::onCharacter(void* window, unsigned int codepoint) {
        auto it = inputs.find(window);
        if (it != inputs.end()) it->second->_textInput.push_back(static_cast<char32_t>(codepoint));
    }

    void Input::onScroll(void* window, double, double yOffset) {
        auto it = inputs.find(window);
        if (it != inputs.end()) it->second->_scrollDelta += static_cast<float>(yOffset);
    }
}
