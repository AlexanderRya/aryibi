#ifndef ARBIYI_VULKAN_DYN_MESH_HPP
#define ARBIYI_VULKAN_DYN_MESH_HPP

#include "buffer.hpp"
#include "types.hpp"

namespace aryibi::renderer {
    struct DynMesh {
        Buffer vbo;
        Buffer ibo;
        usize vertex_count;
        usize index_count;
    };
} // namespace aryibi::renderer

#endif //ARBIYI_VULKAN_DYN_MESH_HPP
