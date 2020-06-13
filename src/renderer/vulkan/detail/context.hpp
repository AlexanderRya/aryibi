#ifndef ARYIBI_VULKAN_CONTEXT_HPP
#define ARYIBI_VULKAN_CONTEXT_HPP

#include "forwards.hpp"
#include "types.hpp"

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace aryibi::renderer {
    constexpr auto samples = vk::SampleCountFlagBits::e4; // Not needed, MSAA is disabled

    struct Context {
        vk::Instance instance{};
#if defined(ARYIBI_DEBUG)
        vk::DebugUtilsMessengerEXT validation{};
#endif
        struct {
            vk::Device logical{};
            vk::PhysicalDevice physical{};
            vk::Queue graphics{};
            u32 family{};
        } device{};
        VmaAllocator allocator{};
    };

    void initialise(GLFWwindow* window);
    void make_surface(GLFWwindow* window);
    [[nodiscard]] vk::SurfaceKHR acquire_surface(GLFWwindow* window);
    [[nodiscard]] const Context& context();
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_CONTEXT_HPP