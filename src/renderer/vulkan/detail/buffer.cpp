#include "context.hpp"
#include "buffer.hpp"

namespace aryibi::renderer {
    void SingleBuffer::allocate(const vk::BufferUsageFlags flags, const usize size) {
        RawBuffer::CreateInfo info{}; {
            info.capacity = size * 2;
            info.flags = flags;
            info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        }
        buffer = make_raw_buffer(info);
    }

    void SingleBuffer::reallocate(const usize size) {
        auto flags_backup = buffer.flags;

        destroy();
        allocate(flags_backup, size);
    }

    void SingleBuffer::create(const vk::BufferUsageFlags flags) {
        allocate(flags, 256);
        buf_size = 0;
    }

    void SingleBuffer::resize(const usize size) {
        if (size == buf_size) {
            return;
        }
        if (size > buffer.capacity) {
            reallocate(size);
        }

        buf_size = size;
    }

    void SingleBuffer::write(const void* data, const usize size) {
        if (size != buffer.capacity) {
            resize(size);
        }

        std::memcpy(buffer.mapped, data, size);
    }

    bool SingleBuffer::exists() const {
        return buffer.handle;
    }

    void* SingleBuffer::buf() const {
        return buffer.mapped;
    }

    usize SingleBuffer::size() const {
        return buf_size;
    }

    void SingleBuffer::destroy() {
        destroy_raw_buffer(buffer);
    }

    vk::Buffer SingleBuffer::handle() const {
        return buffer.handle;
    }

    vk::DescriptorBufferInfo SingleBuffer::info() const {
        return {
            buffer.handle,
            0,
            buf_size == 0 ? VK_WHOLE_SIZE : buf_size
        };
    }


    void Buffer::create(vk::BufferUsageFlags flags) {
        for (auto& buffer : buffers) {
            buffer.create(flags);
        }
    }

    void Buffer::resize(usize size) {
        for (auto& buffer : buffers) {
            buffer.resize(size);
        }
    }

    void Buffer::write(const void* data, usize size) {
        for (auto& buffer : buffers) {
            buffer.write(data, size);
        }
    }

    void Buffer::destroy() {
        for (auto& buffer : buffers) {
            buffer.destroy();
        }
    }

    SingleBuffer& Buffer::operator [](usize idx) {
        return buffers[idx];
    }

    const SingleBuffer& Buffer::operator [](usize idx) const {
        return buffers[idx];
    }

    std::array<vk::DescriptorBufferInfo, meta::max_in_flight> Buffer::info() const {
        std::array<vk::DescriptorBufferInfo, meta::max_in_flight> infos{};

        for (usize i = 0; i < infos.size(); ++i) {
            infos[i] = buffers[i].info();
        }

        return infos;
    }
} // namespace aryibi::renderer