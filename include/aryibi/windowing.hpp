#ifndef ARYIBI_WINDOWING_HPP
#define ARYIBI_WINDOWING_HPP

#include <anton/math/vector2.hpp>
#include <anton/types.hpp>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace aryibi::renderer {
class Renderer;
}

namespace aryibi::windowing {
using namespace anton;

enum class WindowHint : u16 {
    none = 0,
    fullscreen = 1,
    resizable,
    visible,
    decorated,
    focused,
    always_on_top,
    maximized,
    center_cursor,
    transparent_background,
    make_opengl_context // Don't yell at me, this is obviously needed
};

struct WindowHintFlags {
    std::unordered_map<WindowHint, bool> values;
};

void poll_events();

struct InputHandle;
class WindowHandle {
public:
    /// Creates a null window handle. This does absolutely nothing - you have to call init() before
    /// using this object.
    WindowHandle();
    ~WindowHandle();

    WindowHandle(WindowHandle const&);
    WindowHandle& operator=(WindowHandle const&);

    /// Creates a new window and makes this handle point to it.
    /// @param width The initial width of the window, in pixels.
    /// @param height The initial height of the window, in pixels.
    /// @param title The initial title of the window.
    /// @param hint_flags The desired properties of the window.
    void
    init(u32 width, u32 height, std::string_view title, WindowHintFlags const& hint_flags = {});
    void unload();

    /// @returns True if the window handle has been initialized and not unloaded.
    [[nodiscard]] bool exists() const;

    /// Requires the window handle to point to a valid window.
    /// @returns True if the window has been told to close (By i.e. clicking the close button or
    /// Alt+F4)
    bool should_close() const;

    /// @returns The time, in seconds, since the call to init() was made, or -1 if this handle has
    /// not been initalized.
    double time_since_opened() const;

    /// Sets the resolution of the window in screen coordinates to a given value.
    /// Requires the window handle to point to a valid window.
    void set_resolution(u32 width, u32 height);
    /// Requires the window handle to point to a valid window.
    /// @returns The resolution of this window, in screen coordinates.
    [[nodiscard]] anton::math::Vector2 get_resolution() const;

private:
    friend class renderer::Renderer;
    friend class InputHandle;

    struct impl;
    std::unique_ptr<impl> p_impl;
};

enum class [[maybe_unused]] InputKey {
    k_SPACE = 32,
    k_APOSTROPHE = 39, // '
    k_COMMA = 44,      // ,
    k_MINUS = 45,      // -
    k_DOT = 46,        // .
    k_SLASH = 47,      // /
    k_0 = 48,
    k_1 = 49,
    k_2 = 50,
    k_3 = 51,
    k_4 = 52,
    k_5 = 53,
    k_6 = 54,
    k_7 = 55,
    k_8 = 56,
    k_9 = 57,
    k_SEMICOLON = 59, // ;
    k_EQUALS = 61,    // =
    k_A = 65,
    k_B = 66,
    k_C = 67,
    k_D = 68,
    k_E = 69,
    k_F = 70,
    k_G = 71,
    k_H = 72,
    k_I = 73,
    k_J = 74,
    k_K = 75,
    k_L = 76,
    k_M = 77,
    k_N = 78,
    k_O = 79,
    k_P = 80,
    k_Q = 81,
    k_R = 82,
    k_S = 83,
    k_T = 84,
    k_U = 85,
    k_V = 86,
    k_W = 87,
    k_X = 88,
    k_Y = 89,
    k_Z = 90,
    k_LEFT_BRACKET = 91,  // [
    k_BACKSLASH = 92,     /* \ */
    k_RIGHT_BRACKET = 93, // ]
    k_TICK = 96,          // `
    k_TAB = 258,
    k_ESCAPE = 256,
    k_ENTER = 257,
    k_BACKSPACE = 259,
    k_INSERT = 260,
    k_DELETE = 261,
    k_RIGHT = 262,
    k_LEFT = 263,
    k_DOWN = 264,
    k_UP = 265,
    k_PAGE_UP = 266,
    k_PAGE_DOWN = 267,
    k_HOME = 268,
    k_END = 269,
    k_CAPS_LOCK = 280,
    k_NUMLOCK = 282,
    k_F1 = 290,
    k_F2 = 291,
    k_F3 = 292,
    k_F4 = 293,
    k_F5 = 294,
    k_F6 = 295,
    k_F7 = 296,
    k_F8 = 297,
    k_F9 = 298,
    k_F10 = 299,
    k_F11 = 300,
    k_F12 = 301,
    k_NUMPAD_0 = 320,
    k_NUMPAD_1 = 321,
    k_NUMPAD_2 = 322,
    k_NUMPAD_3 = 323,
    k_NUMPAD_4 = 324,
    k_NUMPAD_5 = 325,
    k_NUMPAD_6 = 326,
    k_NUMPAD_7 = 327,
    k_NUMPAD_8 = 328,
    k_NUMPAD_9 = 329,
    k_NUMPAD_DECIMAL = 330,
    k_NUMPAD_DIVIDE = 331,
    k_NUMPAD_MULTIPLY = 332,
    k_NUMPAD_SUBTRACT = 333,
    k_NUMPAD_ADD = 334,
    k_NUMPAD_ENTER = 335,
    k_LEFT_SHIFT = 340,
    k_LEFT_CONTROL = 341,
    k_LEFT_ALT = 342,
    k_RIGHT_SHIFT = 344,
    k_RIGHT_CONTROL = 345,
    k_RIGHT_ALT = 346,
    MAX
};

enum class [[maybe_unused]] MouseButton {
    left = 0,
    right,
    middle,
    custom_4,
    custom_5,
    custom_6,
    custom_7,
    custom_8
};

class InputHandle {
public:
    /// Creates a null input handle. This does absolutely nothing - you have to call init() before
    /// using this object.
    InputHandle();
    ~InputHandle();

    /// Initialize this input to be bound to a window. Some implementations might allow the window
    /// handle to be null, meaning that the input won't be bound to a specific window instance. To
    /// check if the input was initialized successfully, check the value of exists() after calling
    /// this function. This function must be called only if exists() is false, otherwise the
    /// behaviour is implementation-defined.
    /// @param window The window that this input will be bound to. If nullptr, The interface will
    /// try to get global input. If anything failed, this won't do anything and exists() will
    /// continue being false.
    void init(WindowHandle window);
    /// Resets the input handle. This is most commonly useless, but if for some reason you want to
    /// temporarily block inputs from this object, you can use this function.
    void unload();

    /// @returns True if the input handle has been initialized and not unloaded.
    bool exists();

    /// @returns True if the key was pressed between now and the last event poll.
    bool is_pressed(InputKey key);
    /// @returns True if the mouse button was pressed between now and the last event poll.
    bool is_pressed(MouseButton button);

    /// @returns The mouse position, in pixels.
    anton::math::Vector2 mouse_pos();

    // TODO: Add key/mouse callbacks

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace aryibi::windowing

#endif // ARYIBI_WINDOWING_HPP
