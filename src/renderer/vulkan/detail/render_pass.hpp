#ifndef ARYIBI_VULKAN_RENDERPASS_HPP
#define ARYIBI_VULKAN_RENDERPASS_HPP

#include "image.hpp"

#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <string>
#include <vector>

namespace aryibi::renderer {
    struct RenderTarget {
        std::string name{};
        Image image{};
    };

    class RenderPass {
        vk::RenderPass _handle{};
        vk::Framebuffer _framebuffer{};
        std::vector<RenderTarget> _targets{};
    public:
        RenderPass& attach(const std::string& name, const Image::CreateInfo& info);
        void create(const vk::RenderPassCreateInfo& info);
        void destroy();

        [[nodiscard]] vk::RenderPass handle() const;
        [[nodiscard]] vk::Framebuffer framebuffer() const;
        [[nodiscard]] const Image& operator [](const std::string& name) const;
    };
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_RENDERPASS_HPP
