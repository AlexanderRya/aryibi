#ifndef ARYIBI_VULKAN_COMMAND_BUFFER_HPP
#define ARYIBI_VULKAN_COMMAND_BUFFER_HPP

#include "forwards.hpp"
#include "types.hpp"

#include <vector>

namespace aryibi::renderer {
    void make_command_pools();

    [[nodiscard]] std::vector<vk::CommandBuffer> make_command_buffers(const usize size);
    [[nodiscard]] vk::CommandBuffer begin_transient();
    void end_transient(const vk::CommandBuffer command_buffer);
} // namespace aryibi::renderer

#endif //ARYIBI_VULKAN_COMMAND_BUFFER_HPP
