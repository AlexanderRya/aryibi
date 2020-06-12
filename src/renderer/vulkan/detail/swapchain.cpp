#include "swapchain.hpp"
#include "context.hpp"

namespace aryibi::renderer {
    void Swapchain::destroy() {
        for (const auto& view : views) {
            context().device.logical.destroyImageView(view);
        }
        views.clear();
        images.clear();

        context().device.logical.destroySwapchainKHR(handle);
    }
} // namespace aryibi::renderer