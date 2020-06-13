#include "command_buffer.hpp"
#include "context.hpp"

#include <vulkan/vulkan.hpp>

namespace aryibi::renderer {
    static vk::CommandPool main;
    static vk::CommandPool transient;

    void make_command_pools() {
        /* Main */ {
            vk::CommandPoolCreateInfo create_info{}; {
                create_info.queueFamilyIndex = context().device.family;
                create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
            }
            main = context().device.logical.createCommandPool(create_info);
        }

        /* Transient */ {
            vk::CommandPoolCreateInfo create_info{}; {
                create_info.queueFamilyIndex = context().device.family;
                create_info.flags =
                    vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                    vk::CommandPoolCreateFlagBits::eTransient;
            }
            transient = context().device.logical.createCommandPool(create_info);
        }
    }

    std::vector<vk::CommandBuffer> make_command_buffers(const usize size) {
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{}; {
            command_buffer_allocate_info.commandBufferCount = size;
            command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
            command_buffer_allocate_info.commandPool = main;
        }
        return context().device.logical.allocateCommandBuffers(command_buffer_allocate_info);
    }

    vk::CommandBuffer begin_transient() {
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{}; {
            command_buffer_allocate_info.commandBufferCount = 1;
            command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
            command_buffer_allocate_info.commandPool = transient;
        }

        auto command_buffers = context().device.logical.allocateCommandBuffers(command_buffer_allocate_info).back();

        vk::CommandBufferBeginInfo begin_info{}; {
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        }

        command_buffers.begin(begin_info);

        return command_buffers;
    }

    void end_transient(const vk::CommandBuffer command_buffer) {
        command_buffer.end();

        vk::SubmitInfo submit_info{}; {
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
        }

        context().device.graphics.submit(submit_info, nullptr);
        context().device.graphics.waitIdle();
        context().device.logical.freeCommandBuffers(transient, command_buffer);
    }
} // namespace aryibi::renderer