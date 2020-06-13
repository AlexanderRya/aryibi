#include "command_buffer.hpp"
#include "context.hpp"
#include "image.hpp"

namespace aryibi::renderer {
    Image make_image(const Image::CreateInfo& info) {
        vk::ImageCreateInfo image_info{}; {
            image_info.imageType = vk::ImageType::e2D;
            image_info.extent = { {
                static_cast<u32>(info.width),
                static_cast<u32>(info.height),
                1
            } };
            image_info.mipLevels = info.mips;
            image_info.arrayLayers = 1;
            image_info.format = info.format;
            image_info.tiling = info.tiling;
            image_info.initialLayout = vk::ImageLayout::eUndefined;
            image_info.usage = info.usage;
            image_info.samples = info.samples;
            image_info.sharingMode = vk::SharingMode::eExclusive;
        }

        VmaAllocationCreateInfo allocation_create_info{}; {
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            allocation_create_info.requiredFlags = 0;
            allocation_create_info.preferredFlags = 0;
            allocation_create_info.memoryTypeBits = 0;
            allocation_create_info.pool = nullptr;
            allocation_create_info.pUserData = nullptr;
            allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }

        Image image{}; {
            image.format = info.format;
            image.width = info.width;
            image.height = info.height;
            image.samples = info.samples;
            image.mips = info.mips;
        }

        vmaCreateImage(
            context().allocator,
            &static_cast<const VkImageCreateInfo&>(image_info),
            &allocation_create_info,
            reinterpret_cast<VkImage*>(&image.handle),
            &image.allocation,
            nullptr);

        vk::ImageViewCreateInfo image_view_create_info{}; {
            image_view_create_info.image = image.handle;
            image_view_create_info.format = info.format;
            image_view_create_info.components.r = vk::ComponentSwizzle::eIdentity;
            image_view_create_info.components.g = vk::ComponentSwizzle::eIdentity;
            image_view_create_info.components.b = vk::ComponentSwizzle::eIdentity;
            image_view_create_info.components.a = vk::ComponentSwizzle::eIdentity;
            image_view_create_info.viewType = vk::ImageViewType::e2D;
            image_view_create_info.subresourceRange.aspectMask = info.aspect;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = info.mips;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;
        }

        image.view = context().device.logical.createImageView(image_view_create_info, nullptr);

        return image;
    }

    void transition_image_layout(vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout, const u32 mips) {
        auto command_buffer = begin_transient(); {
            vk::ImageMemoryBarrier image_memory_barrier{}; {
                image_memory_barrier.image = image;

                image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                image_memory_barrier.oldLayout = old_layout;
                image_memory_barrier.newLayout = new_layout;

                image_memory_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                image_memory_barrier.subresourceRange.baseMipLevel = 0;
                image_memory_barrier.subresourceRange.levelCount = mips;
                image_memory_barrier.subresourceRange.baseArrayLayer = 0;
                image_memory_barrier.subresourceRange.layerCount = 1;

                image_memory_barrier.srcAccessMask = {};
                image_memory_barrier.dstAccessMask = {};
            }

            vk::PipelineStageFlags source_stage{};
            vk::PipelineStageFlags destination_stage{};

            switch (old_layout) {
                case vk::ImageLayout::eUndefined: {
                    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
                    image_memory_barrier.srcAccessMask = {};
                } break;

                case vk::ImageLayout::eTransferDstOptimal: {
                    source_stage = vk::PipelineStageFlagBits::eTransfer;
                    image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                } break;

                default: {
                    throw std::runtime_error("Unsupported transition");
                }
            }

            switch (new_layout) {
                case vk::ImageLayout::eTransferDstOptimal: {
                    destination_stage = vk::PipelineStageFlagBits::eTransfer;
                    image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                } break;

                case vk::ImageLayout::eShaderReadOnlyOptimal: {
                    image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                    destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
                } break;

                case vk::ImageLayout::eDepthStencilAttachmentOptimal: {
                    image_memory_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                    destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
                    image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                } break;

                case vk::ImageLayout::ePresentSrcKHR: {
                    destination_stage = vk::PipelineStageFlagBits::eAllGraphics;
                    image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
                } break;

                default: {
                    throw std::runtime_error("Unsupported transition");
                }
            }

            command_buffer.pipelineBarrier(
                source_stage,
                destination_stage,
                vk::DependencyFlags{},
                nullptr,
                nullptr,
                image_memory_barrier);

            end_transient(command_buffer);
        }
    }

    void destroy_image(Image& image) {
        context().device.logical.destroy(image.view);
        vmaDestroyImage(context().allocator, static_cast<VkImage>(image.handle), image.allocation);

        image.handle = nullptr;
        image.view = nullptr;
    }
} // namespace aryibi::renderer