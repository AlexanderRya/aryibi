#ifndef ARYIBI_VULKAN_MESH_HPP
#define ARYIBI_VULKAN_MESH_HPP

#include "raw_buffer.hpp"
#include "types.hpp"

#include <vector>

namespace aryibi::renderer {
    struct Mesh {
        RawBuffer vbo{};
        RawBuffer ibo{};
        usize vertex_count{};
        usize index_count{};
    };

    [[nodiscard]] Mesh make_mesh(const std::vector<f32>& vertices);
    [[nodiscard]] Mesh make_mesh(const std::vector<f32>& vertices, const std::vector<u32>& indices);
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_MESH_HPP
