#include "command_buffer.hpp"
#include "descriptor_set.hpp"
#include "context.hpp"
#include "sampler.hpp"
#include "types.hpp"

#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <utility>
#include <vector>
#include <limits>
#include <array>

#define load_instance_function(f) PFN_##f f = reinterpret_cast<PFN_##f>(ctx.instance.getProcAddr(#f))

namespace aryibi::renderer {
    static Context ctx{};
    static VkSurfaceKHR surface{};

    void initialise(GLFWwindow* window) {
        /* Instance */ {
            vk::ApplicationInfo application_info{}; {
                application_info.applicationVersion = VK_API_VERSION_1_1;
                application_info.engineVersion = VK_API_VERSION_1_1;
                application_info.apiVersion = VK_API_VERSION_1_1;
                application_info.pApplicationName = "";
                application_info.pEngineName = "Aryibi Rendering Engine";
            }

            u32 count{};
            auto required = glfwGetRequiredInstanceExtensions(&count);

            std::vector<const char*> extensions(required, required + count);
            extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    #if defined(ARYIBI_DEBUG)
            extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            const char* validation_layer = "VK_LAYER_KHRONOS_validation";
    #endif // ARYIBI_DEBUG

            vk::InstanceCreateInfo instance_create_info{}; {
                instance_create_info.pApplicationInfo = &application_info;
                instance_create_info.enabledExtensionCount = extensions.size();
                instance_create_info.ppEnabledExtensionNames = extensions.data();
    #if defined(ARYIBI_DEBUG)
                instance_create_info.enabledLayerCount = 1;
                instance_create_info.ppEnabledLayerNames = &validation_layer;
    #else
                instance_create_info.enabledLayerCount = 0;
                instance_create_info.ppEnabledLayerNames = nullptr;
    #endif // ARYIBI_DEBUG
            }

            ctx.instance = vk::createInstance(instance_create_info);
        }

    #if defined(ARYIBI_DEBUG)
        /* Validation */ {
            vk::DebugUtilsMessengerCreateInfoEXT create_info{}; {
                create_info.messageSeverity =
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
                create_info.messageType =
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
                create_info.pfnUserCallback = [](
                    VkDebugUtilsMessageSeverityFlagBitsEXT           severity,
                    VkDebugUtilsMessageTypeFlagsEXT                  type,
                    const VkDebugUtilsMessengerCallbackDataEXT*      callback_data,
                    void*) -> VkBool32 {

                        auto severity_str = vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity));
                        auto type_str = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(type));

                        std::printf("[Vulkan] [%s/%s]: %s\n",
                            type_str.c_str(),
                            severity_str.c_str(),
                            callback_data->pMessage);

                        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                            std::abort();
                        }

                        return 0;
                };
            }

            struct Dispatcher {
                load_instance_function(vkCreateDebugUtilsMessengerEXT);
            } dispatcher{};

            ctx.validation = ctx.instance.createDebugUtilsMessengerEXT(create_info, nullptr, dispatcher);
        }
    #endif // ARYIBI_DEBUG

        /* Device */ {
            auto physical_devices = ctx.instance.enumeratePhysicalDevices();

            ctx.device.physical = *std::find_if(physical_devices.begin(), physical_devices.end(), [](const vk::PhysicalDevice& device) {
                auto properties = device.getProperties();
                auto features = device.getFeatures();

                if ((properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
                    properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu ||
                    properties.deviceType == vk::PhysicalDeviceType::eVirtualGpu) &&

                    features.samplerAnisotropy &&
                    features.multiDrawIndirect) {

                    auto major = VK_VERSION_MAJOR(properties.apiVersion);
                    auto minor = VK_VERSION_MINOR(properties.apiVersion);
                    auto patch = VK_VERSION_PATCH(properties.apiVersion);

                    std::printf("Device: %s\n", properties.deviceName.data());
                    std::printf("Vulkan: %d.%d.%d\n", major, minor, patch);

                    return true;
                }

                return false;
            });

            auto queue_family_properties = ctx.device.physical.getQueueFamilyProperties();

            for (u32 i = 0; i < queue_family_properties.size(); ++i) {
                if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    ctx.device.physical.getSurfaceSupportKHR(i, acquire_surface(window))) {
                    ctx.device.family = i;
                }
            }

            auto device_extensions = ctx.device.physical.enumerateDeviceExtensionProperties();

            constexpr std::array enabled_exts{
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
            };

            if (!std::all_of(enabled_exts.begin(), enabled_exts.end(), [&device_extensions](const char* required_name) {
                return std::any_of(device_extensions.begin(), device_extensions.end(), [required_name](const vk::ExtensionProperties& properties) {
                        return std::strcmp(properties.extensionName, required_name) == 0;
                    });
                })) {
                throw std::runtime_error("Required device extension not supported");
            }

            float priority = 1.0f;

            vk::DeviceQueueCreateInfo queue_create_info{}; {
                queue_create_info.queueCount = 1;
                queue_create_info.queueFamilyIndex = ctx.device.family;
                queue_create_info.pQueuePriorities = &priority;
            }

            vk::PhysicalDeviceFeatures features{}; {
                features.samplerAnisotropy = true;
                features.multiDrawIndirect = true;
                features.sampleRateShading = true;
            }

            // Rip descriptor indexing
            /*vk::PhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{}; {
                descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = true;
                descriptor_indexing_features.descriptorBindingVariableDescriptorCount = true;
                descriptor_indexing_features.descriptorBindingPartiallyBound = true;
                descriptor_indexing_features.runtimeDescriptorArray = true;
            }*/

            vk::DeviceCreateInfo device_create_info{}; {
                // device_create_info.pNext = &descriptor_indexing_features;
                device_create_info.ppEnabledExtensionNames = enabled_exts.data();
                device_create_info.enabledExtensionCount = enabled_exts.size();
                device_create_info.pQueueCreateInfos = &queue_create_info;
                device_create_info.queueCreateInfoCount = 1;
                device_create_info.pEnabledFeatures = &features;
            }

            ctx.device.logical = ctx.device.physical.createDevice(device_create_info);
            ctx.device.graphics = ctx.device.logical.getQueue(ctx.device.family, 0);
        }

        /* VmaAllocator */ {
            VmaAllocatorCreateInfo allocator_create_info{}; {
                allocator_create_info.flags = {};
                allocator_create_info.instance = static_cast<VkInstance>(ctx.instance);
                allocator_create_info.device = static_cast<VkDevice>(ctx.device.logical);
                allocator_create_info.physicalDevice = static_cast<VkPhysicalDevice>(ctx.device.physical);
                allocator_create_info.pHeapSizeLimit = nullptr;
                allocator_create_info.pRecordSettings = nullptr;
                allocator_create_info.pAllocationCallbacks = nullptr;
                allocator_create_info.pDeviceMemoryCallbacks = nullptr;
                allocator_create_info.frameInUseCount = 1;
                allocator_create_info.preferredLargeHeapBlockSize = 0;
                allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_1;
            }

            if (vmaCreateAllocator(&allocator_create_info, &ctx.allocator) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create allocator");
            }
        }

        /* Rest */ {
            make_command_pools();
            make_descriptor_pool();
            make_samplers();
        }
    }

    vk::SurfaceKHR acquire_surface(GLFWwindow* window) {
        if (!surface) {
            glfwCreateWindowSurface(static_cast<VkInstance>(ctx.instance), window, nullptr, &surface);
        }
        return surface;
    }

    void make_surface(GLFWwindow* window) {
        if (surface) {
            ctx.instance.destroy(surface);
        }
        glfwCreateWindowSurface(static_cast<VkInstance>(ctx.instance), window, nullptr, &surface);
    }

    const Context& context() {
        return ctx;
    }
} // namespace aryibi::renderer