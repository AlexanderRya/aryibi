#include "aryibi/windowing.hpp"
#include "impl_types.hpp"
#include "util/aryibi_assert.hpp"

#include <GLFW/glfw3.h>

namespace aryibi::windowing {

WindowHandle::WindowHandle() : p_impl(std::make_unique<impl>()) {}
WindowHandle::~WindowHandle() {}

WindowHandle::WindowHandle(WindowHandle const& b) : p_impl(std::make_unique<impl>()) { *p_impl = *b.p_impl; }
WindowHandle& WindowHandle::operator=(WindowHandle const& b) {
    if (this != &b) {
        *p_impl = *b.p_impl;
    }
    return *this;
}

void WindowHandle::init(u32 width,
                        u32 height,
                        std::string_view title,
                        WindowHintFlags const& hint_flags) {
    if(!impl::initialized_glfw) {
        if(!glfwInit()) return;
        impl::initialized_glfw = true;
    }
    for (const auto& [hint, value] : hint_flags.values) {
        int glfw_hint = 0;
        /* clang-format off */
        switch (hint) {
            case WindowHint::resizable: glfw_hint = GLFW_RESIZABLE; break;
            case WindowHint::visible: glfw_hint = GLFW_VISIBLE; break;
            case WindowHint::decorated: glfw_hint = GLFW_DECORATED; break;
            case WindowHint::focused: glfw_hint = GLFW_FOCUSED; break;
            case WindowHint::always_on_top: glfw_hint = GLFW_FLOATING; break;
            case WindowHint::maximized: glfw_hint = GLFW_MAXIMIZED; break;
            case WindowHint::center_cursor: glfw_hint = GLFW_CENTER_CURSOR; break;
            case WindowHint::transparent_background: glfw_hint = GLFW_TRANSPARENT_FRAMEBUFFER; break;
            case WindowHint::make_opengl_context: glfw_hint = GLFW_CLIENT_API; break;
            default: break;
        }
        /* clang-format on */
        if (glfw_hint == 0)
            continue;

        glfwWindowHint(glfw_hint, value);
    }
    const auto fullscreen_it = hint_flags.values.find(WindowHint::fullscreen);
    bool is_fullscreen = fullscreen_it != hint_flags.values.end() && fullscreen_it->second;
    p_impl->handle = glfwCreateWindow(width, height, title.data(),
                                      is_fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
    p_impl->init_time = glfwGetTime();
}

void WindowHandle::unload() {
    ARYIBI_ASSERT(exists(), "Tried to unload window that doesn't exist!");
    glfwDestroyWindow(p_impl->handle);
}

bool WindowHandle::exists() const { return p_impl->handle != nullptr; }

bool WindowHandle::should_close() const {
    ARYIBI_ASSERT(exists(),
                  "Tried to get the \"should close\" value of window that doesn't exist!");
    return glfwWindowShouldClose(p_impl->handle);
}

void WindowHandle::set_resolution(u32 width, u32 height) {
    ARYIBI_ASSERT(exists(), "Tried to set resolution of window that doesn't exist!");
    glfwSetWindowSize(p_impl->handle, width, height);
}

anton::math::Vector2 WindowHandle::get_resolution() const {
    ARYIBI_ASSERT(exists(), "Tried to get resolution of window that doesn't exist!");
    int w, h;
    glfwGetWindowSize(p_impl->handle, &w, &h);
    return {(float)w, (float)h};
}

double WindowHandle::time_since_opened() const {
    if (exists())
        return glfwGetTime() - p_impl->init_time;
    else
        return -1.0;
}

void poll_events() { glfwPollEvents(); }

InputHandle::InputHandle() : p_impl(std::make_unique<impl>()) {}
InputHandle::~InputHandle() {}

void InputHandle::init(WindowHandle window) { p_impl->window = window.p_impl->handle; }

void InputHandle::unload() { p_impl->window = nullptr; }

bool InputHandle::exists() { return p_impl->window != nullptr; }

bool InputHandle::is_pressed(InputKey key) {
    ARYIBI_ASSERT(exists(), "Tried to get input of input handle that isn't initialized!");
    // InputKey maps directly to GLFW keys, so no extra work is needed.
    return glfwGetKey(p_impl->window, (int)key);
}

bool InputHandle::is_pressed(MouseButton button) {
    ARYIBI_ASSERT(exists(), "Tried to get input of input handle that isn't initialized!");
    return glfwGetMouseButton(p_impl->window, (int)button);
}

anton::math::Vector2 InputHandle::mouse_pos() {
    ARYIBI_ASSERT(exists(), "Tried to get input of input handle that isn't initialized!");
    double x, y;
    glfwGetCursorPos(p_impl->window, &x, &y);
    return {(float)x, (float)y};
}

} // namespace aryibi::windowing