#include "sampler.hpp"
#include "context.hpp"

#include <vulkan/vulkan.hpp>

namespace aryibi::renderer {
    static vk::Sampler point{};
    static vk::Sampler linear{};
    static vk::Sampler depth{};

    [[nodiscard]] static vk::Sampler make_point_sampler() {
        vk::SamplerCreateInfo info{}; {
            info.magFilter = vk::Filter::eNearest;
            info.minFilter = vk::Filter::eNearest;
            info.addressModeU = vk::SamplerAddressMode::eRepeat;
            info.addressModeV = vk::SamplerAddressMode::eRepeat;
            info.addressModeW = vk::SamplerAddressMode::eRepeat;
            info.anisotropyEnable = true;
            info.maxAnisotropy = 16;
            info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
            info.unnormalizedCoordinates = false;
            info.compareEnable = false;
            info.compareOp = vk::CompareOp::eAlways;
            info.mipmapMode = vk::SamplerMipmapMode::eLinear;
            info.minLod = 0;
            info.maxLod = 16;
            info.mipLodBias = 0;
        }

        return context().device.logical.createSampler(info);
    }

    [[nodiscard]] static vk::Sampler make_linear_sampler() {
        vk::SamplerCreateInfo info{}; {
            info.magFilter = vk::Filter::eLinear;
            info.minFilter = vk::Filter::eLinear;
            info.addressModeU = vk::SamplerAddressMode::eRepeat;
            info.addressModeV = vk::SamplerAddressMode::eRepeat;
            info.addressModeW = vk::SamplerAddressMode::eRepeat;
            info.anisotropyEnable = true;
            info.maxAnisotropy = 16;
            info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
            info.unnormalizedCoordinates = false;
            info.compareEnable = false;
            info.compareOp = vk::CompareOp::eAlways;
            info.mipmapMode = vk::SamplerMipmapMode::eLinear;
            info.minLod = 0;
            info.maxLod = 16;
            info.mipLodBias = 0;
        }

        return context().device.logical.createSampler(info);
    }

    [[nodiscard]] static vk::Sampler make_depth_sampler() {
        vk::SamplerCreateInfo info{}; {
            info.magFilter = vk::Filter::eLinear;
            info.minFilter = vk::Filter::eLinear;
            info.addressModeU = vk::SamplerAddressMode::eClampToBorder;
            info.addressModeV = vk::SamplerAddressMode::eClampToBorder;
            info.addressModeW = vk::SamplerAddressMode::eClampToBorder;
            info.anisotropyEnable = false;
            info.maxAnisotropy = 1;
            info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
            info.unnormalizedCoordinates = false;
            info.compareEnable = false;
            info.compareOp = vk::CompareOp::eAlways;
            info.mipmapMode = vk::SamplerMipmapMode::eLinear;
            info.minLod = 0;
            info.maxLod = 16;
            info.mipLodBias = 0;
        }

        return context().device.logical.createSampler(info);
    }

    void make_samplers() {
        point = make_point_sampler();
        linear = make_linear_sampler();
        depth = make_depth_sampler();
    }

    vk::Sampler point_sampler() {
        return point;
    }

    vk::Sampler linear_sampler() {
        return linear;
    }

    vk::Sampler depth_sampler() {
        return depth;
    }
} // namespace aryibi::renderer