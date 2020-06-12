#include "pipeline.hpp"
#include "context.hpp"

#include <vulkan/vulkan.hpp>

#include <fstream>
#include <vector>
#include <string>

namespace aryibi::renderer {
    [[nodiscard]] static vk::ShaderModule load_module(const std::string& path) {
        std::ifstream in(path, std::fstream::binary);

        if (!in.is_open()) {
            throw std::runtime_error("Error, \"" + path + "\" file not found.");
        }

        std::string spv{ std::istreambuf_iterator<c8>{ in }, {} };

        vk::ShaderModuleCreateInfo create_info{}; {
            create_info.codeSize = spv.size();
            create_info.pCode = reinterpret_cast<const u32*>(spv.data());
        }

        auto module = context().device.logical.createShaderModule(create_info);

        return module;
    }

    void Pipeline::destroy() {
        context().device.logical.destroyPipelineLayout(layout);
        context().device.logical.destroyPipeline(handle);
    }

    Pipeline make_pipeline(const Pipeline::CreateInfo& info) {
        Pipeline pipeline{};

        vk::PipelineLayoutCreateInfo layout_create_info{}; {
            if (info.push_constants.size != 0) {
                layout_create_info.pushConstantRangeCount = 1;
                layout_create_info.pPushConstantRanges = &info.push_constants;
            }
            if (!info.layouts.empty()) {
                layout_create_info.setLayoutCount = info.layouts.size();
                layout_create_info.pSetLayouts = info.layouts.data();
            }
        }
        pipeline.layout = context().device.logical.createPipelineLayout(layout_create_info);

        std::array<vk::ShaderModule, 2> modules{}; {
            modules[0] = load_module(info.vertex);
            modules[1] = load_module(info.fragment);
        }

        std::array<vk::PipelineShaderStageCreateInfo, 2> stages{}; {
            stages[0].pName = "main";
            stages[0].module = modules[0];
            stages[0].stage = vk::ShaderStageFlagBits::eVertex;

            stages[1].pName = "main";
            stages[1].module = modules[1];
            stages[1].stage = vk::ShaderStageFlagBits::eFragment;
        }

        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{}; {
            dynamic_state_create_info.dynamicStateCount = info.dynamic_states.size();
            dynamic_state_create_info.pDynamicStates = info.dynamic_states.data();
        }

        vk::VertexInputBindingDescription vertex_binding{}; {
            vertex_binding.binding = 0;
            vertex_binding.stride = sizeof(Vertex);
            vertex_binding.inputRate = vk::VertexInputRate::eVertex;
        }

        std::array<vk::VertexInputAttributeDescription, 2> vertex_attributes{}; {
            vertex_attributes[0].binding = 0;
            vertex_attributes[0].format = vk::Format::eR32G32B32Sfloat;
            vertex_attributes[0].location = 0;
            vertex_attributes[0].offset = offsetof(Vertex, pos);

            vertex_attributes[1].binding = 0;
            vertex_attributes[1].format = vk::Format::eR32G32Sfloat;
            vertex_attributes[1].location = 1;
            vertex_attributes[1].offset = offsetof(Vertex, uvs);
        }

        vk::PipelineVertexInputStateCreateInfo vertex_input_info{}; {
            vertex_input_info.pVertexBindingDescriptions = &vertex_binding;
            vertex_input_info.vertexBindingDescriptionCount = 1;
            vertex_input_info.pVertexAttributeDescriptions = vertex_attributes.data();
            vertex_input_info.vertexAttributeDescriptionCount = vertex_attributes.size();
        }

        vk::PipelineInputAssemblyStateCreateInfo input_assembly{}; {
            input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
            input_assembly.primitiveRestartEnable = false;
        }

        vk::Viewport viewport{};
        vk::Rect2D scissor{};

        vk::PipelineViewportStateCreateInfo viewport_state{}; {
            viewport_state.viewportCount = 1;
            viewport_state.pViewports = &viewport;
            viewport_state.scissorCount = 1;
            viewport_state.pScissors = &scissor;
        }

        vk::PipelineRasterizationStateCreateInfo rasterizer_state_info{}; {
            rasterizer_state_info.lineWidth = 1.0f;
            rasterizer_state_info.depthBiasEnable = false;
            rasterizer_state_info.depthBiasSlopeFactor = 0.0f;
            rasterizer_state_info.depthBiasConstantFactor = 0.0f;
            rasterizer_state_info.depthClampEnable = false;
            rasterizer_state_info.rasterizerDiscardEnable = false;
            rasterizer_state_info.polygonMode = vk::PolygonMode::eFill;
            rasterizer_state_info.cullMode = info.cull;
            rasterizer_state_info.frontFace = vk::FrontFace::eClockwise;
        }

        if (info.samples > samples) {
            throw std::runtime_error("Invalid number of samples requested in pipeline creation.");
        }

        vk::PipelineMultisampleStateCreateInfo multisampling_state_info{}; {
            multisampling_state_info.alphaToCoverageEnable = false;
            multisampling_state_info.sampleShadingEnable = true;
            multisampling_state_info.alphaToOneEnable = false;
            multisampling_state_info.rasterizationSamples = info.samples;
            multisampling_state_info.minSampleShading = 0.2f;
            multisampling_state_info.pSampleMask = nullptr;
        }

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{}; {
            depth_stencil_info.stencilTestEnable = false;
            depth_stencil_info.depthTestEnable = info.depth;
            depth_stencil_info.depthWriteEnable = info.depth;
            depth_stencil_info.depthCompareOp = vk::CompareOp::eLessOrEqual;
            depth_stencil_info.depthBoundsTestEnable = false;
            depth_stencil_info.minDepthBounds = 0.0f;
            depth_stencil_info.maxDepthBounds = 1.0f;
            depth_stencil_info.stencilTestEnable = false;
            depth_stencil_info.front = vk::StencilOpState{};
            depth_stencil_info.back = vk::StencilOpState{};
        }

        vk::PipelineColorBlendAttachmentState color_blend_attachment{}; {
            color_blend_attachment.blendEnable = true;
            color_blend_attachment.colorWriteMask =
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA;
            color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
            color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
            color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
        }

        vk::PipelineColorBlendStateCreateInfo color_blend_info{}; {
            color_blend_info.attachmentCount = 1;
            color_blend_info.pAttachments = &color_blend_attachment;
            color_blend_info.logicOp = vk::LogicOp::eCopy;
            color_blend_info.logicOpEnable = false;
            color_blend_info.blendConstants[0] = 0.0f;
            color_blend_info.blendConstants[1] = 0.0f;
            color_blend_info.blendConstants[2] = 0.0f;
            color_blend_info.blendConstants[3] = 0.0f;
        }

        vk::GraphicsPipelineCreateInfo pipeline_info{}; {
            pipeline_info.stageCount = stages.size();
            pipeline_info.pStages = stages.data();
            pipeline_info.pVertexInputState = &vertex_input_info;
            pipeline_info.pInputAssemblyState = &input_assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &rasterizer_state_info;
            pipeline_info.pMultisampleState = &multisampling_state_info;
            pipeline_info.pDepthStencilState = &depth_stencil_info;
            pipeline_info.pColorBlendState = &color_blend_info;
            pipeline_info.pDynamicState = &dynamic_state_create_info;
            pipeline_info.layout = pipeline.layout;
            pipeline_info.renderPass = info.render_pass;
            pipeline_info.subpass = 0;
            pipeline_info.basePipelineHandle = nullptr;
            pipeline_info.basePipelineIndex = -1;
        }

        pipeline.handle = context().device.logical.createGraphicsPipeline(nullptr, pipeline_info).value;

        for (const auto& module : modules) {
            context().device.logical.destroyShaderModule(module);
        }

        return pipeline;
    }
} // namespace aryibi::renderer