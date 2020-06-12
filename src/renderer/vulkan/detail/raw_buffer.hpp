#ifndef ARYIBI_VULKAN_RAW_BUFFER_HPP
#define ARYIBI_VULKAN_RAW_BUFFER_HPP

#include "forwards.hpp"
#include "types.hpp"

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace aryibi::renderer {
    struct RawBuffer {
        struct CreateInfo {
            vk::BufferUsageFlags flags{};
            VmaMemoryUsage usage{};
            usize capacity{};
        };

        vk::BufferUsageFlags flags{};
        VmaMemoryUsage usage{};
        VmaAllocation allocation{};
        void* mapped{};
        usize capacity{};
        vk::Buffer handle{};
    };

    [[nodiscard]] RawBuffer make_raw_buffer(const RawBuffer::CreateInfo& info);
    void copy_data_to_local(const void* data, const usize size, const RawBuffer& dest);
    void copy_data_to_local(const void* data, const usize size, const Image& dest);
    void destroy_raw_buffer(RawBuffer& buffer);
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_RAW_BUFFER_HPP
