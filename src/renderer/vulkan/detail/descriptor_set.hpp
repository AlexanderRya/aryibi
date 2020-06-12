#ifndef ARYIBI_VULKAN_DESCRIPTOR_SET_HPP
#define ARYIBI_VULKAN_DESCRIPTOR_SET_HPP

#include "constants.hpp"
#include "forwards.hpp"
#include "types.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <array>

namespace aryibi::renderer {
    struct SingleUpdateBufferInfo {
        vk::DescriptorBufferInfo buffer{};
        vk::DescriptorType type{};
        u64 binding{};
    };

    struct UpdateImageInfo {
        std::vector<vk::DescriptorImageInfo>* images{};
        vk::DescriptorType type{};
        u64 binding{};
    };

    struct SingleUpdateImageInfo {
        vk::DescriptorImageInfo image{};
        vk::DescriptorType type{};
        u64 binding{};
    };

    class SingleDescriptorSet {
        vk::DescriptorSet descriptor_set{};
    public:
        void create(const vk::DescriptorSetLayout);
        void update(const SingleUpdateBufferInfo&);
        void update(const std::vector<SingleUpdateBufferInfo>&);
        void update(const UpdateImageInfo&);
        void update(const SingleUpdateImageInfo&);

        [[nodiscard]] vk::DescriptorSet handle() const;
    };

    struct UpdateBufferInfo {
        std::array<vk::DescriptorBufferInfo, meta::max_in_flight> buffers;
        vk::DescriptorType type{};
        u64 binding{};
    };

    class DescriptorSet {
        std::array<SingleDescriptorSet, meta::max_in_flight> descriptor_sets;
    public:
        DescriptorSet() = default;

        void create(const vk::DescriptorSetLayout);
        void update(const UpdateBufferInfo&);
        void update(const std::vector<UpdateBufferInfo>&);
        void update(const UpdateImageInfo&);
        void update(const SingleUpdateImageInfo&);

        [[nodiscard]] SingleDescriptorSet& operator [](const usize);
        [[nodiscard]] const SingleDescriptorSet& operator [](const usize) const;
    };

    void make_descriptor_pool();
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_DESCRIPTOR_SET_HPP
