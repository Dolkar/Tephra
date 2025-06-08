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
    struct SwapchainImage {
        std::unique_ptr<Image> imageOwner;
        ImageView imageView;
        // Present semaphores must be tied to each image since we do not know the order of acquired images
        Lifeguard<VkSemaphoreHandle> presentSemaphore;
    };

    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;

    Lifeguard<VkSwapchainHandleKHR> swapchainHandle;
    std::vector<SwapchainImage> images;
    std::vector<Lifeguard<VkSemaphoreHandle>> acquireSemaphores;
    uint32_t nextFreeAcquireSemaphoreIndex = 0;
    SwapchainStatus status;

    void setupSwapchainImages(const SwapchainSetup& setup, ArrayParameter<const VkImageHandle> vkSwapchainImageHandles);

    void setupAcquireSemaphores(uint64_t imagesCount);
};

}
