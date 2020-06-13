#ifndef ARBIYI_VULKAN_TEXTURE_HPP
#define ARBIYI_VULKAN_TEXTURE_HPP

#include "aryibi/renderer.hpp"
#include "forwards.hpp"
#include "image.hpp"
#include "types.hpp"

#include <vulkan/vulkan.hpp>

namespace aryibi::renderer {
    struct Texture {
        Image image{};

        [[nodiscard]] vk::DescriptorImageInfo info(const vk::Sampler sampler) const;
        void destroy();
    };

    [[nodiscard]] Texture load_texture(const std::string& path, const vk::Format format);
    [[nodiscard]] Texture load_texture(const u8* data, const u32 width, const u32 height, const u32 channels, const vk::Format format);
} // namespace aryibi::renderer

#endif //ARBIYI_VULKAN_TEXTURE_HPP
