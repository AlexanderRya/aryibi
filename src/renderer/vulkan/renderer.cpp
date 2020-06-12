#include "detail/descriptor_set.hpp"
#include "detail/command_buffer.hpp"
#include "detail/constants.hpp"
#include "detail/context.hpp"
#include "detail/buffer.hpp"

#include "windowing/glfw/impl_types.hpp"
#include "aryibi/renderer.hpp"
#include "impl_types.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

namespace aryibi::renderer {
    const static auto& ctx = context();
    static u32 image_index{};
    static u32 frame_index{};

    Renderer::Renderer(windowing::WindowHandle window)
        : window(window),
          p_impl(std::make_unique<impl>()) {
        initialise(window.p_impl->handle);

        /* Swapchain */ {
            auto capabilities = ctx.device.physical.getSurfaceCapabilitiesKHR(surface(window.p_impl->handle));

            auto image_count = capabilities.minImageCount + 1;

            if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
                image_count = capabilities.maxImageCount;
            }

            if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
                p_impl->swapchain.extent = capabilities.currentExtent;
            } else {
                p_impl->swapchain.extent = vk::Extent2D{
                    std::clamp<u32>(window.get_resolution().x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                    std::clamp<u32>(window.get_resolution().y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
                };
            }

            auto surface_formats = ctx.device.physical.getSurfaceFormatsKHR(surface(window.p_impl->handle));
            p_impl->swapchain.format = surface_formats[0];

            for (const auto& each : surface_formats) {
                if ((each.format == vk::Format::eB8G8R8A8Srgb || each.format == vk::Format::eR8G8B8A8Srgb) &&
                    each.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                    p_impl->swapchain.format = each;
                }
            }

            vk::SwapchainCreateInfoKHR swapchain_create_info{}; {
                swapchain_create_info.surface = surface(window.p_impl->handle);
                swapchain_create_info.minImageCount = image_count;
                swapchain_create_info.imageFormat = p_impl->swapchain.format.format;
                swapchain_create_info.imageColorSpace = p_impl->swapchain.format.colorSpace;
                swapchain_create_info.imageExtent = p_impl->swapchain.extent;
                swapchain_create_info.preTransform = capabilities.currentTransform;
                swapchain_create_info.imageArrayLayers = 1;
                swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
                swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
                swapchain_create_info.queueFamilyIndexCount = 1;
                swapchain_create_info.pQueueFamilyIndices = &ctx.device.family;
                swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
                swapchain_create_info.presentMode = vk::PresentModeKHR::eImmediate;
                swapchain_create_info.clipped = true;
                swapchain_create_info.oldSwapchain = nullptr;
            }

            p_impl->swapchain.handle = ctx.device.logical.createSwapchainKHR(swapchain_create_info);
            p_impl->swapchain.images = ctx.device.logical.getSwapchainImagesKHR(p_impl->swapchain.handle);

            vk::ImageViewCreateInfo image_view_create_info{}; {
                image_view_create_info.format = p_impl->swapchain.format.format;
                image_view_create_info.components.r = vk::ComponentSwizzle::eIdentity;
                image_view_create_info.components.g = vk::ComponentSwizzle::eIdentity;
                image_view_create_info.components.b = vk::ComponentSwizzle::eIdentity;
                image_view_create_info.components.a = vk::ComponentSwizzle::eIdentity;
                image_view_create_info.viewType = vk::ImageViewType::e2D;
                image_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                image_view_create_info.subresourceRange.baseMipLevel = 0;
                image_view_create_info.subresourceRange.levelCount = 1;
                image_view_create_info.subresourceRange.baseArrayLayer = 0;
                image_view_create_info.subresourceRange.layerCount = 1;
            }

            for (const auto& image : p_impl->swapchain.images) {
                image_view_create_info.image = image;
                p_impl->swapchain.views.emplace_back(ctx.device.logical.createImageView(image_view_create_info));
            }
        }

        /* Color pass */ {
            Image::CreateInfo color_create_info{}; {
                color_create_info.format = p_impl->swapchain.format.format;
                color_create_info.width = p_impl->swapchain.extent.width;
                color_create_info.height = p_impl->swapchain.extent.height;
                color_create_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
                color_create_info.tiling = vk::ImageTiling::eOptimal;
                color_create_info.aspect = vk::ImageAspectFlagBits::eColor;
                color_create_info.samples = vk::SampleCountFlagBits::e1;
                color_create_info.mips = 1;
            }

            Image::CreateInfo depth_create_info{}; {
                depth_create_info.format = vk::Format::eD32SfloatS8Uint;
                depth_create_info.width = p_impl->swapchain.extent.width;
                depth_create_info.height = p_impl->swapchain.extent.height;
                depth_create_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                depth_create_info.tiling = vk::ImageTiling::eOptimal;
                depth_create_info.aspect = vk::ImageAspectFlagBits::eDepth;
                depth_create_info.samples = vk::SampleCountFlagBits::e1;
                depth_create_info.mips = 1;
            }

            std::array<vk::AttachmentDescription, 2> attachments{}; {
                attachments[0].format = p_impl->swapchain.format.format;
                attachments[0].samples = vk::SampleCountFlagBits::e1;
                attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
                attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
                attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
                attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
                attachments[0].initialLayout = vk::ImageLayout::eUndefined;
                attachments[0].finalLayout = vk::ImageLayout::eTransferSrcOptimal;

                attachments[1].format = vk::Format::eD32SfloatS8Uint;
                attachments[1].samples = vk::SampleCountFlagBits::e1;
                attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
                attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
                attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
                attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
                attachments[1].initialLayout = vk::ImageLayout::eUndefined;
                attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            }

            vk::AttachmentReference color_attachment{}; {
                color_attachment.layout = vk::ImageLayout::eColorAttachmentOptimal;
                color_attachment.attachment = 0;
            }

            vk::AttachmentReference depth_attachment{}; {
                depth_attachment.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                depth_attachment.attachment = 1;
            }

            vk::SubpassDescription subpass_description{}; {
                subpass_description.colorAttachmentCount = 1;
                subpass_description.pColorAttachments = &color_attachment;
                subpass_description.pDepthStencilAttachment = &depth_attachment;
                subpass_description.pResolveAttachments = nullptr;
                subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            }

            vk::SubpassDependency subpass_dependency{}; {
                subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                subpass_dependency.dstSubpass = 0;
                subpass_dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                subpass_dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                subpass_dependency.srcAccessMask = {};
                subpass_dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            }

            vk::RenderPassCreateInfo create_info{}; {
                create_info.attachmentCount = attachments.size();
                create_info.pAttachments = attachments.data();
                create_info.subpassCount = 1;
                create_info.pSubpasses = &subpass_description;
                create_info.dependencyCount = 1;
                create_info.pDependencies = &subpass_dependency;
            }
            p_impl->color_pass
                .attach("color", color_create_info)
                .attach("depth", depth_create_info)
                .create(create_info);
        }

        /* Depth pass */ {
            Image::CreateInfo depth_create_info{}; {
                depth_create_info.format = vk::Format::eD16Unorm;
                depth_create_info.width = 1024;
                depth_create_info.height = 1024;
                depth_create_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
                depth_create_info.tiling = vk::ImageTiling::eOptimal;
                depth_create_info.aspect = vk::ImageAspectFlagBits::eDepth;
                depth_create_info.samples = vk::SampleCountFlagBits::e1;
                depth_create_info.mips = 1;
            }

            vk::AttachmentDescription depth_description{}; {
                depth_description.format = vk::Format::eD16Unorm;
                depth_description.samples = vk::SampleCountFlagBits::e1;
                depth_description.loadOp = vk::AttachmentLoadOp::eClear;
                depth_description.storeOp = vk::AttachmentStoreOp::eStore;
                depth_description.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
                depth_description.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
                depth_description.initialLayout = vk::ImageLayout::eUndefined;
                depth_description.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            }

            vk::AttachmentReference depth_attachment{}; {
                depth_attachment.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                depth_attachment.attachment = 0;
            }

            vk::SubpassDescription subpass_description{}; {
                subpass_description.colorAttachmentCount = 0;
                subpass_description.pColorAttachments = nullptr;
                subpass_description.pDepthStencilAttachment = &depth_attachment;
                subpass_description.pResolveAttachments = nullptr;
                subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            }

            vk::SubpassDependency subpass_dependency{}; {
                subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                subpass_dependency.dstSubpass = 0;
                subpass_dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                subpass_dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                subpass_dependency.srcAccessMask = {};
                subpass_dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            }

            vk::RenderPassCreateInfo create_info{}; {
                create_info.attachmentCount = 1;
                create_info.pAttachments = &depth_description;
                create_info.subpassCount = 1;
                create_info.pSubpasses = &subpass_description;
                create_info.dependencyCount = 1;
                create_info.pDependencies = &subpass_dependency;
            }
            p_impl->depth_pass
                .attach("depth", depth_create_info)
                .create(create_info);
        }

        /* Layouts */ {
            std::array<vk::DescriptorSetLayoutBinding, 2> main_layout_bindings{}; {
                main_layout_bindings[0].descriptorCount = 1;
                main_layout_bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                main_layout_bindings[0].binding = 0;
                main_layout_bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

                main_layout_bindings[1].descriptorCount = 1;
                main_layout_bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
                main_layout_bindings[1].binding = 1;
                main_layout_bindings[1].stageFlags = vk::ShaderStageFlagBits::eVertex;
            }
            vk::DescriptorSetLayoutCreateInfo main_layout_info{}; {
                main_layout_info.bindingCount = main_layout_bindings.size();
                main_layout_info.pBindings = main_layout_bindings.data();
            }
            p_impl->main_layout = ctx.device.logical.createDescriptorSetLayout(main_layout_info);

            vk::DescriptorSetLayoutBinding texture_layout_binding{}; {
                texture_layout_binding.descriptorCount = 1;
                texture_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                texture_layout_binding.binding = 0;
                texture_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
            }
            vk::DescriptorSetLayoutCreateInfo texture_layout_info{}; {
                texture_layout_info.bindingCount = 1;
                texture_layout_info.pBindings = &texture_layout_binding;
            }
            p_impl->texture_layout = ctx.device.logical.createDescriptorSetLayout(texture_layout_info);

            std::array<vk::DescriptorSetLayoutBinding, 2> palette_depth_bindings{}; {
                palette_depth_bindings[0].descriptorCount = 1;
                palette_depth_bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                palette_depth_bindings[0].binding = 0;
                palette_depth_bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

                palette_depth_bindings[1].descriptorCount = 1;
                palette_depth_bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                palette_depth_bindings[1].binding = 1;
                palette_depth_bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
            }
            vk::DescriptorSetLayoutCreateInfo palette_depth_info{}; {
                palette_depth_info.bindingCount = palette_depth_bindings.size();
                palette_depth_info.pBindings = palette_depth_bindings.data();
            }
            p_impl->palette_depth = ctx.device.logical.createDescriptorSetLayout(palette_depth_info);

            vk::DescriptorSetLayoutBinding lights_layout_binding{}; {
                lights_layout_binding.descriptorCount = 1;
                lights_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                lights_layout_binding.binding = 0;
                lights_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
            }
            vk::DescriptorSetLayoutCreateInfo lights_layout_info{}; {
                lights_layout_info.bindingCount = 1;
                lights_layout_info.pBindings = &lights_layout_binding;
            }
            p_impl->lights_layout = ctx.device.logical.createDescriptorSetLayout(lights_layout_info);
        }

        /* Shaders */ {
            Pipeline::CreateInfo basic_tile_info{}; {
                basic_tile_info.vertex = "assets/vulkan/basic_tile.vert.spv";
                basic_tile_info.fragment = "assets/vulkan/basic_tile.frag.spv";
                basic_tile_info.push_constants = vk::PushConstantRange{
                    vk::ShaderStageFlagBits::eVertex,
                    0,
                    sizeof(u32)
                };
                basic_tile_info.layouts = {
                    p_impl->main_layout,
                    p_impl->texture_layout
                };
                basic_tile_info.render_pass = p_impl->color_pass.handle();
                basic_tile_info.samples = vk::SampleCountFlagBits::e1;
                basic_tile_info.cull = vk::CullModeFlagBits::eNone;
                basic_tile_info.depth = true;
                basic_tile_info.dynamic_states = {
                    vk::DynamicState::eViewport,
                    vk::DynamicState::eScissor
                };
            }
            p_impl->basic_tile_shader = make_pipeline(basic_tile_info);

            Pipeline::CreateInfo depth_info{}; {
                depth_info.vertex = "assets/vulkan/depth.vert.spv";
                depth_info.fragment = "assets/vulkan/depth.frag.spv";
                depth_info.push_constants = vk::PushConstantRange{
                    vk::ShaderStageFlagBits::eVertex,
                    0,
                    sizeof(u32)
                };
                depth_info.layouts = {
                    p_impl->main_layout,
                    p_impl->palette_depth,
                    p_impl->texture_layout
                };
                depth_info.render_pass = p_impl->depth_pass.handle();
                depth_info.samples = vk::SampleCountFlagBits::e1;
                depth_info.cull = vk::CullModeFlagBits::eNone;
                depth_info.depth = true;
                depth_info.dynamic_states = {
                    vk::DynamicState::eViewport,
                    vk::DynamicState::eScissor,
                    vk::DynamicState::eDepthBias
                };
            }
            p_impl->depth_shader = make_pipeline(depth_info);

            Pipeline::CreateInfo shaded_pal_info{}; {
                shaded_pal_info.vertex = "assets/vulkan/shaded_pal_tile.vert.spv";
                shaded_pal_info.fragment = "assets/vulkan/shaded_pal_tile.frag.spv";
                shaded_pal_info.push_constants = vk::PushConstantRange{
                    vk::ShaderStageFlagBits::eVertex,
                    0,
                    sizeof(u32)
                };
                shaded_pal_info.layouts = {
                    p_impl->main_layout,
                    p_impl->palette_depth,
                    p_impl->texture_layout,
                };
                shaded_pal_info.render_pass = p_impl->color_pass.handle();
                shaded_pal_info.samples = vk::SampleCountFlagBits::e1;
                shaded_pal_info.cull = vk::CullModeFlagBits::eNone;
                shaded_pal_info.depth = true;
                shaded_pal_info.dynamic_states = {
                    vk::DynamicState::eViewport,
                    vk::DynamicState::eScissor
                };
            }
            p_impl->shaded_pal_shader = make_pipeline(shaded_pal_info);

            Pipeline::CreateInfo shaded_tile_info{}; {
                shaded_tile_info.vertex = "assets/vulkan/shaded_tile.vert.spv";
                shaded_tile_info.fragment = "assets/vulkan/shaded_tile.frag.spv";
                shaded_tile_info.push_constants = vk::PushConstantRange{
                    vk::ShaderStageFlagBits::eVertex,
                    0,
                    sizeof(u32)
                };
                shaded_tile_info.layouts = {
                    p_impl->main_layout,
                    p_impl->palette_depth,
                    p_impl->texture_layout,
                    p_impl->lights_layout,
                };
                shaded_tile_info.render_pass = p_impl->color_pass.handle();
                shaded_tile_info.samples = vk::SampleCountFlagBits::e1;
                shaded_tile_info.cull = vk::CullModeFlagBits::eNone;
                shaded_tile_info.depth = true;
                shaded_tile_info.dynamic_states = {
                    vk::DynamicState::eViewport,
                    vk::DynamicState::eScissor
                };
            }
            p_impl->shaded_tile_shader = make_pipeline(shaded_tile_info);
        }

        /* Synchronization */ {
            p_impl->image_available.reserve(meta::max_in_flight);
            p_impl->render_finished.reserve(meta::max_in_flight);

            for (u32 i = 0; i < meta::max_in_flight; ++i) {
                p_impl->image_available.emplace_back(ctx.device.logical.createSemaphore({}));
                p_impl->render_finished.emplace_back(ctx.device.logical.createSemaphore({}));
            }

            p_impl->in_flight.resize(meta::max_in_flight, nullptr);
        }

        /* Command Buffers */ {
            p_impl->command_buffers = make_command_buffers(3);
        }
    }

    Renderer::~Renderer() = default; // Fuck destroying vk context, who cares.

    void Renderer::draw(const DrawCmdList& draw_commands, const Framebuffer&) {
        auto result = ctx.device.logical.acquireNextImageKHR(p_impl->swapchain.handle, -1, p_impl->image_available[frame_index], nullptr, &image_index);

        if (!p_impl->in_flight[frame_index]) {
            vk::FenceCreateInfo fence_create_info{}; {
                fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
            }

            p_impl->in_flight[frame_index] = ctx.device.logical.createFence(fence_create_info, nullptr);
        }

        ctx.device.logical.waitForFences(p_impl->in_flight[frame_index], true, -1);

        auto& command_buffer = p_impl->command_buffers[image_index];

        vk::CommandBufferBeginInfo begin_info{}; {
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        }

        command_buffer.begin(begin_info);

        /* Final color pass */ {
            std::array<vk::ClearValue, 2> clear_values{}; {
                clear_values[0].color = std::array{ 0.01f, 0.01f, 0.01f, 0.0f };
                clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
            }

            vk::RenderPassBeginInfo render_pass_begin_info{}; {
                render_pass_begin_info.renderArea.extent = p_impl->swapchain.extent;
                render_pass_begin_info.framebuffer = p_impl->color_pass.framebuffer();
                render_pass_begin_info.renderPass = p_impl->color_pass.handle();
                render_pass_begin_info.clearValueCount = clear_values.size();
                render_pass_begin_info.pClearValues = clear_values.data();
            }

            vk::Viewport viewport{}; {
                viewport.width = p_impl->swapchain.extent.width;
                viewport.height = p_impl->swapchain.extent.height;
                viewport.x = 0;
                viewport.y = 0;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
            }

            vk::Rect2D scissor{}; {
                scissor.extent = p_impl->swapchain.extent;
                scissor.offset = { { 0, 0 } };
            }

            command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
            command_buffer.setViewport(0, viewport);
            command_buffer.setScissor(0, scissor);
            command_buffer.endRenderPass();
        }

        vk::ImageCopy copy{}; {
            copy.extent.width = p_impl->swapchain.extent.width;
            copy.extent.height = p_impl->swapchain.extent.height;
            copy.extent.depth = 1;
            copy.srcOffset = vk::Offset3D{};
            copy.dstOffset = vk::Offset3D{};
            copy.srcSubresource.baseArrayLayer = 0;
            copy.srcSubresource.layerCount = 1;
            copy.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            copy.srcSubresource.mipLevel = 0;
            copy.dstSubresource.baseArrayLayer = 0;
            copy.dstSubresource.layerCount = 1;
            copy.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            copy.dstSubresource.mipLevel = 0;
        }

        vk::ImageMemoryBarrier copy_barrier{}; {
            copy_barrier.image = p_impl->swapchain.images[image_index];
            copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            copy_barrier.subresourceRange.layerCount = 1;
            copy_barrier.subresourceRange.baseArrayLayer = 0;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.baseMipLevel = 0;
            copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
            copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
            copy_barrier.srcAccessMask = {};
            copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        }

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlagBits{},
            nullptr,
            nullptr,
            copy_barrier);

        command_buffer.copyImage(
            p_impl->color_pass["color"].handle, vk::ImageLayout::eTransferSrcOptimal,
            p_impl->swapchain.images[image_index], vk::ImageLayout::eTransferDstOptimal, copy);

        vk::ImageMemoryBarrier present_barrier{}; {
            present_barrier.image = p_impl->swapchain.images[image_index];
            present_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            present_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            present_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            present_barrier.subresourceRange.layerCount = 1;
            present_barrier.subresourceRange.baseArrayLayer = 0;
            present_barrier.subresourceRange.levelCount = 1;
            present_barrier.subresourceRange.baseMipLevel = 0;
            present_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            present_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
            present_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            present_barrier.dstAccessMask = {};
        }

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::DependencyFlagBits{},
            nullptr,
            nullptr,
            present_barrier);

        command_buffer.end();

        vk::PipelineStageFlags wait_mask{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submit_info{}; {
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &p_impl->command_buffers[image_index];
            submit_info.pWaitDstStageMask = &wait_mask;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &p_impl->image_available[frame_index];
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &p_impl->render_finished[frame_index];
        }

        ctx.device.logical.resetFences(p_impl->in_flight[frame_index]);
        ctx.device.graphics.submit(submit_info, p_impl->in_flight[frame_index]);

        vk::PresentInfoKHR present_info{}; {
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &p_impl->render_finished[frame_index];
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &p_impl->swapchain.handle;
            present_info.pImageIndices = &image_index;
        }

        result = ctx.device.graphics.presentKHR(&present_info);

        frame_index = (frame_index + 1) % meta::max_in_flight;
    }
} // namespace aryibi::renderer