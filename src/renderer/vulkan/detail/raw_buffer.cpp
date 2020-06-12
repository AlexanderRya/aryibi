#include "command_buffer.hpp"
#include "raw_buffer.hpp"
#include "context.hpp"
#include "image.hpp"

namespace aryibi::renderer {
	RawBuffer make_raw_buffer(const RawBuffer::CreateInfo& info) {
        vk::BufferCreateInfo buffer_create_info{}; {
            buffer_create_info.size = info.capacity;
            buffer_create_info.queueFamilyIndexCount = 1;
            buffer_create_info.pQueueFamilyIndices = &context().device.family;
            buffer_create_info.usage = info.flags;
            buffer_create_info.sharingMode = vk::SharingMode::eExclusive;
        }

        VmaAllocationCreateInfo allocation_create_info{}; {
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocation_create_info.requiredFlags = 0;
            allocation_create_info.preferredFlags = 0;
            allocation_create_info.memoryTypeBits = 0;
            allocation_create_info.pool = nullptr;
            allocation_create_info.pUserData = nullptr;
            allocation_create_info.usage = info.usage;
        }

        RawBuffer buffer{}; {
            buffer.capacity = info.capacity;
            buffer.usage = info.usage;
            buffer.flags = info.flags;
        }

        vmaCreateBuffer(
            context().allocator,
            reinterpret_cast<VkBufferCreateInfo*>(&buffer_create_info),
            &allocation_create_info,
            reinterpret_cast<VkBuffer*>(&buffer.handle),
            &buffer.allocation,
            nullptr);

        if (info.usage != VMA_MEMORY_USAGE_GPU_ONLY) {
            vmaMapMemory(context().allocator, buffer.allocation, &buffer.mapped);
        }

        return buffer;
    }

    void copy_data_to_local(const void* data, const usize size, const RawBuffer& dest) {
        RawBuffer::CreateInfo staging_info{}; {
            staging_info.flags = vk::BufferUsageFlagBits::eTransferSrc;
            staging_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            staging_info.capacity = size;
        }
        auto staging = make_raw_buffer(staging_info);

        std::memcpy(staging.mapped, data, size);

        vk::BufferCopy copy{}; {
            copy.size = size;
            copy.srcOffset = 0;
            copy.dstOffset = 0;
        }

        auto command_buffer = begin_transient(); {
            command_buffer.copyBuffer(staging.handle, dest.handle, copy);
        } end_transient(command_buffer);

        destroy_raw_buffer(staging);
    }

    void copy_data_to_local(const void* data, const usize size, const Image& dest) {
        RawBuffer::CreateInfo staging_info{}; {
            staging_info.flags = vk::BufferUsageFlagBits::eTransferSrc;
            staging_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            staging_info.capacity = size;
        }
        auto staging = make_raw_buffer(staging_info);

        std::memcpy(staging.mapped, data, size);

        transition_image_layout(dest.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, dest.mips);

        auto command_buffer = begin_transient(); {
            vk::BufferImageCopy region{}; {
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;

                region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;

                region.imageOffset = { { 0, 0, 0 } };
                region.imageExtent = { {
                    dest.width,
                    dest.height,
                    1
                } };
            }

            command_buffer.copyBufferToImage(staging.handle, dest.handle, vk::ImageLayout::eTransferDstOptimal, region);

            end_transient(command_buffer);
        }

        destroy_raw_buffer(staging);
    }

    void destroy_raw_buffer(RawBuffer& buffer) {
        if (buffer.mapped) {
            vmaUnmapMemory(context().allocator, buffer.allocation);
            buffer.mapped = nullptr;
            buffer.handle = nullptr;
        }

        vmaDestroyBuffer(context().allocator, buffer.handle, buffer.allocation);
    }
} // namespace aryibi::renderer