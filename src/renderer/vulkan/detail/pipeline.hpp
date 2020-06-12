#ifndef ARYIBI_VULKAN_PIPELINE_HPP
#define ARYIBI_VULKAN_PIPELINE_HPP

#include "types.hpp"

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>

namespace aryibi::renderer {
    struct Vertex {
        f32 pos[3];
        f32 uvs[2];
    };

    struct Pipeline {
        struct CreateInfo {
            std::string vertex{};
            std::string fragment{};

            std::vector<vk::DescriptorSetLayout> layouts{};
            vk::PushConstantRange push_constants{};
            vk::RenderPass render_pass{};
            vk::SampleCountFlagBits samples{};
            vk::CullModeFlagBits cull{};
            bool depth{};
            std::vector<vk::DynamicState> dynamic_states{};
        };

        vk::Pipeline handle{};
        vk::PipelineLayout layout{};

        operator vk::Pipeline() const {
            return handle;
        }

        operator vk::PipelineLayout() const {
            return layout;
        }

        void destroy();
    };

    [[nodiscard]] Pipeline make_pipeline(const Pipeline::CreateInfo& info);
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_PIPELINE_HPP
