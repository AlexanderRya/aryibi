#ifndef ARYIBI_VULKAN_MESH_HPP
#define ARYIBI_VULKAN_MESH_HPP

#include "raw_buffer.hpp"
#include "types.hpp"

#include <vector>

namespace aryibi::renderer {
    struct Mesh {
        RawBuffer vbo{};
        usize vertex_count{};
    };

    [[nodiscard]] Mesh make_mesh(const std::vector<f32>& vertices);
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_MESH_HPP
