#ifndef ARYIBI_VULKAN_FORWARDS_HPP
#define ARYIBI_VULKAN_FORWARDS_HPP

#include <cstdint>

struct GLFWwindow;
namespace vk {
    class Instance;
    class DebugUtilsMessengerEXT;
    class SurfaceKHR;
    class DescriptorPool;
    class CommandPool;
    class CommandBuffer;
    class RenderPass;
    class Framebuffer;
    class Fence;
    class Semaphore;
    class Sampler;
    class Pipeline;
    enum class Format;
    enum class BufferUsageFlagBits : std::uint32_t;
    template <typename>
    class Flags;
    using BufferUsageFlags = Flags<BufferUsageFlagBits>;
} // namespace vk

namespace aryibi::renderer {
    struct Texture;
    struct Swapchain;
    struct RawBuffer;
    struct Image;
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_FORWARDS_HPP