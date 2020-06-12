#ifndef ARBIYI_VULKAN_SAMPLER_HPP
#define ARBIYI_VULKAN_SAMPLER_HPP

#include "aryibi/renderer.hpp"
#include "forwards.hpp"
#include "types.hpp"

#include <vulkan/vulkan.hpp>

namespace aryibi::renderer {
    void make_samplers();
    [[nodiscard]] vk::Sampler point_sampler();
    [[nodiscard]] vk::Sampler linear_sampler();
    [[nodiscard]] vk::Sampler depth_sampler();
} // namespace aryibi::renderer

#endif //ARBIYI_VULKAN_SAMPLER_HPP
