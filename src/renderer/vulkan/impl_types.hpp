#ifndef ARYIBI_VULKAN_IMPL_TYPES_HPP
#define ARYIBI_VULKAN_IMPL_TYPES_HPP

#include "detail/descriptor_set.hpp"
#include "detail/render_pass.hpp"
#include "detail/raw_buffer.hpp"
#include "detail/swapchain.hpp"
#include "detail/pipeline.hpp"
#include "detail/forwards.hpp"
#include "detail/buffer.hpp"
#include "detail/image.hpp"
#include "detail/mesh.hpp"

#include "aryibi/renderer.hpp"

#include <vector>

namespace aryibi::renderer {
    struct TextureHandle::impl {
        Image image;
        vk::Sampler sampler;
        ColorType color_type;
        FilteringMethod filter;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
    };

    struct MeshHandle::impl {
        Mesh handle;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
    };

    struct MeshBuilder::impl {
        std::vector<f32> result;

        static constexpr auto sizeof_vertex = 5;
        static constexpr auto sizeof_triangle = 3 * sizeof_vertex;
        static constexpr auto sizeof_quad = 2 * sizeof_triangle;
    };

    struct Framebuffer::impl {
        TextureHandle texture;

        void create_handle();
        void bind_texture();
        [[nodiscard]] bool exists() const;

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
    };

    struct ShaderHandle::impl {
        Pipeline handle;
    };

    struct Renderer::impl {
        Swapchain swapchain;
        RenderPass depth_pass;
        RenderPass color_pass;

        std::vector<vk::Semaphore> image_available{};
        std::vector<vk::Semaphore> render_finished{};
        std::vector<vk::Fence> in_flight{};

        std::vector<vk::CommandBuffer> command_buffers{};

        vk::DescriptorSetLayout main_layout{};
        vk::DescriptorSetLayout palette_depth{};
        vk::DescriptorSetLayout texture_layout{};
        vk::DescriptorSetLayout lights_layout{};

        Pipeline basic_tile_shader;
        Pipeline depth_shader;
        Pipeline shaded_pal_shader;
        Pipeline shaded_tile_shader;

        DescriptorSet main_set{};

        Buffer uniform_data{};
        Buffer transforms{};
    };
} // namespace aryibi::renderer

#endif //ARYIBI_VULKAN_IMPL_TYPES_HPP
