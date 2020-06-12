#include "command_buffer.hpp"
#include "raw_buffer.hpp"
#include "texture.hpp"
#include "sampler.hpp"

#include "stb_image.h"

#include <fstream>

namespace aryibi::renderer {
    Texture load_texture(const std::string& path, const vk::Format format) {
        if (!std::ifstream(path).is_open()) {
            throw std::runtime_error("File not found error at: " + path);
        }

        i32 width, height, channels = 4;

        auto data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        auto texture = load_texture(data, width, height, 4, format);
        stbi_image_free(data);

        return texture;
    }

    Texture load_texture(const u8* data, const u32 width, const u32 height, const u32 channels, const vk::Format format) {
        Texture texture{};

        if (!data) {
            throw std::runtime_error("Error, can't load texture without data");
        }

        if (width <= 0 || height <= 0 || channels <= 0) {
            throw std::runtime_error("wtf are you doing");
        }

        auto texture_size = width * height * channels;

        Image::CreateInfo create_info{}; {
            create_info.width = width;
            create_info.height = height;
            create_info.mips = 1;
            create_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
            create_info.format = format;
            create_info.aspect = vk::ImageAspectFlagBits::eColor;
            create_info.tiling = vk::ImageTiling::eOptimal;
            create_info.samples = vk::SampleCountFlagBits::e1;
        }
        texture.image = make_image(create_info);
        copy_data_to_local(data, texture_size, texture.image);
        transition_image_layout(texture.image.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, texture.image.mips);

        return texture;
    }

    vk::DescriptorImageInfo Texture::info(const vk::Sampler sampler) const {
        vk::DescriptorImageInfo image_info{}; {
            image_info.sampler = sampler;
            image_info.imageView = image.view;
            image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        }
        return image_info;
    }
} // namespace aryibi::renderer