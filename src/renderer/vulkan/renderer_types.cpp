#include "renderer/vulkan/impl_types.hpp"
#include "util/aryibi_assert.hpp"
#include "aryibi/renderer.hpp"
#include "aryibi/sprites.hpp"

#include "detail/sampler.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <anton/math/transform.hpp>
#include <anton/math/vector4.hpp>
#include <anton/math/math.hpp>

#include <memory>
#include <cstring>
#include <fstream>
#include <filesystem>

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

    TextureHandle::TextureHandle(const TextureHandle& other) : p_impl(std::make_unique<impl>()) {
        *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle]++;
#endif
    }

    TextureHandle& TextureHandle::operator =(TextureHandle const& other) {
        if (this != &other) {
            *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
            impl::handle_ref_count[p_impl->handle]++;
#endif
        }
        return *this;
    }

    void TextureHandle::init(u32 width, u32 height, ColorType type, FilteringMethod filter, const void* data) {
        ARYIBI_ASSERT(!exists(), "Called init(...) without calling unload() first!");

        p_impl->color_type = type;
        p_impl->filter = filter;
        p_impl->width = width;
        p_impl->height = height;

        switch (filter) {
            case FilteringMethod::point: {
                p_impl->sampler = point_sampler();
            } break;

            case FilteringMethod::linear: {
                p_impl->sampler = linear_sampler();
            } break;

            default: ARYIBI_ASSERT(false, "Unknown FilteringMethod! (Implementation not finished?)");
        }

        switch (type) {
            case ColorType::rgba: {
                p_impl->handle = Renderer::impl::load_texture(reinterpret_cast<const u8*>(data), *this);
            } break;

            case ColorType::indexed_palette: {
                ARYIBI_ASSERT(false, "Not implemented");
            } break;

            case ColorType::depth: {
                // no-op
            } break;

            default: ARYIBI_ASSERT(false, "Unknown ColorType! (Implementation not finished?)");
        }

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle] = 1;
#endif
    }

    void TextureHandle::unload() {
        if (!exists()) {
            return;
        }


        p_impl->handle = -1;
    }

    bool TextureHandle::exists() const {
        return p_impl->handle != -1;
    }

    u32 TextureHandle::width() const {
        return p_impl->width;
    }

    u32 TextureHandle::height() const {
        return p_impl->height;
    }

    TextureHandle::ColorType TextureHandle::color_type() const {
        return p_impl->color_type;
    }

    TextureHandle::FilteringMethod TextureHandle::filter() const {
        return p_impl->filter;
    }

    ImTextureID TextureHandle::imgui_id() const {
        ARYIBI_ASSERT(exists(), "Called imgui_id() with a texture that doesn't exist!");
        return nullptr;
    }

    TextureHandle TextureHandle::from_file_rgba(const fs::path& path, FilteringMethod filter, bool flip) {
        stbi_set_flip_vertically_on_load(flip);
        i32 width, height, channels = 4;
        TextureHandle texture;
        u8* data = stbi_load(path.generic_string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            return texture;
        }

        texture.init(width, height, ColorType::rgba, filter, data);

        stbi_image_free(data);
        return texture;
    }

    TextureHandle TextureHandle::from_file_indexed(const std::filesystem::path& path,
                                                   const ColorPalette& palette,
                                                   FilteringMethod filter,
                                                   bool flip) {
        stbi_set_flip_vertically_on_load(flip);
        i32 width, height, channels = 4;
        u8* original_data = stbi_load(path.generic_string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        constexpr i32 original_bytes_per_pixel = 4;
        /// Indexed only has two channels: Red (color) and green (shade)
        constexpr i32 indexed_bytes_per_pixel = 2;
        auto indexed_data = new u8[width * height * indexed_bytes_per_pixel];
        for (u32 x = 0; x < width; ++x) {
            for (u32 y = 0; y < height; ++y) {
                struct {
                    u8 color_index;
                    u8 shade_index;
                } closest_color{};
                float closest_color_distance = 99999999.f;

                Color raw_original_color;
                std::memcpy(&raw_original_color.hex_val,
                            original_data + (x + y * width) * original_bytes_per_pixel, sizeof(u32));
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

                std::memcpy(indexed_data + (x + y * width) * indexed_bytes_per_pixel,
                            &closest_color.shade_index, sizeof(u8));
                std::memcpy(indexed_data + (x + y * width) * indexed_bytes_per_pixel + sizeof(u8),
                            &closest_color.color_index, sizeof(u8));
            }
        }
        TextureHandle tex{};

        tex.init(width, height, ColorType::indexed_palette, filter, indexed_data);

        stbi_image_free(original_data);
        delete[] indexed_data;
        return tex;
    }

    bool operator ==(TextureHandle const& a, TextureHandle const& b) {
        return a.p_impl->handle == b.p_impl->handle;
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

    MeshHandle::MeshHandle(const MeshHandle& other) : p_impl(std::make_unique<impl>()) {
        *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->vao]++;
#endif
    }

    MeshHandle& MeshHandle::operator =(const MeshHandle& other) {
        if (this != &other) {
            *p_impl = *other.p_impl;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
            impl::handle_ref_count[p_impl->vao]++;
#endif
        }
        return *this;
    }

    bool MeshHandle::exists() const {
        return p_impl->handle.vbo.handle;
    }

    void MeshHandle::unload() {
        Renderer::impl::enqueue_for_deletion(p_impl->handle.vbo);
    }


    ShaderHandle::ShaderHandle() : p_impl(std::make_unique<impl>()) {}

    ShaderHandle::~ShaderHandle() = default;

    ShaderHandle::ShaderHandle(const ShaderHandle& other) : p_impl(std::make_unique<impl>()) {
        *p_impl = *other.p_impl;
    }

    ShaderHandle& ShaderHandle::operator =(const ShaderHandle& other) {
        if (this != &other) {
            *p_impl = *other.p_impl;
        }
        return *this;
    }

    bool ShaderHandle::exists() const {
        return static_cast<vk::Pipeline>(p_impl->handle);
    }

    void ShaderHandle::unload() {
        p_impl->handle.destroy();
    }

    ShaderHandle ShaderHandle::from_file(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) {
        // Dis be a problem.
        return {};
    }


    MeshBuilder::MeshBuilder() : p_impl(std::make_unique<impl>()) {
        p_impl->result.reserve(256);
    }

    MeshBuilder::~MeshBuilder() = default;

    MeshBuilder::MeshBuilder(const MeshBuilder& other) : p_impl(std::make_unique<impl>()) {
        *p_impl = *other.p_impl;
    }

    MeshBuilder& MeshBuilder::operator =(const MeshBuilder& other) {
        if (this != &other) {
            *p_impl = *other.p_impl;
        }
        return *this;
    }

    void MeshBuilder::add_sprite(const sprites::Sprite& spr,
                                 anton::math::Vector3 offset,
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
        MeshHandle mesh{};
        mesh.p_impl->handle = make_mesh(p_impl->result);

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        MeshHandle::impl::handle_ref_count[mesh.p_impl->vao] = 1;
#endif

        p_impl->result.clear();
        return mesh;
    }


    Framebuffer::Framebuffer() : p_impl(std::make_unique<impl>()) {

    }

    Framebuffer::Framebuffer(const TextureHandle& texture) : p_impl(std::make_unique<impl>()) {
        p_impl->create_handle();
        p_impl->texture = texture;
        p_impl->bind_texture();
    }

    Framebuffer::Framebuffer(const Framebuffer& other) : p_impl(std::make_unique<impl>()) {
        p_impl->texture = other.p_impl->texture;

#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle]++;
#endif
    }

    Framebuffer::~Framebuffer() {
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        if (p_impl->handle == -1 || p_impl->handle == 0 || glfwGetCurrentContext() == nullptr {
            return;
        }
    ARYIBI_ASSERT(impl::handle_ref_count[p_impl->handle] != 1,
                  "All handles to a framebuffer were destroyed without unloading them first!!");
    impl::handle_ref_count[p_impl->handle]--;
#endif
    }

    Framebuffer& Framebuffer::operator=(const Framebuffer& other) {
        if (&other == this) {
            return *this;
        }

        p_impl->texture = other.p_impl->texture;
#ifdef ARYIBI_DETECT_RENDERER_LEAKS
        impl::handle_ref_count[p_impl->handle]++;
#endif

        return *this;
    }

    bool Framebuffer::exists() const {
        return p_impl->texture.p_impl->handle;
    }

    void Framebuffer::unload() {
        p_impl->texture.unload();
    }

    void Framebuffer::resize(u32 width, u32 height) {
        if (width == p_impl->texture.width() && height == p_impl->texture.height()) {
            return;
        }

        const auto color_type_backup = p_impl->texture.color_type();
        const auto filter_backup = p_impl->texture.filter();

        p_impl->texture.unload();
        p_impl->texture.init(width, height, color_type_backup, filter_backup, nullptr);
        p_impl->bind_texture();
    }

    void Framebuffer::impl::create_handle() {
        // no-op
    }

    void Framebuffer::impl::bind_texture() {
        // no-op
    }

    bool Framebuffer::impl::exists() const {
        return texture.exists();
    }
} // namespace aryibi::renderer