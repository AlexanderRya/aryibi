#ifndef ARYIBI_RENDERER_HPP
#define ARYIBI_RENDERER_HPP

#include "windowing.hpp"

#include <anton/math/matrix4.hpp>
#include <anton/math/vector2.hpp>
#include <anton/math/vector3.hpp>
#include <anton/math/vector4.hpp>
#include <filesystem>
#include <limits>
#include <memory>
#include <vector>

struct GLFWwindow;
typedef void* ImTextureID;

namespace aryibi::sprites {
struct Sprite;
}

namespace aryibi::renderer {

using namespace anton; // For integer types

class Renderer;
struct ColorPalette;

class TextureHandle {
public:
    enum class ColorType { rgba, indexed_palette, depth };
    enum class FilteringMethod { point, linear };
    /// Doesn't actually create a texture -- If exists() is called before
    /// initializing it, it will return false. Call init() to initialize and
    /// create the texture.
    TextureHandle();
    /// The destructor will NOT unload the texture underneath. Remember to call
    /// unload() first if you want to actually destroy the texture.
    ~TextureHandle();
    TextureHandle(TextureHandle const&);
    TextureHandle& operator=(TextureHandle const&);

    /// IMPORTANT: This function will assert if called when a previous texture
    /// existed in this slot. Remember to call unload() first if you want to
    /// reload the texture with different parameters.
    void
    init(u32 width, u32 height, ColorType type, FilteringMethod filter, const void* data = nullptr);
    /// Destroys the texture underneath, or does nothing if it doesn't exist
    /// already.
    void unload();
    /// @returns True if the texture has been initialized and not unloaded.
    [[nodiscard]] bool exists() const;
    /// The width of the texture. If the texture doesn't exist, the result is
    /// implementation-defined.
    [[nodiscard]] u32 width() const;
    /// The height of the texture. If the texture doesn't exist, the result is
    /// implementation-defined.
    [[nodiscard]] u32 height() const;
    /// The color type of the texture. If the texture doesn't exist, the result is
    /// implementation-defined.
    [[nodiscard]] ColorType color_type() const;
    /// The filtering method of the texture. If the texture doesn't exist, the
    /// result is implementation-defined.
    [[nodiscard]] FilteringMethod filter() const;
    /// Returns an ImGui ID that represents the texture. If the texture doesn't
    /// exist, this function will assert.
    [[nodiscard]] ImTextureID imgui_id() const;

    /// Loads a RGBA texture from a file path, and returns a handle to it. If
    /// there were any problems loading it, the TextureHandle returned won't be
    /// initialized to a value and its exists() will return false. Must support
    /// the following formats: JPEG, PNG, TGA, BMP, PSD, GIF, HDR, PIC and PNM
    /// (Basically all the formats stb_image supports, which you can see in
    /// std_image.h)
    static TextureHandle from_file_rgba(std::filesystem::path const&,
                                        FilteringMethod filter = FilteringMethod::point,
                                        bool flip = false);
    static TextureHandle from_file_indexed(std::filesystem::path const&,
                                           ColorPalette const&,
                                           FilteringMethod filter,
                                           bool flip);

private:
    friend class Renderer;
    friend class Framebuffer;
    friend class RenderMapContext;
    friend class RenderTilesetContext;
    friend bool operator==(TextureHandle const&, TextureHandle const&);
    friend bool operator!=(TextureHandle const&, TextureHandle const&);

    struct impl;
    std::unique_ptr<impl> p_impl;
};

/// Compares the internal handle.
bool operator==(TextureHandle const&, TextureHandle const&);
inline bool operator!=(TextureHandle const& a, TextureHandle const& b) { return !(a == b); }

/// A handle to a generic framebuffer with a texture attached to it.
/// TODO: Rename to FramebufferHandle for consistency
class Framebuffer {
public:
    Framebuffer();
    explicit Framebuffer(TextureHandle const& texture);
    /// The destructor should NOT destroy the underlying framebuffer/handle. It is
    /// just put here so that the implementation links correctly.
    ~Framebuffer();
    Framebuffer(Framebuffer const&);
    Framebuffer& operator=(Framebuffer const&);

    bool exists() const;
    void unload();

    void resize(u32 width, u32 height);
    [[nodiscard]] TextureHandle const& texture() const;

private:
    friend class Renderer;
    friend class RenderMapContext;
    friend class RenderTilesetContext;

    struct impl;
    std::unique_ptr<impl> p_impl;
};

struct MeshHandle {
    /// Meshes are only meant to be created by MeshBuilder. Otherwise you won't be
    /// able to put data in them.
    MeshHandle();
    /// The MeshHandle destructor won't actually unload the underlying mesh. Use
    /// unload() for that.
    ~MeshHandle();
    MeshHandle(MeshHandle const&);
    MeshHandle& operator=(MeshHandle const&);

    /// Returns true if the texture exists and has not been unloaded.
    [[nodiscard]] bool exists() const;
    /// Destroys the underlying mesh. Does nothing if the mesh was already
    /// unloaded previously.
    void unload();

private:
    friend class MeshBuilder;
    friend class Renderer;

    struct impl;
    std::unique_ptr<impl> p_impl;
};

/// Represents a GLSL shader handle. A regular shader must have the following
/// structure: Vertex shader: in vec3 iPos; in vec2 iTexCoords; layout(location
/// = 0) uniform mat4 model; layout(location = 1) uniform mat4 projection;
/// layout(location = 2) uniform mat4 view;
/// layout(location = 3) uniform mat4 lightSpaceMatrix; // If lighting is
/// needed. Optional Fragment shader: uniform sampler2D tile;     // MUST have
/// this name uniform sampler2D shadow;   // MUST have this name, if lighting is
/// needed. Optional
struct ShaderHandle {
    /// Creates a blank shader handle. Does not really have an use outside of the
    /// renderer implementation.
    ShaderHandle();
    ~ShaderHandle();
    ShaderHandle(ShaderHandle const&);
    ShaderHandle& operator=(ShaderHandle const&);

    /// Returns true if the shader exists and has not been unloaded.
    [[nodiscard]] bool exists() const;
    /// Destroys the underlying shader. Does nothing if the shader was already
    /// unloaded previously.
    void unload();

    /// Loads a GLSL shader from two paths (One for the fragment shader and
    /// another one for the vertex one)
    static ShaderHandle from_file(std::filesystem::path const& vert_path,
                                  std::filesystem::path const& frag_path);

private:
    friend class Renderer;

    struct impl;
    std::unique_ptr<impl> p_impl;
};

/// Represents a RGBA 32-bit color.
struct Color {
    constexpr Color() : hex_val(0) {}
    constexpr Color(u32 hex_val) : hex_val(hex_val) {}
    constexpr Color(u8 red, u8 blue, u8 green, u8 alpha = 255) :
        hex_val(red | (green << 8u) | (blue << 16u) | (alpha << 24u)) {}
    constexpr Color(float red, float blue, float green, float alpha = 1) :
        hex_val((u32)(red * 255u) | ((u32)(green * 255u) << 8u) | ((u32)(blue * 255u) << 16u) |
                ((u32)(alpha * 255u) << 24u)) {}

    [[nodiscard]] constexpr u8 red() const noexcept { return hex_val & 0xFFu; }
    [[nodiscard]] constexpr float fred() const noexcept {
        return static_cast<float>(hex_val & 0xFFu) / 255.f;
    }
    [[nodiscard]] constexpr u8 green() const noexcept { return (hex_val >> 8u) & 0xFFu; }
    [[nodiscard]] constexpr float fgreen() const noexcept {
        return static_cast<float>((hex_val >> 8u) & 0xFFu) / 255.f;
    }
    [[nodiscard]] constexpr u8 blue() const noexcept { return (hex_val >> 16u) & 0xFFu; }
    [[nodiscard]] constexpr float fblue() const noexcept {
        return static_cast<float>((hex_val >> 16u) & 0xFFu) / 255.f;
    }
    [[nodiscard]] constexpr u8 alpha() const noexcept { return (hex_val >> 24u) & 0xFFu; }
    [[nodiscard]] constexpr float falpha() const noexcept {
        return static_cast<float>((hex_val >> 24u) & 0xFFu) / 255.f;
    }

    u32 hex_val;
};

namespace colors {

constexpr static Color transparent = 0x00000000;
constexpr static Color black = 0xFF000000;
constexpr static Color red = 0xFF0000FF;
constexpr static Color green = 0xFF00FF00;
constexpr static Color blue = 0xFFFF0000;
constexpr static Color white = 0xFFFFFFFF;

} // namespace colors

struct Transform {
    anton::math::Vector3 position;
};

struct Camera {
    anton::math::Vector3 position;
    /// How big is a single mesh unit on the screen.
    float unit_size = 1;
    bool center_view = true;
};

struct DrawCmd {
    TextureHandle texture;
    MeshHandle mesh;
    ShaderHandle shader;
    Transform transform;
    bool cast_shadows = false;
};

struct Light {
private:
    friend class Renderer;
    /// The position within the light atlas, in UV coordinates.
    mutable anton::math::Vector2 light_atlas_pos;
    /// The size of this light's tile in the atlas.
    mutable float light_atlas_size;
    mutable anton::math::Matrix4 matrix;
};

struct DirectionalLight : Light {
    anton::math::Vector3 rotation;
    Color color;
    float intensity;
};

struct PointLight : Light {
    anton::math::Vector3 position;
    float radius;
    Color color;
    float intensity;
};

struct DrawCmdList {
    Camera camera;
    std::vector<DrawCmd> commands;
    std::vector<DirectionalLight> directional_lights;
    std::vector<PointLight> point_lights;
    Color ambient_light_color = colors::black;
};

class MeshBuilder {
public:
    MeshBuilder();
    ~MeshBuilder();
    MeshBuilder(MeshBuilder const&);
    MeshBuilder& operator=(MeshBuilder const&);

    /// Adds a sprite to the mesh.
    /// @param spr The sprite.
    /// @param offset Where to place the sprite, in tile units.
    /// @param vertical_slope How many tile units to distort the sprite in the Z
    /// axis per Y unit.
    /// @param horizontal_slope How many tile units to distort the sprite in the Z
    /// axis per X unit.
    void add_sprite(sprites::Sprite const& spr,
                    anton::math::Vector3 offset,
                    float vertical_slope = 0,
                    float horizontal_slope = 0,
                    float z_min = std::numeric_limits<float>::min(),
                    float z_max = std::numeric_limits<float>::max());

    /// Returns a mesh with the data added until now and resets the meshbuilder's internal state.
    [[nodiscard]] MeshHandle finish() const;

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

struct ColorPalette {
    struct ColorShades {
        /// The shades of this color. First element should be the darkest one and
        /// last one should be the brightest one.
        std::vector<Color> shades;
    };

    /// The colors and shades available in this palette.
    std::vector<ColorShades> colors;
    Color transparent_color = 0;
};

class Renderer {
public:
    /// Create and initialize a renderer bound to a valid window. No more than one renderer can be
    /// bound to a single window.
    explicit Renderer(windowing::WindowHandle result);
    ~Renderer();

    void draw(DrawCmdList const& commands, Framebuffer const& output_fb);
    void clear(Framebuffer& fb, anton::math::Vector4 color);

    void set_shadow_resolution(u32 width, u32 height);
    [[nodiscard]] anton::math::Vector2 get_shadow_resolution() const;
    void set_palette(ColorPalette const&);

    // Returns the default lit shader. The handle will be valid until the renderer
    // is destroyed.
    ShaderHandle lit_shader() const;
    // Returns the paletted lit shader, that uses the renderer's set palette to
    // calculate the colors used when lighting the scene. Requires indexed textures.
    ShaderHandle lit_paletted_shader() const;
    // Returns the default unlit shader. The handle will be valid until the
    // renderer is destroyed.
    ShaderHandle unlit_shader() const;

    Framebuffer get_window_framebuffer();

    void start_frame(Color clear_color = colors::black);
    void finish_frame();

private:
    windowing::WindowHandle window;
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace aryibi::renderer

#include <functional>

namespace std {

template<> struct hash<aryibi::renderer::TextureHandle> {
    std::size_t operator()(aryibi::renderer::TextureHandle const&) const noexcept;
};
template<> struct hash<aryibi::renderer::MeshHandle> {
    std::size_t operator()(aryibi::renderer::MeshHandle const&) const noexcept;
};
template<> struct hash<aryibi::renderer::ShaderHandle> {
    std::size_t operator()(aryibi::renderer::ShaderHandle const&) const noexcept;
};

} // namespace std

#endif // ARYIBI_RENDERER_HPP
