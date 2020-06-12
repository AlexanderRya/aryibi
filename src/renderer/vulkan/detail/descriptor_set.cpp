#include "descriptor_set.hpp"
#include "context.hpp"

namespace aryibi::renderer {
    static vk::DescriptorPool descriptor_pool;

    void SingleDescriptorSet::create(const vk::DescriptorSetLayout layout) {
        vk::DescriptorSetAllocateInfo info{}; {
            info.descriptorSetCount = 1;
            info.descriptorPool = descriptor_pool;
            info.pSetLayouts = &layout;
        }

        descriptor_set = context().device.logical.allocateDescriptorSets(info).back();
    }

    void SingleDescriptorSet::update(const SingleUpdateBufferInfo& info) {
        vk::WriteDescriptorSet write{}; {
            write.descriptorCount = 1;
            write.pImageInfo = nullptr;
            write.pTexelBufferView = nullptr;
            write.pBufferInfo = &info.buffer;
            write.dstSet = descriptor_set;
            write.dstBinding = info.binding;
            write.dstArrayElement = 0;
            write.descriptorType = info.type;
        }

        context().device.logical.updateDescriptorSets(write, nullptr);
    }

    void SingleDescriptorSet::update(const std::vector<SingleUpdateBufferInfo>& info) {
        for (const auto& each : info) {
            vk::WriteDescriptorSet write{}; {
                write.descriptorCount = 1;
                write.pImageInfo = nullptr;
                write.pTexelBufferView = nullptr;
                write.pBufferInfo = &each.buffer;
                write.dstSet = descriptor_set;
                write.dstBinding = each.binding;
                write.dstArrayElement = 0;
                write.descriptorType = each.type;
            }

            context().device.logical.updateDescriptorSets(write, nullptr);
        }
    }

    void SingleDescriptorSet::update(const UpdateImageInfo& info) {
        vk::WriteDescriptorSet write{}; {
            write.descriptorCount = info.images->size();
            write.pImageInfo = info.images->data();
            write.pTexelBufferView = nullptr;
            write.pBufferInfo = nullptr;
            write.dstSet = descriptor_set;
            write.dstBinding = info.binding;
            write.dstArrayElement = 0;
            write.descriptorType = info.type;
        }

        context().device.logical.updateDescriptorSets(write, nullptr);
    }

    void SingleDescriptorSet::update(const SingleUpdateImageInfo& info) {
        vk::WriteDescriptorSet write{}; {
            write.descriptorCount = 1;
            write.pImageInfo = &info.image;
            write.pTexelBufferView = nullptr;
            write.pBufferInfo = nullptr;
            write.dstSet = descriptor_set;
            write.dstBinding = info.binding;
            write.dstArrayElement = 0;
            write.descriptorType = info.type;
        }

        context().device.logical.updateDescriptorSets(write, nullptr);
    }

    vk::DescriptorSet SingleDescriptorSet::handle() const {
        return descriptor_set;
    }


    void DescriptorSet::create(const vk::DescriptorSetLayout layout) {
        for (auto& descriptor_set : descriptor_sets) {
            descriptor_set.create(layout);
        }
    }

    void DescriptorSet::update(const UpdateBufferInfo& info) {
        for (auto& descriptor_set : descriptor_sets) {
            for (const auto& buffer : info.buffers) {
                SingleUpdateBufferInfo single_info{}; {
                    single_info.buffer = buffer;
                    single_info.binding = info.binding;
                    single_info.type = info.type;
                }

                descriptor_set.update(single_info);
            }
        }
    }

    void DescriptorSet::update(const std::vector<UpdateBufferInfo>& infos) {
        for (auto& descriptor_set : descriptor_sets) {
            for (auto& info : infos) {
                for (const auto& buffer : info.buffers) {
                    SingleUpdateBufferInfo single_info{}; {
                        single_info.buffer = buffer;
                        single_info.binding = info.binding;
                        single_info.type = info.type;
                    }

                    descriptor_set.update(single_info);
                }
            }
        }
    }

    void DescriptorSet::update(const UpdateImageInfo& info) {
        for (auto& descriptor_set : descriptor_sets) {
            descriptor_set.update(info);
        }
    }

    void DescriptorSet::update(const SingleUpdateImageInfo& info) {
        for (auto& descriptor_set : descriptor_sets) {
            descriptor_set.update(info);
        }
    }

    SingleDescriptorSet& DescriptorSet::operator [](const usize idx) {
        return descriptor_sets[idx];
    }

    const SingleDescriptorSet& DescriptorSet::operator [](const usize idx) const {
        return descriptor_sets[idx];
    }

    void make_descriptor_pool() {
        std::array<vk::DescriptorPoolSize, 4> descriptor_pool_sizes{ {
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 }
        } };

        vk::DescriptorPoolCreateInfo create_info{}; {
            create_info.poolSizeCount = descriptor_pool_sizes.size();
            create_info.pPoolSizes = descriptor_pool_sizes.data();
            create_info.maxSets = 4 * 1000;
        }

        descriptor_pool = context().device.logical.createDescriptorPool(create_info);
    }
} // namespace aryibi::renderer