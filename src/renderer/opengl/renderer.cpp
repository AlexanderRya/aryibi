/* clang-format off */
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>
/* clang-format on */

// This implementation uses GLFW.
#include "windowing/glfw/impl_types.hpp"

#include "aryibi/renderer.hpp"
#include "aryibi/windowing.hpp"
#include "renderer/opengl/impl_types.hpp"

#include <anton/math/matrix4.hpp>
#include <anton/math/vector4.hpp>
#include <anton/math/transform.hpp>
#include <anton/math/vector2.hpp>
#include "util/aryibi_assert.hpp"

#include <iostream>
#include <memory>
#include <cstring> // For memcpy

namespace aml = anton::math;

namespace {

static void debug_callback(GLenum const source,
                           GLenum const type,
                           GLuint,
                           GLenum const severity,
                           GLsizei,
                           GLchar const* const message,
                           void const*) {
    auto stringify_source = [](GLenum const source) {
      switch (source) {
          case GL_DEBUG_SOURCE_API: return u8"API";
          case GL_DEBUG_SOURCE_APPLICATION: return u8"Application";
          case GL_DEBUG_SOURCE_SHADER_COMPILER: return u8"Shader Compiler";
          case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return u8"Window System";
          case GL_DEBUG_SOURCE_THIRD_PARTY: return u8"Third Party";
          case GL_DEBUG_SOURCE_OTHER: return u8"Other";
          default: return "";
      }
    };

    auto stringify_type = [](GLenum const type) {
      switch (type) {
          case GL_DEBUG_TYPE_ERROR: return u8"Error";
          case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return u8"Deprecated Behavior";
          case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return u8"Undefined Behavior";
          case GL_DEBUG_TYPE_PORTABILITY: return u8"Portability";
          case GL_DEBUG_TYPE_PERFORMANCE: return u8"Performance";
          case GL_DEBUG_TYPE_MARKER: return u8"Marker";
          case GL_DEBUG_TYPE_PUSH_GROUP: return u8"Push Group";
          case GL_DEBUG_TYPE_POP_GROUP: return u8"Pop Group";
          case GL_DEBUG_TYPE_OTHER: return u8"Other";
          default: return "";
      }
    };

    auto stringify_severity = [](GLenum const severity) {
      switch (severity) {
          case GL_DEBUG_SEVERITY_HIGH: return u8"Fatal Error";
          case GL_DEBUG_SEVERITY_MEDIUM: return u8"Error";
          case GL_DEBUG_SEVERITY_LOW: return u8"Warning";
          case GL_DEBUG_SEVERITY_NOTIFICATION: return u8"Note";
          default: return "";
      }
    };

    // Do not send bloat information
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    ARYIBI_LOG(("[" + std::string(stringify_severity(severity)) + ":" + stringify_type(type) + " in "
                   + stringify_source(source) + "]: " + message).c_str());

    ARYIBI_ASSERT(severity != GL_DEBUG_SEVERITY_HIGH, "OpenGL Internal Fatal Error!");
}

}

namespace aryibi::renderer {

Renderer::~Renderer() {
    glfwTerminate();
}

Renderer::Renderer(windowing::WindowHandle _w) : window(_w), p_impl(std::make_unique<impl>()) {
    ARYIBI_ASSERT(_w.exists(), "Window handle given to renderer isn't valid!");

    // Activate VSync and fix FPS
    glfwMakeContextCurrent(window.p_impl->handle);
    glfwSwapInterval(0); // TODO: Change to 1 to enable VSync
    gladLoadGLLoader((GLADloadproc)&glfwGetProcAddress);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEBUG_OUTPUT);
    glCullFace(GL_FRONT_AND_BACK);

    glDebugMessageCallback(debug_callback, nullptr);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    constexpr const char* glsl_version = "#version 450";
    ImGui_ImplGlfw_InitForOpenGL(window.p_impl->handle, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    p_impl->lit_shader =
        ShaderHandle::from_file("assets/opengl/shaded_tile.vert", "assets/opengl/shaded_tile.frag");
    p_impl->unlit_shader =
        ShaderHandle::from_file("assets/opengl/basic_tile.vert", "assets/opengl/basic_tile.frag");
    p_impl->lit_pal_shader =
        ShaderHandle::from_file("assets/opengl/shaded_pal_tile.vert", "assets/opengl/shaded_pal_tile.frag");

    p_impl->depth_shader = ShaderHandle::from_file("assets/opengl/depth.vert", "assets/opengl/depth.frag");

    TextureHandle tex;
    constexpr u32 default_shadow_res_width = 1024;
    constexpr u32 default_shadow_res_height = 1024;
    tex.init(default_shadow_res_width, default_shadow_res_height, TextureHandle::ColorType::depth,
             TextureHandle::FilteringMethod::point);
    p_impl->shadow_depth_fb = Framebuffer(tex);

    constexpr u64 lights_ubo_aligned_size = 2768;
    glGenBuffers(1, &p_impl->lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, p_impl->lights_ubo);
    glBufferData(GL_UNIFORM_BUFFER, lights_ubo_aligned_size, nullptr, GL_DYNAMIC_DRAW);

    p_impl->window_framebuffer.p_impl->handle = 0;
}

ShaderHandle Renderer::lit_shader() const { return p_impl->lit_shader; }
ShaderHandle Renderer::unlit_shader() const { return p_impl->unlit_shader; }
ShaderHandle Renderer::lit_paletted_shader() const { return p_impl->lit_pal_shader; }

void Renderer::start_frame(Color clear_color) {
    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // This is faster than actually querying the framebuffer size since imgui already does it.
    const auto& io = ImGui::GetIO();
    glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    glClearColor(clear_color.fred(), clear_color.fgreen(), clear_color.fblue(),
                 clear_color.falpha());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::finish_frame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window.p_impl->handle);
}

Framebuffer Renderer::get_window_framebuffer() {
    int display_w, display_h;
    glfwGetFramebufferSize(window.p_impl->handle, &display_w, &display_h);
    TextureHandle virtual_window_tex;
    virtual_window_tex.p_impl->width = display_w;
    virtual_window_tex.p_impl->height = display_h;
    virtual_window_tex.p_impl->filter = TextureHandle::FilteringMethod::point;
    virtual_window_tex.p_impl->color_type = TextureHandle::ColorType::rgba;
    p_impl->window_framebuffer.p_impl->tex = virtual_window_tex;
    p_impl->window_framebuffer.p_impl->handle = 0;
    return p_impl->window_framebuffer;
}

void Renderer::draw(DrawCmdList const& draw_commands, Framebuffer const& output_fb) {
    aml::Vector2 camera_view_size_in_tiles{
        (float)output_fb.texture().width() / draw_commands.camera.unit_size,
        (float)output_fb.texture().height() / draw_commands.camera.unit_size};
    // Position the camera. Since this is right-handed, the camera will look at -Z, which is exactly
    // what we want. The camera should be placed at +Z and looking at -Z so that objects that have
    // higher Z are closer to the camera.
    aml::Matrix4 view = aml::inverse(aml::translate(draw_commands.camera.position));
    aml::Matrix4 proj;
    if (draw_commands.camera.center_view) {
        proj = aml::orthographic_rh(
            -camera_view_size_in_tiles.x / 2.f, camera_view_size_in_tiles.x / 2.f,
            -camera_view_size_in_tiles.y / 2.f, camera_view_size_in_tiles.y / 2.f, 0.0f, 20.0f);
    } else {
        proj = aml::orthographic_rh(0, camera_view_size_in_tiles.x, -camera_view_size_in_tiles.y, 0,
                                    0.0f, 20.0f);
    }
    aml::Matrix4 point_light_proj = aml::perspective_rh(aml::pi / 3.f * 2.f, 1, 1.f, 10.f);

    /// The light depth texture is divided into NxN tiles, each one representing a light.
    /// This constant represents N.
    const int light_atlas_tiles = aml::ceil(
        aml::sqrt(draw_commands.directional_lights.size() + draw_commands.point_lights.size()));

    /// Update light UBO data
    glBindBuffer(GL_UNIFORM_BUFFER, p_impl->lights_ubo);
    const u32 directional_lights_count = draw_commands.directional_lights.size();
    glBufferSubData(GL_UNIFORM_BUFFER, 480, 4, &directional_lights_count);

    const u32 point_lights_count = draw_commands.point_lights.size();
    glBufferSubData(GL_UNIFORM_BUFFER, 2736, 4, &point_lights_count);
    constexpr u64 directional_light_size_aligned = 96;
    u64 directional_light_i = 0;
    ARYIBI_ASSERT(draw_commands.directional_lights.size() <= 5,
                  "Maximum directional light count (5) surpassed!");
    for (const auto& directional_light : draw_commands.directional_lights) {
        aml::Vector4 color{directional_light.color.fred(), directional_light.color.fgreen(),
                           directional_light.color.fblue(), directional_light.intensity};
        static_assert(sizeof(aml::Vector4) == 16);
        glBufferSubData(GL_UNIFORM_BUFFER, directional_light_i * directional_light_size_aligned + 0,
                        16, &color.r);

        // To create the light view, we position the light as if it were a camera and
        // then invert the matrix.
        aml::Matrix4 lightView = aml::translate(draw_commands.camera.position);
        // We want the light to be rotated on the Z and X axis to make it seem there's
        // some directionality to it.
        // We rotate the light 180º in the Y axis so that it faces -X (The scene).
        lightView *= aml::rotate_z(directional_light.rotation.z) *
                     aml::rotate_y(directional_light.rotation.y) *
                     aml::rotate_x(directional_light.rotation.x);
        lightView = aml::inverse(lightView);
        directional_light.matrix = proj * lightView;
        static_assert(sizeof(aml::Matrix4) == 64);
        static_assert(sizeof(aml::Vector2) == 8);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        directional_light_i * directional_light_size_aligned + 16, 64,
                        directional_light.matrix.get_raw());
        directional_light.light_atlas_pos = {
            (float)(directional_light_i % light_atlas_tiles) / (float)light_atlas_tiles,
            (float)(directional_light_i / light_atlas_tiles) / (float)light_atlas_tiles};
        directional_light.light_atlas_size = 1.f / (float)light_atlas_tiles;
        glBufferSubData(GL_UNIFORM_BUFFER,
                        directional_light_i * directional_light_size_aligned + 80, 8,
                        &directional_light.light_atlas_pos.x);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        directional_light_i * directional_light_size_aligned + 88, 4,
                        &directional_light.light_atlas_size);
        ++directional_light_i;
    }

    ARYIBI_ASSERT(draw_commands.directional_lights.size() <= 20,
                  "Maximum point light count (20) surpassed!");
    constexpr u64 point_light_size_aligned = 112;
    constexpr u64 point_lights_offset = 496;
    u64 point_light_i = 0;
    for (const auto& point_light : draw_commands.point_lights) {
        aml::Vector4 color{point_light.color.fred(), point_light.color.fgreen(),
                           point_light.color.fblue(), point_light.intensity};
        static_assert(sizeof(aml::Vector4) == 16);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        point_lights_offset + point_light_i * point_light_size_aligned + 0, 16,
                        &color.r);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        point_lights_offset + point_light_i * point_light_size_aligned + 16, 4,
                        &point_light.radius);

        // To create the light view, we position the light as if it were a camera and
        // then invert the matrix.
        // Point lights directly look at the scene, with no rotation because it's already looking at -Z (towards the scene).
        aml::Matrix4 lightView =
            aml::translate(point_light.position);
        lightView = aml::inverse(lightView);
        point_light.matrix = point_light_proj * lightView;
        static_assert(sizeof(aml::Matrix4) == 64);
        static_assert(sizeof(aml::Vector2) == 8);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        point_lights_offset + point_light_i * point_light_size_aligned + 32, 64,
                        point_light.matrix.get_raw());
        point_light.light_atlas_pos = {
            (float)((directional_lights_count + point_light_i) % light_atlas_tiles) /
                (float)light_atlas_tiles,
            (float)((directional_lights_count + point_light_i) / light_atlas_tiles) /
                (float)light_atlas_tiles};
        point_light.light_atlas_size = 1.f / (float)light_atlas_tiles;
        glBufferSubData(GL_UNIFORM_BUFFER,
                        point_lights_offset + point_light_i * point_light_size_aligned + 96, 8,
                        &point_light.light_atlas_pos.x);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        point_lights_offset + point_light_i * point_light_size_aligned + 104, 4,
                        &point_light.light_atlas_size);
        ++point_light_i;
    }
    aml::Vector3 ambient_light_color{draw_commands.ambient_light_color.fred(),
                                     draw_commands.ambient_light_color.fgreen(),
                                     draw_commands.ambient_light_color.fblue()};
    glBufferSubData(GL_UNIFORM_BUFFER, 2752, 12, &ambient_light_color.x);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(p_impl->depth_shader.p_impl->handle);
    glBindFramebuffer(GL_FRAMEBUFFER, p_impl->shadow_depth_fb.p_impl->handle);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LEQUAL);
    glActiveTexture(GL_TEXTURE0);
    int light_index = 0;
    for (const auto& directional_light : draw_commands.directional_lights) {
        static const auto light_atlas_pos_location =
            glGetUniformLocation(p_impl->depth_shader.p_impl->handle, "light_atlas_pos");
        static const auto light_atlas_size_location =
            glGetUniformLocation(p_impl->depth_shader.p_impl->handle, "light_atlas_size");
        glViewport(directional_light.light_atlas_pos.x * p_impl->shadow_depth_fb.texture().width(),
                   directional_light.light_atlas_pos.y * p_impl->shadow_depth_fb.texture().height(),
                   directional_light.light_atlas_size * p_impl->shadow_depth_fb.texture().width(),
                   directional_light.light_atlas_size * p_impl->shadow_depth_fb.texture().height());
        glUniformMatrix4fv(3, 1, GL_FALSE, directional_light.matrix.get_raw()); // Light view matrix
        for (const auto& cmd : draw_commands.commands) {
            if (!cmd.cast_shadows)
                continue;
            aml::Matrix4 model = aml::translate(cmd.transform.position);

            glBindVertexArray(cmd.mesh.p_impl->vao);
            glBindTexture(GL_TEXTURE_2D, cmd.texture.p_impl->handle);
            glUniformMatrix4fv(0, 1, GL_FALSE, model.get_raw()); // Model matrix
            glDrawArrays(GL_TRIANGLES, 0, cmd.mesh.p_impl->vertex_count);

            ++light_index;
        }
    }
    for (const auto& point_light : draw_commands.point_lights) {
        static const auto light_atlas_pos_location =
            glGetUniformLocation(p_impl->depth_shader.p_impl->handle, "light_atlas_pos");
        static const auto light_atlas_size_location =
            glGetUniformLocation(p_impl->depth_shader.p_impl->handle, "light_atlas_size");
        glViewport(point_light.light_atlas_pos.x * p_impl->shadow_depth_fb.texture().width(),
                   point_light.light_atlas_pos.y * p_impl->shadow_depth_fb.texture().height(),
                   point_light.light_atlas_size * p_impl->shadow_depth_fb.texture().width(),
                   point_light.light_atlas_size * p_impl->shadow_depth_fb.texture().height());
        glUniformMatrix4fv(3, 1, GL_FALSE, point_light.matrix.get_raw()); // Light view matrix
        for (const auto& cmd : draw_commands.commands) {
            if (!cmd.cast_shadows)
                continue;
            aml::Matrix4 model = aml::translate(cmd.transform.position);

            glBindVertexArray(cmd.mesh.p_impl->vao);
            glBindTexture(GL_TEXTURE_2D, cmd.texture.p_impl->handle);
            glUniformMatrix4fv(0, 1, GL_FALSE, model.get_raw()); // Model matrix
            glDrawArrays(GL_TRIANGLES, 0, cmd.mesh.p_impl->vertex_count);

            ++light_index;
        }
    }

    glViewport(0, 0, output_fb.texture().width(), output_fb.texture().height());
    glBindFramebuffer(GL_FRAMEBUFFER, output_fb.p_impl->handle);
    for (const auto& cmd : draw_commands.commands) {
        bool is_lit = cmd.shader.p_impl->shadow_tex_location != static_cast<u32>(-1);
        bool is_paletted = cmd.shader.p_impl->palette_tex_location != static_cast<u32>(-1);
        aml::Matrix4 model = aml::translate(cmd.transform.position);

        glUseProgram(cmd.shader.p_impl->handle);
        glUniformMatrix4fv(0, 1, GL_FALSE, model.get_raw()); // Model matrix
        glUniformMatrix4fv(1, 1, GL_FALSE, proj.get_raw());  // Projection matrix
        glUniformMatrix4fv(2, 1, GL_FALSE, view.get_raw());  // View matrix
        glBindVertexArray(cmd.mesh.p_impl->vao);

        glUniform1i(cmd.shader.p_impl->tile_tex_location,
                    0); // Set tile sampler2D to GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cmd.texture.p_impl->handle);
        glBindBufferBase(GL_UNIFORM_BUFFER, 5, p_impl->lights_ubo);
        if (is_lit) {
            glUniform1i(cmd.shader.p_impl->shadow_tex_location,
                        1); // Set shadow sampler2D to GL_TEXTURE1
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, p_impl->shadow_depth_fb.texture().p_impl->handle);
        }
        if (is_paletted) {
            glUniform1i(cmd.shader.p_impl->palette_tex_location,
                        2); // Set palette sampler2D to GL_TEXTURE2
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, p_impl->palette_texture.p_impl->handle);
        }

        glDrawArrays(GL_TRIANGLES, 0, cmd.mesh.p_impl->vertex_count);
    }
}

void Renderer::clear(Framebuffer& fb, aml::Vector4 color) {
    glBindFramebuffer(GL_FRAMEBUFFER, fb.p_impl->handle);
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::set_palette(ColorPalette const& palette) {
    p_impl->palette_texture.unload();
    // The palette texture is a regular 2D texture. The X axis represents the
    // different shades of a color, and the Y axis represents the different colors
    // available. We are going to dedicate 0,0 and its row entirely just for the
    // transparent color, So we'll add 1 to the height.
    const u32 palette_texture_height = palette.colors.size() + 1;
    // The width of the palette texture will be equal to the maximum shades
    // available of any color.
    u32 palette_texture_width = 0;
    for (const auto& color : palette.colors) {
        if (color.shades.size() > palette_texture_width)
            palette_texture_width = color.shades.size();
    }
    assert(palette_texture_height > 0 && palette_texture_width > 0);

    constexpr int bytes_per_pixel = 4;
    auto palette_texture_data =
        new unsigned char[palette_texture_width * palette_texture_height * bytes_per_pixel];
    std::memcpy((void*)(palette_texture_data), (void*)&palette.transparent_color, sizeof(u32));
    int px_y = 1, px_x = 0;
    for (const auto& color : palette.colors) {
        for (const auto& shade : color.shades) {
            std::memcpy((void*)(palette_texture_data +
                                (px_x + px_y * palette_texture_width) * bytes_per_pixel),
                        (void*)&shade, sizeof(u32));
            ++px_x;
        }
        px_x = 0;
        ++px_y;
    }
    p_impl->palette_texture.init(palette_texture_width, palette_texture_height,
                                 TextureHandle::ColorType::rgba,
                                 TextureHandle::FilteringMethod::point, palette_texture_data);
    delete[] palette_texture_data;
}

void Renderer::set_shadow_resolution(u32 width, u32 height) {
    p_impl->shadow_depth_fb.resize(width, height);
}

aml::Vector2 Renderer::get_shadow_resolution() const {
    return {(float)p_impl->shadow_depth_fb.texture().width(),
            (float)p_impl->shadow_depth_fb.texture().height()};
}

} // namespace aryibi::renderer