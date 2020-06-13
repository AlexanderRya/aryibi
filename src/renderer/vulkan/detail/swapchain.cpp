#include "swapchain.hpp"
#include "context.hpp"

namespace aryibi::renderer {
    void Swapchain::destroy() {
        for (const auto& view : views) {
            context().device.logical.destroy(view);
        }
        views.clear();
        images.clear();

        context().device.logical.destroy(handle);
    }
} // namespace aryibi::renderer