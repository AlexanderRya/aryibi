#ifndef ARYIBI_VULKAN_IMPL_TYPES_HPP
#define ARYIBI_VULKAN_IMPL_TYPES_HPP

#include "detail/descriptor_set.hpp"
#include "detail/render_pass.hpp"
#include "detail/raw_buffer.hpp"
#include "detail/swapchain.hpp"
#include "detail/pipeline.hpp"
#include "detail/dyn_mesh.hpp"
#include "detail/forwards.hpp"
#include "detail/texture.hpp"
#include "detail/buffer.hpp"
#include "detail/image.hpp"
#include "detail/mesh.hpp"

#include "aryibi/renderer.hpp"

#include <vector>

namespace aryibi::renderer {
    struct TextureHandle::impl {
        u64 handle = -1;
        u32 width = 0;
        u32 height = 0;
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
        static void enqueue_for_deletion(RawBuffer& buffer);
        static void write_dyn_mesh(DynMesh& mesh, const std::vector<f32>& vertices);
        [[nodiscard]] static usize load_texture(const u8* data, const TextureHandle& handle);
        void update_buffers(const DrawCmdList&);

        Swapchain swapchain{};
        RenderPass depth_pass{};
        RenderPass color_pass{};
        RenderPass imgui_pass{};

        Texture palette_texture{};

        std::vector<vk::Semaphore> image_available{};
        std::vector<vk::Semaphore> render_finished{};
        std::vector<vk::Fence> in_flight{};

        std::vector<vk::CommandBuffer> command_buffers{};

        vk::DescriptorSetLayout main_layout{};
        vk::DescriptorSetLayout palette_depth_layout{};
        vk::DescriptorSetLayout lights_layout{};

        Pipeline basic_tile_shader{};
        Pipeline depth_shader{};
        Pipeline shaded_pal_shader{};
        Pipeline shaded_tile_shader{};

        DescriptorSet main_set{};
        DescriptorSet palette_depth_set{};
        DescriptorSet lights_set{};

        Buffer uniform_data{};
        Buffer transforms{};
        Buffer light_mats{};
        Buffer lights_data{};
    };
} // namespace aryibi::renderer

#endif //ARYIBI_VULKAN_IMPL_TYPES_HPP
