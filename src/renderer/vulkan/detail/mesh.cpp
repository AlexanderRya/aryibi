#include "pipeline.hpp"
#include "mesh.hpp"

namespace aryibi::renderer {
    Mesh make_mesh(const std::vector<f32>& vertices) {
        Mesh mesh{};

        RawBuffer::CreateInfo vertex_info{}; {
            vertex_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vertex_info.flags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
            vertex_info.capacity = vertices.size() * sizeof(f32);
        }
        mesh.vbo = make_raw_buffer(vertex_info);
        copy_data_to_local(vertices.data(), vertices.size() * sizeof(f32), mesh.vbo);
        mesh.vertex_count = vertices.size() * sizeof(f32) / sizeof(Vertex);

        return mesh;
    }

    [[nodiscard]] Mesh make_mesh(const std::vector<f32>& vertices, const std::vector<u32>& indices) {
        Mesh mesh{};

        RawBuffer::CreateInfo vertex_info{}; {
            vertex_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vertex_info.flags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
            vertex_info.capacity = vertices.size() * sizeof(f32);
        }

        RawBuffer::CreateInfo index_info{}; {
            index_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            index_info.flags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
            index_info.capacity = indices.size() * sizeof(u32);
        }

        mesh.vbo = make_raw_buffer(vertex_info);
        mesh.ibo = make_raw_buffer(index_info);

        copy_data_to_local(vertices.data(), vertices.size() * sizeof(f32), mesh.vbo);
        copy_data_to_local(indices.data(), indices.size() * sizeof(u32), mesh.ibo);

        mesh.vertex_count = vertices.size() * sizeof(f32) / sizeof(Vertex);
        mesh.index_count = indices.size();

        return mesh;
    }
} // namespace triton::vulkan