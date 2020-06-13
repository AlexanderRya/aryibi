#include "detail/descriptor_set.hpp"
#include "detail/command_buffer.hpp"
#include "detail/constants.hpp"
#include "detail/context.hpp"
#include "detail/sampler.hpp"
#include "detail/buffer.hpp"

#include "windowing/glfw/impl_types.hpp"
#include "aryibi/renderer.hpp"
#include "impl_types.hpp"

#include <anton/math/transform.hpp>
#include <anton/math/matrix4.hpp>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_vulkan.h"

namespace aryibi::renderer {
    namespace aml = anton::math;

    struct InternalTexture {
        Texture handle{};
        DescriptorSet set{};
    };

    struct UniformData {
        aml::Matrix4 projection;
        aml::Matrix4 view;
    };

    struct LightData {
        struct {
            aml::Vector4 color{};
            aml::Matrix4 light_space_matrix{};
            aml::Vector3 light_atlas_pos{};
            f32 _pad0{};
        } directional_lights[5] = {};
        struct {
            aml::Vector4 color{};
            aml::Matrix4 light_space_matrix{};
            aml::Vector3 light_atlas_pos{};
            f32 radius{};
        } point_lights[20] = {};
        aml::Vector3 ambient_light_color{};
        u32 point_light_count{};
        u32 directional_light_count{};
    };

    const static auto& ctx = context();

    static u32 image_index{};
    static u32 frame_index{};

    static vk::DescriptorSetLayout texture_layout{};
    static std::vector<InternalTexture> textures{};

    static std::vector<std::pair<RawBuffer, usize>> to_delete{};

    void Renderer::impl::enqueue_for_deletion(RawBuffer& buffer) {
        to_delete.emplace_back(buffer, 0);

        for (auto& [buf, idx] : to_delete) {
            if (idx == meta::max_in_flight) {
                destroy_raw_buffer(buf);
            }
            idx++;
        }

        to_delete.erase(std::remove_if(to_delete.begin(), to_delete.end(), [](const auto& pair) {
            return pair.second == meta::max_in_flight + 1;
        }), to_delete.end());

        buffer.handle = nullptr;
        buffer.mapped = nullptr;
    }

    void Renderer::impl::write_dyn_mesh(DynMesh& mesh, const std::vector<f32>& vertices) {
        auto& vbo = mesh.vbo[frame_index];

        if (!vbo.exists()) {
            vbo.create(vk::BufferUsageFlagBits::eVertexBuffer);
        }

        vbo.write(vertices.data(), vertices.size() * sizeof(f32));
        mesh.vertex_count = vertices.size() / sizeof(Vertex);
    }

    usize Renderer::impl::load_texture(const u8* data, const TextureHandle& handle) {
        auto& texture = textures.emplace_back();

        texture.handle = renderer::load_texture(data, handle.width(), handle.height(), 4, vk::Format::eR8G8B8A8Srgb);
        texture.set.create(texture_layout);

        SingleUpdateImageInfo update{}; {
            update.image = texture.handle.info(handle.p_impl->sampler);
            update.binding = 0;
            update.type = vk::DescriptorType::eCombinedImageSampler;
        }
        texture.set.update(update);

        return textures.size() - 1;
    }

    void Renderer::impl::update_buffers(const DrawCmdList& commands) {
        auto& transform_buffer = transforms[frame_index];
        auto& uniform_data_buffer = uniform_data[frame_index];
        auto& lights_buffer = lights_data[frame_index];
        auto& light_mat_buffer = light_mats[frame_index];

        auto& current_main_set = main_set[frame_index];
        auto& current_lights_set = lights_set[frame_index];

        aml::Vector2 camera_view_size_in_tiles{
            static_cast<float>(swapchain.extent.width) / commands.camera.unit_size,
            static_cast<float>(swapchain.extent.height) / commands.camera.unit_size
        };

        UniformData camera_data{}; {
            camera_data.view = aml::inverse(aml::translate(commands.camera.position));
            if (commands.camera.center_view) {
                camera_data.projection = aml::orthographic_rh(
                    -camera_view_size_in_tiles.x / 2.f, camera_view_size_in_tiles.x / 2.f,
                    -camera_view_size_in_tiles.y / 2.f, camera_view_size_in_tiles.y / 2.f,
                    -10.0f, 20.0f);
            } else {
                camera_data.projection = aml::orthographic_rh(0, camera_view_size_in_tiles.x, -camera_view_size_in_tiles.y, 0, -10.0f, 20.0f);
            }

            camera_data.projection[1][1] *= -1;
        }

        if (uniform_data_buffer.size() == sizeof(UniformData)) {
            uniform_data_buffer.write(&camera_data, sizeof(UniformData));
        } else {
            uniform_data_buffer.write(&camera_data, sizeof(UniformData));

            SingleUpdateBufferInfo update{}; {
                update.binding = 0;
                update.buffer = uniform_data_buffer.info();
                update.type = vk::DescriptorType::eUniformBuffer;
            }

            current_main_set.update(update);
        }

        std::vector<aml::Matrix4> models;
        models.reserve(commands.commands.size());

        for (const auto& cmd : commands.commands) {
            models.emplace_back(aml::translate(cmd.transform.position));
        }

        if (transform_buffer.size() == models.size() * sizeof(aml::Matrix4)) {
            transform_buffer.write(models.data(), models.size() * sizeof(aml::Matrix4));
        } else {
            transform_buffer.write(models.data(), models.size() * sizeof(aml::Matrix4));

            SingleUpdateBufferInfo update{}; {
                update.binding = 1;
                update.buffer = transform_buffer.info();
                update.type = vk::DescriptorType::eStorageBuffer;
            }

            current_main_set.update(update);
        }

        const i32 light_atlas_tiles = aml::ceil(aml::sqrt(commands.directional_lights.size() + commands.point_lights.size()));

        LightData light_data{};
        std::vector<aml::Matrix4> light_matrices{};

        light_data.directional_light_count = commands.directional_lights.size();
        light_data.point_light_count = commands.point_lights.size();

        light_matrices.reserve(light_data.directional_light_count + light_data.point_light_count);
        for (usize i = 0; i < light_data.directional_light_count; ++i) {
            auto& light = commands.directional_lights[i];

            auto view = aml::translate(commands.camera.position);
            view *= aml::rotate_z(light.rotation.z) *
                    aml::rotate_y(light.rotation.y) *
                    aml::rotate_x(light.rotation.x);
            view = aml::inverse(view);

            light_data.directional_lights[i].color = {
                light.color.fred(),
                light.color.fgreen(),
                light.color.fblue(),
                light.color.falpha()
            };
            light.matrix = light_data.directional_lights[i].light_space_matrix = camera_data.projection * view;
            light.light_atlas_pos = {
                static_cast<float>(i % light_atlas_tiles) / static_cast<float>(light_atlas_tiles),
                static_cast<float>(i / light_atlas_tiles) / static_cast<float>(light_atlas_tiles),
            };
            light.light_atlas_size = 1.f / static_cast<float>(light_atlas_tiles);
            light_data.directional_lights[i].light_atlas_pos = aml::Vector3{
                light.light_atlas_pos,
                light.light_atlas_size
            };

            light_matrices.emplace_back(camera_data.projection * view);
        }

        for (usize i = 0; i < commands.point_lights.size(); ++i) {
            auto& light = commands.point_lights[i];

            auto view = aml::translate(commands.camera.position);
            view = aml::inverse(view);

            light_data.point_lights[i].color = {
                light.color.fred(),
                light.color.fgreen(),
                light.color.fblue(),
                light.color.falpha()
            };
            light_data.point_lights[i].radius = light.radius;
            light.matrix = light_data.point_lights[i].light_space_matrix = camera_data.projection * view;
            light.light_atlas_pos = {
                static_cast<float>(light_data.directional_light_count + i % light_atlas_tiles) / static_cast<float>(light_atlas_tiles),
                static_cast<float>(light_data.directional_light_count + i / light_atlas_tiles) / static_cast<float>(light_atlas_tiles),
            };
            light.light_atlas_size = 1.f / static_cast<float>(light_atlas_tiles);
            light_data.point_lights[i].light_atlas_pos = aml::Vector3{
                light.light_atlas_pos,
                light.light_atlas_size
            };

            light_matrices.emplace_back(camera_data.projection * view);
        }

        light_data.ambient_light_color = {
            commands.ambient_light_color.fred(),
            commands.ambient_light_color.fgreen(),
            commands.ambient_light_color.fblue()
        };

        if (lights_buffer.size() == sizeof(LightData)) {
            lights_buffer.write(&light_data, sizeof(LightData));
        } else {
            lights_buffer.write(&light_data, sizeof(LightData));

            SingleUpdateBufferInfo update{}; {
                update.binding = 0;
                update.buffer = lights_buffer.info();
                update.type = vk::DescriptorType::eUniformBuffer;
            }

            current_lights_set.update(update);
        }

        if (light_mat_buffer.size() == light_matrices.size() * sizeof(aml::Matrix4)) {
            light_mat_buffer.write(light_matrices.data(), light_matrices.size() * sizeof(aml::Matrix4));
        } else {
            light_mat_buffer.write(light_matrices.data(), light_matrices.size() * sizeof(aml::Matrix4));

            SingleUpdateBufferInfo update{}; {
                update.binding = 2;
                update.buffer = light_mat_buffer.info();
                update.type = vk::DescriptorType::eStorageBuffer;
            }

            current_main_set.update(update);
        }
    }

    Renderer::Renderer(windowing::WindowHandle window)
        : window(window),
          p_impl(std::make_unique<impl>()) {
        initialise(window.p_impl->handle);

        /* Swapchain */ {
            auto capabilities = ctx.device.physical.getSurfaceCapabilitiesKHR(acquire_surface(window.p_impl->handle));

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

            auto surface_formats = ctx.device.physical.getSurfaceFormatsKHR(acquire_surface(window.p_impl->handle));
            p_impl->swapchain.format = surface_formats[0];

            for (const auto& each : surface_formats) {
                if ((each.format == vk::Format::eB8G8R8A8Srgb || each.format == vk::Format::eR8G8B8A8Srgb) &&
                    each.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                    p_impl->swapchain.format = each;
                }
            }

            vk::SwapchainCreateInfoKHR swapchain_create_info{}; {
                swapchain_create_info.surface = acquire_surface(window.p_impl->handle);
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

        /* Fuck this shit */ {
            std::array<u8, 4> black{ 0, 0, 0, 255 };
            p_impl->palette_texture = load_texture(black.data(), 1, 1, 4, vk::Format::eR8G8B8A8Srgb);
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
                subpass_dependency.srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
                subpass_dependency.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
                subpass_dependency.srcAccessMask = {};
                subpass_dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
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
                attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

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
                subpass_dependency.srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
                subpass_dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
                subpass_dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                subpass_dependency.dstAccessMask = vk::AccessFlagBits::eShaderRead;
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

        /* ImGui pass */ {
            vk::AttachmentDescription color_description{}; {
                color_description.format = p_impl->swapchain.format.format;
                color_description.samples = vk::SampleCountFlagBits::e1;
                color_description.loadOp = vk::AttachmentLoadOp::eLoad;
                color_description.storeOp = vk::AttachmentStoreOp::eStore;
                color_description.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
                color_description.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
                color_description.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
                color_description.finalLayout = vk::ImageLayout::eTransferSrcOptimal;
            }

            vk::AttachmentReference color_attachment{}; {
                color_attachment.layout = vk::ImageLayout::eColorAttachmentOptimal;
                color_attachment.attachment = 0;
            }

            vk::SubpassDescription subpass_description{}; {
                subpass_description.colorAttachmentCount = 1;
                subpass_description.pColorAttachments = &color_attachment;
                subpass_description.pDepthStencilAttachment = nullptr;
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
                create_info.pAttachments = &color_description;
                create_info.subpassCount = 1;
                create_info.pSubpasses = &subpass_description;
                create_info.dependencyCount = 1;
                create_info.pDependencies = &subpass_dependency;
            }
            p_impl->imgui_pass
                .attach("color", p_impl->color_pass["color"])
                .create(create_info);
        }

        /* Layouts */ {
            std::array<vk::DescriptorSetLayoutBinding, 3> main_layout_bindings{}; {
                main_layout_bindings[0].descriptorCount = 1;
                main_layout_bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                main_layout_bindings[0].binding = 0;
                main_layout_bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

                main_layout_bindings[1].descriptorCount = 1;
                main_layout_bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
                main_layout_bindings[1].binding = 1;
                main_layout_bindings[1].stageFlags = vk::ShaderStageFlagBits::eVertex;

                main_layout_bindings[2].descriptorCount = 1;
                main_layout_bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
                main_layout_bindings[2].binding = 2;
                main_layout_bindings[2].stageFlags = vk::ShaderStageFlagBits::eVertex;
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
            texture_layout = ctx.device.logical.createDescriptorSetLayout(texture_layout_info);

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
            p_impl->palette_depth_layout = ctx.device.logical.createDescriptorSetLayout(palette_depth_info);

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
                    sizeof(u32) * 2
                };
                basic_tile_info.layouts = {
                    p_impl->main_layout,
                    texture_layout
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
                    sizeof(u32) * 2
                };
                depth_info.layouts = {
                    p_impl->main_layout,
                    texture_layout
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
                    sizeof(u32) * 2
                };
                shaded_pal_info.layouts = {
                    p_impl->main_layout,
                    p_impl->palette_depth_layout,
                    texture_layout,
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
                    sizeof(u32) * 2
                };
                shaded_tile_info.layouts = {
                    p_impl->main_layout,
                    p_impl->palette_depth_layout,
                    texture_layout,
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

        /* Resources */ {
            p_impl->uniform_data.create(vk::BufferUsageFlagBits::eUniformBuffer);
            p_impl->lights_data.create(vk::BufferUsageFlagBits::eUniformBuffer);
            p_impl->transforms.create(vk::BufferUsageFlagBits::eStorageBuffer);
            p_impl->light_mats.create(vk::BufferUsageFlagBits::eStorageBuffer);

            p_impl->main_set.create(p_impl->main_layout);
            p_impl->palette_depth_set.create(p_impl->palette_depth_layout);
            p_impl->lights_set.create(p_impl->lights_layout);

            std::vector<SingleUpdateImageInfo> palette_depth_update(2); {
                palette_depth_update[0].type = vk::DescriptorType::eCombinedImageSampler;
                palette_depth_update[0].binding = 0;
                palette_depth_update[0].image = {
                    depth_sampler(),
                    p_impl->depth_pass["depth"].view,
                    vk::ImageLayout::eShaderReadOnlyOptimal
                };

                palette_depth_update[1].type = vk::DescriptorType::eCombinedImageSampler;
                palette_depth_update[1].binding = 1;
                palette_depth_update[1].image = p_impl->palette_texture.info(linear_sampler());
            }
            p_impl->palette_depth_set.update(palette_depth_update);
        }

        /* ImGui context */ {
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            (void)io;
            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForVulkan(window.p_impl->handle, true);

            ImGui_ImplVulkan_InitInfo imgui_vulkan_init_info{}; {
                imgui_vulkan_init_info.QueueFamily = context().device.family;
                imgui_vulkan_init_info.Queue = context().device.graphics;
                imgui_vulkan_init_info.PhysicalDevice = context().device.physical;
                imgui_vulkan_init_info.MinImageCount = p_impl->swapchain.images.size() - 1;
                imgui_vulkan_init_info.ImageCount = p_impl->swapchain.images.size();
                imgui_vulkan_init_info.Device = context().device.logical;
                imgui_vulkan_init_info.Allocator = nullptr;
                imgui_vulkan_init_info.Instance = context().instance;
                imgui_vulkan_init_info.PipelineCache = nullptr;
                imgui_vulkan_init_info.DescriptorPool = main_descriptor_pool();
                imgui_vulkan_init_info.CheckVkResultFn = [](VkResult result) -> void {
                    if (result != VK_SUCCESS) {
                        throw std::runtime_error("Internal ImGui error");
                    }
                };
            }

            ImGui_ImplVulkan_Init(&imgui_vulkan_init_info, p_impl->imgui_pass.handle());

            auto command_buffer = begin_transient(); {
                ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
            } end_transient(command_buffer);

            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }
    }

    Renderer::~Renderer() = default; // Fuck destroying vk context, who cares.

    void Renderer::draw(const DrawCmdList& commands, const Framebuffer&) {
        static u64 frames = 0;

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

        p_impl->update_buffers(commands);

        /* Shadow pass */ {
            vk::ClearValue clear_value{}; {
                clear_value.color = {};
                clear_value.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
            }

            vk::RenderPassBeginInfo render_pass_begin_info{}; {
                render_pass_begin_info.renderArea.extent = vk::Extent2D{
                    p_impl->depth_pass["depth"].width,
                    p_impl->depth_pass["depth"].height
                };
                render_pass_begin_info.framebuffer = p_impl->depth_pass.framebuffer();
                render_pass_begin_info.renderPass = p_impl->depth_pass.handle();
                render_pass_begin_info.clearValueCount = 1;
                render_pass_begin_info.pClearValues = &clear_value;
            }

            command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);

            for (usize i = 0; i < commands.directional_lights.size(); ++i) {
                auto& light = commands.directional_lights[i];

                vk::Viewport viewport{}; {
                    viewport.width = light.light_atlas_size * p_impl->depth_pass["depth"].width;
                    viewport.height = light.light_atlas_size * p_impl->depth_pass["depth"].height;
                    viewport.x = light.light_atlas_pos.x * p_impl->depth_pass["depth"].width;
                    viewport.y = light.light_atlas_pos.y * p_impl->depth_pass["depth"].height;
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;
                }

                vk::Rect2D scissor{}; {
                    scissor.extent.width = p_impl->depth_pass["depth"].width;
                    scissor.extent.height = p_impl->depth_pass["depth"].height;
                    scissor.offset = { { 0, 0 } };
                }

                command_buffer.setViewport(0, viewport);
                command_buffer.setScissor(0, scissor);

                for (usize j = 0; j < commands.commands.size(); ++j) {
                    auto& command = commands.commands[j];
                    auto& mesh = command.mesh.p_impl->handle;
                    auto& texture = textures[command.texture.p_impl->handle];

                    if (!command.cast_shadows) {
                        continue;
                    }

                    std::array constants{
                        static_cast<u32>(j),
                        static_cast<u32>(i)
                    };

                    std::array descriptor_sets{
                        p_impl->main_set[frame_index].handle(),
                        texture.set[frame_index].handle()
                    };

                    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p_impl->depth_shader);
                    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, p_impl->depth_shader, 0, descriptor_sets, nullptr);
                    command_buffer.pushConstants<u32>(p_impl->depth_shader, vk::ShaderStageFlagBits::eVertex, 0, constants);
                    command_buffer.bindVertexBuffers(0, mesh.vbo.handle, static_cast<vk::DeviceSize>(0));
                    command_buffer.draw(mesh.vertex_count, 1, 0, 0);
                }
            }

            for (usize i = 0; i < commands.point_lights.size(); ++i) {
                auto& light = commands.point_lights[i];

                vk::Viewport viewport{}; {
                    viewport.width = light.light_atlas_size * p_impl->depth_pass["depth"].width;
                    viewport.height = light.light_atlas_size * p_impl->depth_pass["depth"].height;
                    viewport.x = light.light_atlas_pos.x * p_impl->depth_pass["depth"].width;
                    viewport.y = light.light_atlas_pos.y * p_impl->depth_pass["depth"].height;
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;
                }

                vk::Rect2D scissor{}; {
                    scissor.extent.width = p_impl->depth_pass["depth"].width;
                    scissor.extent.height = p_impl->depth_pass["depth"].height;
                    scissor.offset = { { 0, 0 } };
                }

                command_buffer.setViewport(0, viewport);
                command_buffer.setScissor(0, scissor);

                for (usize j = 0; j < commands.commands.size(); ++j) {
                    auto& command = commands.commands[j];
                    auto& mesh = command.mesh.p_impl->handle;
                    auto& texture = textures[command.texture.p_impl->handle];

                    if (!command.cast_shadows) {
                        continue;
                    }

                    std::array constants{
                        static_cast<u32>(j),
                        static_cast<u32>(i)
                    };

                    std::array descriptor_sets{
                        p_impl->main_set[frame_index].handle(),
                        texture.set[frame_index].handle()
                    };

                    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p_impl->depth_shader);
                    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, p_impl->depth_shader, 0, descriptor_sets, nullptr);
                    command_buffer.pushConstants<u32>(p_impl->depth_shader, vk::ShaderStageFlagBits::eVertex, 0, constants);
                    command_buffer.bindVertexBuffers(0, mesh.vbo.handle, static_cast<vk::DeviceSize>(0));
                    command_buffer.draw(mesh.vertex_count, 1, 0, 0);
                }
            }
            command_buffer.endRenderPass();
        }

        /* Color pass */ {
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

            for (usize i = 0; i < commands.commands.size(); ++i) {
                auto& command = commands.commands[i];
                auto& mesh = command.mesh.p_impl->handle;
                auto& texture = textures[command.texture.p_impl->handle];
                auto& shader = command.shader.p_impl->handle;

                std::array constants{
                    static_cast<u32>(i),
                    static_cast<u32>(0)
                };

                std::vector<vk::DescriptorSet> descriptor_sets{};

                if (shader.handle == p_impl->basic_tile_shader.handle) {
                    descriptor_sets = {
                        p_impl->main_set[frame_index].handle(),
                        texture.set[frame_index].handle()
                    };
                } else if (shader.handle == p_impl->shaded_tile_shader.handle) {
                    descriptor_sets = {
                        p_impl->main_set[frame_index].handle(),
                        p_impl->palette_depth_set[frame_index].handle(),
                        texture.set[frame_index].handle(),
                        p_impl->lights_set[frame_index].handle()
                    };
                }

                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shader);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shader, 0, descriptor_sets, nullptr);
                command_buffer.pushConstants<u32>(shader, vk::ShaderStageFlagBits::eVertex, 0, constants);
                command_buffer.bindVertexBuffers(0, mesh.vbo.handle, static_cast<vk::DeviceSize>(0));
                command_buffer.draw(mesh.vertex_count, 1, 0, 0);
            }

            command_buffer.endRenderPass();
        }

        /* ImGui pass */ {
            vk::ClearValue clear_value{};
            clear_value.color = std::array{ 0.01f, 0.01f, 0.01f, 0.0f };

            vk::RenderPassBeginInfo render_pass_begin_info{}; {
                render_pass_begin_info.renderArea.extent = p_impl->swapchain.extent;
                render_pass_begin_info.framebuffer = p_impl->imgui_pass.framebuffer();
                render_pass_begin_info.renderPass = p_impl->imgui_pass.handle();
                render_pass_begin_info.clearValueCount = 1;
                render_pass_begin_info.pClearValues = &clear_value;
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
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
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
            p_impl->imgui_pass["color"].handle, vk::ImageLayout::eTransferSrcOptimal,
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

        vk::PipelineStageFlags wait_mask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
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
        frames++;
    }

    void Renderer::clear(Framebuffer&, anton::math::Vector4) {

    }

    void Renderer::set_shadow_resolution(u32, u32) {

    }

    anton::math::Vector2 Renderer::get_shadow_resolution() const {
        return {
            static_cast<f32>(p_impl->depth_pass["depth"].width),
            static_cast<f32>(p_impl->depth_pass["depth"].height)
        };
    }

    void Renderer::set_palette(const ColorPalette& palette) {
        p_impl->palette_texture.destroy();
        // The palette texture is a regular 2D texture. The X axis represents the
        // different shades of a color, and the Y axis represents the different colors
        // available. We are going to dedicate 0,0 and its row entirely just for the
        // transparent color, So we'll add 1 to the height.
        const u32 height = palette.colors.size() + 1;
        // The width of the palette texture will be equal to the maximum shades
        // available of any color.
        u32 width = 0;
        for (const auto& color : palette.colors) {
            if (color.shades.size() > width) {
                width = color.shades.size();
            }
        }
        assert(height > 0 && width > 0);

        constexpr i32 channels = 4;
        auto data = new u8[width * height * channels];
        std::memcpy((void*)(data), (void*)&palette.transparent_color, sizeof(u32));
        i32 px_y = 1, px_x = 0;
        for (const auto& color : palette.colors) {
            for (const auto& shade : color.shades) {
                std::memcpy((void*)(data + (px_x + px_y * width) * channels), (void*)&shade, sizeof(u32));
                ++px_x;
            }
            px_x = 0;
            ++px_y;
        }
        p_impl->palette_texture = load_texture(data, width, height, channels, vk::Format::eR8G8B8A8Srgb);

        SingleUpdateImageInfo update{}; {
            update.type = vk::DescriptorType::eCombinedImageSampler;
            update.binding = 1;
            update.image = p_impl->palette_texture.info(linear_sampler());
        }
        p_impl->palette_depth_set.update(update);

        delete[] data;
    }

    ShaderHandle Renderer::lit_shader() const {
        ShaderHandle handle{};
        handle.p_impl->handle = p_impl->shaded_tile_shader;
        return handle;
    }

    ShaderHandle Renderer::lit_paletted_shader() const {
        ShaderHandle handle{};
        handle.p_impl->handle = p_impl->shaded_pal_shader;
        return handle;
    }

    ShaderHandle Renderer::unlit_shader() const {
        ShaderHandle handle{};
        handle.p_impl->handle = p_impl->basic_tile_shader;
        return handle;
    }

    Framebuffer Renderer::get_window_framebuffer() {
        return {};
    }

    void Renderer::start_frame(Color) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::finish_frame() {

    }
} // namespace aryibi::renderer