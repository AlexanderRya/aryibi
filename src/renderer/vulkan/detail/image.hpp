#ifndef ARYIBI_VULKAN_IMAGE_HPP
#define ARYIBI_VULKAN_IMAGE_HPP

#include "types.hpp"

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace aryibi::renderer {
    struct Image {
        struct CreateInfo {
            u32 width{};
            u32 height{};
            u32 mips{};

            vk::Format format{};
            vk::ImageTiling tiling{};
            vk::SampleCountFlagBits samples{};
            vk::ImageAspectFlags aspect{};
            vk::ImageUsageFlags usage{};
        };

        u32 width{};
        u32 height{};
        u32 mips{};
        vk::Image handle{};
        vk::ImageView view{};
        vk::Format format{};
        vk::SampleCountFlagBits samples{};
        VmaAllocation allocation{};
    };

    [[nodiscard]] Image make_image(const Image::CreateInfo& info);
    void transition_image_layout(vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout, const u32 mips);
    void destroy_image(Image& image);
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_IMAGE_HPP