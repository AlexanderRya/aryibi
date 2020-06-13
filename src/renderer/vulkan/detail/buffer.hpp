#ifndef ARYIBI_VULKAN_BUFFER_HPP
#define ARYIBI_VULKAN_BUFFER_HPP

#include "raw_buffer.hpp"
#include "constants.hpp"
#include "forwards.hpp"
#include "types.hpp"

#include <array>

namespace aryibi::renderer {
    class SingleBuffer {
        RawBuffer buffer{};
        usize buf_size{};

        void allocate(const vk::BufferUsageFlags flags, const usize size);
        void reallocate(const usize size);
    public:
        void create(const vk::BufferUsageFlags flags);
        void resize(const usize size);
        void write(const void* data, const usize size);
        void destroy();

        [[nodiscard]] bool exists() const;
        [[nodiscard]] void* buf() const;
        [[nodiscard]] usize size() const;
        [[nodiscard]] vk::Buffer handle() const;
        [[nodiscard]] vk::DescriptorBufferInfo info() const;
    };

    class Buffer {
        std::array<SingleBuffer, meta::max_in_flight> buffers{};
    public:
        void create(const vk::BufferUsageFlags flags);
        void resize(const usize size);
        void write(const void* data, const usize size);
        void destroy();

        [[nodiscard]] SingleBuffer& operator [](const usize idx);
        [[nodiscard]] const SingleBuffer& operator [](const usize idx) const;
        [[nodiscard]] std::array<vk::DescriptorBufferInfo, meta::max_in_flight> info() const;
    };
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_BUFFER_HPP
