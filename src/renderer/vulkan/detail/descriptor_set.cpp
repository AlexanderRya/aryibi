#include "descriptor_set.hpp"
#include "context.hpp"

namespace aryibi::renderer {
    static vk::DescriptorPool main_pool;

    void SingleDescriptorSet::create(const vk::DescriptorSetLayout layout) {
        vk::DescriptorSetAllocateInfo info{}; {
            info.descriptorSetCount = 1;
            info.descriptorPool = main_pool;
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

    void DescriptorSet::update(const std::vector<SingleUpdateImageInfo>& infos) {
        for (auto& set : descriptor_sets) {
            for (const auto& info : infos) {
                SingleUpdateImageInfo single_info{}; {
                    single_info.image = info.image;
                    single_info.binding = info.binding;
                    single_info.type = info.type;
                }

                set.update(single_info);
            }
        }
    }

    vk::DescriptorSet SingleDescriptorSet::handle() const {
        return descriptor_set;
    }

    bool SingleDescriptorSet::exists() const {
        return descriptor_set;
    }


    void DescriptorSet::create(const vk::DescriptorSetLayout layout) {
        for (auto& set : descriptor_sets) {
            set.create(layout);
        }
    }

    void DescriptorSet::update(const UpdateBufferInfo& info) {
        for (auto& set : descriptor_sets) {
            for (const auto& buffer : info.buffers) {
                SingleUpdateBufferInfo single_info{}; {
                    single_info.buffer = buffer;
                    single_info.binding = info.binding;
                    single_info.type = info.type;
                }

                set.update(single_info);
            }
        }
    }

    void DescriptorSet::update(const std::vector<UpdateBufferInfo>& infos) {
        for (auto& set : descriptor_sets) {
            for (const auto& info : infos) {
                for (const auto& buffer : info.buffers) {
                    SingleUpdateBufferInfo single_info{}; {
                        single_info.buffer = buffer;
                        single_info.binding = info.binding;
                        single_info.type = info.type;
                    }

                    set.update(single_info);
                }
            }
        }
    }

    void DescriptorSet::update(const UpdateImageInfo& info) {
        for (auto& set : descriptor_sets) {
            set.update(info);
        }
    }

    void DescriptorSet::update(const SingleUpdateImageInfo& info) {
        for (auto& set : descriptor_sets) {
            set.update(info);
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

        main_pool = context().device.logical.createDescriptorPool(create_info);
    }

    vk::DescriptorPool main_descriptor_pool() {
        return main_pool;
    }
} // namespace aryibi::renderer