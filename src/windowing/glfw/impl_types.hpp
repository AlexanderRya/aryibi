#ifndef ARYIBI_IMPL_TYPES_HPP
#define ARYIBI_IMPL_TYPES_HPP

#include "aryibi/windowing.hpp"

struct GLFWwindow;

namespace aryibi::windowing {

struct WindowHandle::impl {
    GLFWwindow* handle = nullptr;
    double init_time;
    static inline bool initialized_glfw = false;
};

struct InputHandle::impl {
    GLFWwindow* window = nullptr;
};

}

#endif // ARYIBI_IMPL_TYPES_HPP
