/* clang-format off */
#include <glad/glad.h>
#include <GLFW/glfw3.h>
/* clang-format on */

#include "aryibi/renderer.hpp"
#include "renderer/opengl/impl_types.hpp"
#include "aryibi/sprites.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <anton/math/transform.hpp>
#include <anton/math/math.hpp>
#include <anton/math/vector4.hpp>
#include "util/aryibi_assert.hpp"

#include <memory>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace aml = anton::math;

namespace aryibi::renderer {

TextureHandle::TextureHandle() : p_impl(std::make_unique<impl>()) {}
TextureHandle::~TextureHandle() {
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    if (p_impl->handle == 0 || glfwGetCurrentContext() == nullptr)
        return;
    ARYIBI_ASSERT(impl::handle_ref_count[p_impl->handle] != 1,
                  "All handles to a texture were destroyed without unloading them first!!");
    impl::handle_ref_count[p_impl->handle]--;
#endif
}
TextureHandle::TextureHandle(TextureHandle const& other) : p_impl(std::make_unique<impl>()) {
    *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->handle]++;
#endif
}
TextureHandle& TextureHandle::operator=(TextureHandle const& other) {
    if (this != &other) {
        *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle]++;
#endif
    }
    return *this;
}

ImTextureID TextureHandle::imgui_id() const {
    ARYIBI_ASSERT(exists(), "Called imgui_id() with a texture that doesn't exist!");
    return reinterpret_cast<void*>(p_impl->handle);
}
bool TextureHandle::exists() const { return p_impl->handle != 0; }
u32 TextureHandle::width() const { return p_impl->width; }
u32 TextureHandle::height() const { return p_impl->height; }
TextureHandle::ColorType TextureHandle::color_type() const { return p_impl->color_type; }
TextureHandle::FilteringMethod TextureHandle::filter() const { return p_impl->filter; }

bool operator==(TextureHandle const& a, TextureHandle const& b) {
    return a.p_impl->handle == b.p_impl->handle;
}

void TextureHandle::init(
    u32 width, u32 height, ColorType type, FilteringMethod filter, const void* data) {
    ARYIBI_ASSERT(!exists(), "Called init(...) without calling unload() first!");
    glGenTextures(1, (u32*)&p_impl->handle);
    glBindTexture(GL_TEXTURE_2D, p_impl->handle);

    p_impl->width = width;
    p_impl->height = height;
    p_impl->color_type = type;
    p_impl->filter = filter;
    switch (type) {
        case (ColorType::rgba):
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         data);
            break;
        case (ColorType::indexed_palette):
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, data);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
            break;
        case (ColorType::depth):
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, data);
            break;
        default: ARYIBI_ASSERT(false, "Unknown ColorType! (Implementation not finished?)");
    }
    switch (filter) {
        case FilteringMethod::point:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case FilteringMethod::linear:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
            break;
        default: ARYIBI_ASSERT(false, "Unknown FilteringMethod! (Implementation not finished?)");
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border_color[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->handle] = 1;
#endif
}
TextureHandle
TextureHandle::from_file_rgba(fs::path const& path, FilteringMethod filter, bool flip) {
    stbi_set_flip_vertically_on_load(flip);
    int w, h, channels;
    TextureHandle tex;
    unsigned char* data = stbi_load(path.generic_string().c_str(), &w, &h, &channels, 4);

    if (!data)
        // Return empty handle if something went wrong
        return tex;

    tex.init(w, h, ColorType::rgba, filter, data);

    stbi_image_free(data);
    return tex;
}

TextureHandle TextureHandle::from_file_indexed(fs::path const& path,
                                               ColorPalette const& palette,
                                               FilteringMethod filter,
                                               bool flip) {
    stbi_set_flip_vertically_on_load(flip);
    int w, h, channels;
    unsigned char* original_data = stbi_load(path.generic_string().c_str(), &w, &h, &channels, 4);
    constexpr int original_bytes_per_pixel = 4;
    /// Indexed only has two channels: Red (color) and green (shade)
    constexpr int indexed_bytes_per_pixel = 2;
    auto indexed_data = new unsigned char[w * h * indexed_bytes_per_pixel];
    for (u32 x = 0; x < w; ++x) {
        for (u32 y = 0; y < h; ++y) {
            struct {
                u8 color_index;
                u8 shade_index;
            } closest_color;
            float closest_color_distance = 99999999.f;

            Color raw_original_color;
            std::memcpy(&raw_original_color.hex_val,
                        original_data + (x + y * w) * original_bytes_per_pixel, sizeof(u32));
            if (raw_original_color.alpha() == 0) {
                // Transparent color
                closest_color = {0, 0};
            } else {
                for (u8 color = 0; color < palette.colors.size(); ++color) {
                    for (u8 shade = 0; shade < palette.colors[color].shades.size(); ++shade) {
                        aml::Vector4 this_color{
                            static_cast<float>(palette.colors[color].shades[shade].red()),
                            static_cast<float>(palette.colors[color].shades[shade].green()),
                            static_cast<float>(palette.colors[color].shades[shade].blue()),
                            static_cast<float>(palette.colors[color].shades[shade].alpha())};
                        aml::Vector4 original_color{static_cast<float>(raw_original_color.red()),
                                                    static_cast<float>(raw_original_color.green()),
                                                    static_cast<float>(raw_original_color.blue()),
                                                    static_cast<float>(raw_original_color.alpha())};
                        float color_distance = aml::length(this_color - original_color);
                        if (closest_color_distance > color_distance) {
                            closest_color_distance = color_distance;
                            // Add one to the color and shade because 0,0 is the transparent color
                            closest_color = {static_cast<u8>(color + 1u),
                                             static_cast<u8>(shade + 1u)};
                        }
                    }
                }
            }

            std::memcpy(indexed_data + (x + y * w) * indexed_bytes_per_pixel,
                        &closest_color.shade_index, sizeof(u8));
            std::memcpy(indexed_data + (x + y * w) * indexed_bytes_per_pixel + sizeof(u8),
                        &closest_color.color_index, sizeof(u8));
        }
    }
    TextureHandle tex;

    tex.init(w, h, ColorType::indexed_palette, filter, indexed_data);

    stbi_image_free(original_data);
    delete[] indexed_data;
    return tex;
}

void TextureHandle::unload() {
    // glDeleteTextures ignores 0s (not created textures)
    glDeleteTextures(1, &p_impl->handle);

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->handle] = 0;
#endif

    p_impl->handle = 0;
}

MeshHandle::MeshHandle() : p_impl(std::make_unique<impl>()) {}
MeshHandle::~MeshHandle() {
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    if (p_impl->vao == 0 || glfwGetCurrentContext() == nullptr)
        return;
    ARYIBI_ASSERT(impl::handle_ref_count[p_impl->vao] != 1,
                  "All handles to a mesh were destroyed without unloading them first!!");
    impl::handle_ref_count[p_impl->vao]--;
#endif
}
MeshHandle::MeshHandle(MeshHandle const& other) : p_impl(std::make_unique<impl>()) {
    *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->vao]++;
#endif
}
MeshHandle& MeshHandle::operator=(MeshHandle const& other) {
    if (this != &other) {
        *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->vao]++;
#endif
    }
    return *this;
}

bool MeshHandle::exists() const {
    ARYIBI_ASSERT((bool)(p_impl->vao) ^ (bool)(p_impl->vbo),
                  "[Internal error] Only VAO or VBO exist, but not both at once?");
    return p_impl->vao;
}
void MeshHandle::unload() {
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->vao] = 0;
#endif
    // Zeros (non-existent meshes) are silently ignored
    glDeleteVertexArrays(1, &p_impl->vao);
    p_impl->vao = 0;
    glDeleteBuffers(1, &p_impl->vbo);
    p_impl->vbo = 0;
}

ShaderHandle::ShaderHandle() : p_impl(std::make_unique<impl>()) {}
ShaderHandle::~ShaderHandle() = default;
ShaderHandle::ShaderHandle(ShaderHandle const& other) : p_impl(std::make_unique<impl>()) {
    *p_impl = *other.p_impl;
}
ShaderHandle& ShaderHandle::operator=(ShaderHandle const& other) {
    if (this != &other) {
        *p_impl = *other.p_impl;
    }
    return *this;
}

bool ShaderHandle::exists() const { return p_impl->handle; }
void ShaderHandle::unload() {
    glDeleteProgram(p_impl->handle);
    p_impl->handle = 0;
}

static unsigned int create_shader_stage(GLenum stage, fs::path const& path) {
    using namespace std::literals::string_literals;

    std::ifstream f(path);
    if (!f.good()) {
        throw std::runtime_error("Failed to open file: "s + path.generic_string());
    }
    std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    char* src = source.data();

    unsigned int shader = glCreateShader(stage);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infolog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infolog);
        throw std::runtime_error("Failed to compile shader:\n"s + source + "\nReason: "s + infolog);
    }

    return shader;
}

ShaderHandle ShaderHandle::from_file(fs::path const& vert_path, fs::path const& frag_path) {
    using namespace std::literals::string_literals;

    unsigned int vtx = create_shader_stage(GL_VERTEX_SHADER, vert_path);
    unsigned int frag = create_shader_stage(GL_FRAGMENT_SHADER, frag_path);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vtx);
    glAttachShader(prog, frag);

    glLinkProgram(prog);
    int success;
    char infolog[512];
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(prog, 512, nullptr, infolog);
        throw std::runtime_error("Failed to link shader.\nReason: "s + infolog);
    }

    glDeleteShader(vtx);
    glDeleteShader(frag);

    ShaderHandle shader;
    shader.p_impl->handle = prog;
    shader.p_impl->tile_tex_location = glGetUniformLocation(prog, "tile");
    shader.p_impl->shadow_tex_location = glGetUniformLocation(prog, "shadow");
    shader.p_impl->palette_tex_location = glGetUniformLocation(prog, "palette");
    return shader;
}

MeshBuilder::MeshBuilder() : p_impl(std::make_unique<impl>()) { p_impl->result.reserve(256); }
MeshBuilder::~MeshBuilder() = default;
MeshBuilder::MeshBuilder(MeshBuilder const& other) : p_impl(std::make_unique<impl>()) {
    *p_impl = *other.p_impl;
}
MeshBuilder& MeshBuilder::operator=(MeshBuilder const& other) {
    if (this != &other) {
        *p_impl = *other.p_impl;
    }
    return *this;
}

void MeshBuilder::add_sprite(sprites::Sprite const& spr,
                             aml::Vector3 offset,
                             float vertical_slope,
                             float horizontal_slope,
                             float z_min,
                             float z_max) {
    const auto add_piece = [&](sprites::Sprite::Piece const& piece, std::size_t base_n) {
        auto& result = p_impl->result;
        const sprites::Rect2D pos_rect{
            {piece.destination.start.x + offset.x, piece.destination.start.y + offset.y},
            {piece.destination.end.x + offset.x, piece.destination.end.y + offset.y}};
        const sprites::Rect2D uv_rect = piece.source;
        const sprites::Rect2D z_map{
            {aml::clamp(piece.destination.start.x * horizontal_slope, z_min, z_max),
             aml::clamp(piece.destination.start.y * vertical_slope, z_min, z_max)},
            {aml::clamp(piece.destination.end.x * horizontal_slope, z_min, z_max),
             aml::clamp(piece.destination.end.y * vertical_slope, z_min, z_max)}};

        // First triangle //
        // First triangle //
        /* X pos 1st vertex */ result[base_n + 0] = pos_rect.start.x;
        /* Y pos 1st vertex */ result[base_n + 1] = pos_rect.start.y;
        /* Z pos 1st vertex */ result[base_n + 2] = z_map.start.x + z_map.start.y + offset.z;
        /* X UV 1st vertex  */ result[base_n + 3] = uv_rect.start.x;
        /* Y UV 1st vertex  */ result[base_n + 4] = uv_rect.end.y;
        /* X pos 2nd vertex */ result[base_n + 5] = pos_rect.end.x;
        /* Y pos 2nd vertex */ result[base_n + 6] = pos_rect.start.y;
        /* Z pos 2nd vertex */ result[base_n + 7] = z_map.end.x + z_map.start.y + offset.z;
        /* X UV 2nd vertex  */ result[base_n + 8] = uv_rect.end.x;
        /* Y UV 2nd vertex  */ result[base_n + 9] = uv_rect.end.y;
        /* X pos 3rd vertex */ result[base_n + 10] = pos_rect.start.x;
        /* Y pos 3rd vertex */ result[base_n + 11] = pos_rect.end.y;
        /* Z pos 3rd vertex */ result[base_n + 12] = z_map.start.x + z_map.end.y + offset.z;
        /* X UV 2nd vertex  */ result[base_n + 13] = uv_rect.start.x;
        /* Y UV 2nd vertex  */ result[base_n + 14] = uv_rect.start.y;

        // Second triangle //
        /* X pos 1st vertex */ result[base_n + 15] = pos_rect.end.x;
        /* Y pos 1st vertex */ result[base_n + 16] = pos_rect.start.y;
        /* Z pos 1st vertex */ result[base_n + 17] = z_map.end.x + z_map.start.y + offset.z;
        /* X UV 1st vertex  */ result[base_n + 18] = uv_rect.end.x;
        /* Y UV 1st vertex  */ result[base_n + 19] = uv_rect.end.y;
        /* X pos 2nd vertex */ result[base_n + 20] = pos_rect.end.x;
        /* Y pos 2nd vertex */ result[base_n + 21] = pos_rect.end.y;
        /* Z pos 2nd vertex */ result[base_n + 22] = z_map.end.x + z_map.end.y + offset.z;
        /* X UV 2nd vertex  */ result[base_n + 23] = uv_rect.end.x;
        /* Y UV 2nd vertex  */ result[base_n + 24] = uv_rect.start.y;
        /* X pos 3rd vertex */ result[base_n + 25] = pos_rect.start.x;
        /* Y pos 3rd vertex */ result[base_n + 26] = pos_rect.end.y;
        /* Z pos 3rd vertex */ result[base_n + 27] = z_map.start.x + z_map.end.y + offset.z;
        /* X UV 3rd vertex  */ result[base_n + 28] = uv_rect.start.x;
        /* Y UV 3rd vertex  */ result[base_n + 29] = uv_rect.start.y;
    };

    const auto prev_size = p_impl->result.size();
    p_impl->result.resize(prev_size + spr.pieces.size() * impl::sizeof_quad);
    for (std::size_t i = 0; i < spr.pieces.size(); ++i) {
        add_piece(spr.pieces[i], prev_size + i * impl::sizeof_quad);
    }
}

MeshHandle MeshBuilder::finish() const {
    MeshHandle mesh;
    glGenVertexArrays(1, &mesh.p_impl->vao);
    glGenBuffers(1, &mesh.p_impl->vbo);

    // Fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, mesh.p_impl->vbo);
    glBufferData(GL_ARRAY_BUFFER, p_impl->result.size() * sizeof(float), p_impl->result.data(),
                 GL_STATIC_DRAW);

    glBindVertexArray(mesh.p_impl->vao);
    // Vertex Positions
    glEnableVertexAttribArray(0); // location 0
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glBindVertexBuffer(0, mesh.p_impl->vbo, 0, impl::sizeof_vertex * sizeof(float));
    glVertexAttribBinding(0, 0);
    // UV Positions
    glEnableVertexAttribArray(1); // location 1
    glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glBindVertexBuffer(1, mesh.p_impl->vbo, 0, impl::sizeof_vertex * sizeof(float));
    glVertexAttribBinding(1, 1);

    mesh.p_impl->vertex_count = p_impl->result.size() / impl::sizeof_vertex;

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    MeshHandle::impl::handle_ref_count[mesh.p_impl->vao] = 1;
#endif
    p_impl->result.clear();
    return mesh;
}

Framebuffer::Framebuffer() : p_impl(std::make_unique<impl>()) {
    p_impl->handle = static_cast<unsigned int>(-1);
}
Framebuffer::Framebuffer(TextureHandle const& texture) : p_impl(std::make_unique<impl>()) {
    p_impl->create_handle();
    p_impl->tex = texture;
    p_impl->bind_texture();
}

Framebuffer::Framebuffer(Framebuffer const& other) : p_impl(std::make_unique<impl>()) {
    p_impl->handle = other.p_impl->handle;
    p_impl->tex = other.p_impl->tex;

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->handle]++;
#endif
}

Framebuffer::~Framebuffer() {
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    if (p_impl->handle == -1 || p_impl->handle == 0 || glfwGetCurrentContext() == nullptr)
        return;
    ARYIBI_ASSERT(impl::handle_ref_count[p_impl->handle] != 1,
                  "All handles to a framebuffer were destroyed without unloading them first!!");
    impl::handle_ref_count[p_impl->handle]--;
#endif
}

Framebuffer& Framebuffer::operator=(Framebuffer const& other) {
    if (&other == this)
        return *this;

    p_impl->handle = other.p_impl->handle;
    p_impl->tex = other.p_impl->tex;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[p_impl->handle]++;
#endif

    return *this;
}

void Framebuffer::impl::create_handle() {
    if (exists()) {
        glDeleteFramebuffers(1, &handle);
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[handle]--;
#endif
    }
    glCreateFramebuffers(1, &handle);
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
    impl::handle_ref_count[handle] = 1;
#endif
}

void Framebuffer::impl::bind_texture() {
    ARYIBI_ASSERT(exists(),
                  "[Internal error] Called impl::bind_texture with non-existent framebuffer?");
    glBindFramebuffer(GL_FRAMEBUFFER, handle);
    switch (tex.color_type()) {
        case TextureHandle::ColorType::rgba:
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   tex.p_impl->handle, 0);
            break;
        case TextureHandle::ColorType::depth:
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                   tex.p_impl->handle, 0);
            break;
        default: assert(false && "Unknown color type"); return;
    }
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool Framebuffer::impl::exists() const { return handle != static_cast<u32>(-1); }

bool Framebuffer::exists() const { return p_impl->exists(); }

void Framebuffer::resize(u32 width, u32 height) {
    ARYIBI_ASSERT(exists(), "Tried to resize non-existent framebuffer!");
    auto& tex = p_impl->tex;
    const auto prev_color_type = tex.color_type();
    const auto prev_filter_type = tex.filter();
    tex.unload();
    tex.init(width, height, prev_color_type, prev_filter_type);
    p_impl->bind_texture();
}

TextureHandle const& Framebuffer::texture() const { return p_impl->tex; }

void Framebuffer::unload() {
    if (glfwGetCurrentContext() == nullptr)
        return;
    p_impl->tex.unload();
    if (p_impl->handle != static_cast<unsigned int>(-1)) {
        glDeleteFramebuffers(1, &p_impl->handle);
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle] = 0;
#endif
    }
}

} // namespace aryibi::renderer

namespace std {

std::size_t
hash<aryibi::renderer::TextureHandle>::operator()(aryibi::renderer::TextureHandle const& tex) const
    noexcept {
    return reinterpret_cast<std::size_t>(tex.imgui_id());
}

} // namespace std