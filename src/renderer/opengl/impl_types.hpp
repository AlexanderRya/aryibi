#ifndef ARYIBI_OPENGL_IMPL_TYPES_HPP
#define ARYIBI_OPENGL_IMPL_TYPES_HPP

#include "aryibi/renderer.hpp"

#include <vector>

namespace aryibi::renderer {

struct TextureHandle::impl {
    u32 width;
    u32 height;
    ColorType color_type;
    FilteringMethod filter;
    u32 handle = 0;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
};

struct MeshHandle::impl {
    u32 vbo = 0;
    u32 vao = 0;
    u32 vertex_count;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
};

struct ShaderHandle::impl {
    u32 handle = 0;
    u32 tile_tex_location = -1;
    u32 shadow_tex_location = -1;
    u32 palette_tex_location = -1;
};

struct MeshBuilder::impl {
    std::vector<float> result;

    static constexpr auto sizeof_vertex = 5;
    static constexpr auto sizeof_triangle = 3 * sizeof_vertex;
    static constexpr auto sizeof_quad = 2 * sizeof_triangle;
};

struct Framebuffer::impl {
    unsigned int handle = -1;
    TextureHandle tex;
    [[nodiscard]] bool exists() const;
    void create_handle();
    void bind_texture();

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    static inline std::unordered_map<u32, u32> handle_ref_count;
#endif
};

struct Renderer::impl {
    ShaderHandle lit_pal_shader;
    ShaderHandle lit_shader;
    ShaderHandle unlit_shader;
    ShaderHandle depth_shader;
    Framebuffer shadow_depth_fb;
    TextureHandle palette_texture;

    Framebuffer window_framebuffer;

    unsigned int lights_ubo;
};

}

#endif // ARYIBI_OPENGL_IMPL_TYPES_HPP
