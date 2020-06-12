#ifndef ARYIBI_VULKAN_SWAPCHAIN_HPP
#define ARYIBI_VULKAN_SWAPCHAIN_HPP

#include <vulkan/vulkan.hpp>

#include <vector>

namespace aryibi::renderer {
    struct Swapchain {
        vk::SwapchainKHR handle;
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> views;
        vk::Extent2D extent;
        vk::SurfaceFormatKHR format;

        void destroy();
    };
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_SWAPCHAIN_HPP
