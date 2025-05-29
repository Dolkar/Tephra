#pragma once

#include "common_impl.hpp"
#include "device/queue_state.hpp"
#include <tephra/swapchain.hpp>

namespace tp {

class SwapchainImpl : public Swapchain {
public:
    SwapchainImpl(
        DeviceContainer* deviceImpl,
        const SwapchainSetup& setup,
        Lifeguard<VkSwapchainHandleKHR> swapchainHandle,
        ArrayParameter<const VkImageHandle> vkSwapchainImageHandles,
        DebugTarget debugTarget);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    std::optional<AcquiredImageInfo> acquireNextImage_(Timeout timeout);

    void retire();

    SwapchainStatus getStatus_() const {
        return status;
    }

    VkSwapchainHandleKHR vkGetSwapchainHandle_() const {
        return swapchainHandle.vkGetHandle();
    }

    TEPHRA_MAKE_NONCOPYABLE(SwapchainImpl);
    TEPHRA_MAKE_NONMOVABLE(SwapchainImpl);
    ~SwapchainImpl() = default;

    static void submitPresentImages(
        DeviceContainer* deviceImpl,
        uint32_t queueIndex,
        ArrayParameter<Swapchain* const> swapchains,
        ArrayParameter<const uint32_t> imageIndices);

private:
    struct SwapchainImageSync {
        uint32_t imageIndex;
        Lifeguard<VkSemaphoreHandle> acquireSemaphore;
        Lifeguard<VkSemaphoreHandle> presentSemaphore;
    };

    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;

    Lifeguard<VkSwapchainHandleKHR> swapchainHandle;
    std::vector<std::unique_ptr<Image>> swapchainImages;
    std::vector<ImageView> swapchainImageViews;
    SwapchainStatus status;

    std::deque<SwapchainImageSync> acquiredImageSync;
    std::deque<SwapchainImageSync> presentedImageSync;

    void setupSwapchainImages(const SwapchainSetup& setup, ArrayParameter<const VkImageHandle> vkSwapchainImageHandles);

    void setupSyncPrimitives(uint64_t imagesCount);
};

}
