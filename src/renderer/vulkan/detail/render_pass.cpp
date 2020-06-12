#include "render_pass.hpp"
#include "context.hpp"

namespace aryibi::renderer {
    RenderPass& RenderPass::attach(const std::string& name, const Image::CreateInfo& info) {
        _targets.emplace_back(RenderTarget{
            name, make_image(info)
        });

        return *this;
    }

    void RenderPass::create(const vk::RenderPassCreateInfo& info) {
        _handle = context().device.logical.createRenderPass(info);
        const auto& first = _targets.front().image;

        std::vector<vk::ImageView> attachments{};
        attachments.reserve(_targets.size());

        for (const auto& [_, image] : _targets) {
            attachments.emplace_back(image.view);
        }

        vk::FramebufferCreateInfo create_info{}; {
            create_info.renderPass = _handle;
            create_info.width = first.width;
            create_info.height = first.height;
            create_info.layers = 1;
            create_info.attachmentCount = attachments.size();
            create_info.pAttachments = attachments.data();
        }
        _framebuffer = context().device.logical.createFramebuffer(create_info);
    }

	void RenderPass::destroy() {
        context().device.logical.destroyFramebuffer(_framebuffer);
        for (auto& [_, target] : _targets) {
            destroy_image(target);
        }
        context().device.logical.destroyRenderPass(_handle);
        _targets.clear();
	}

    vk::RenderPass RenderPass::handle() const {
        return _handle;
    }

    vk::Framebuffer RenderPass::framebuffer() const {
        return _framebuffer;
    }

    const Image& RenderPass::operator [](const std::string& name) const {
        return std::find_if(_targets.begin(), _targets.end(), [&name](const RenderTarget& target) {
            return target.name == name;
        })->image;
    }
} // namespace aryibi::renderer