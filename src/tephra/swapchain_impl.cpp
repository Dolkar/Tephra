
#include "swapchain_impl.hpp"
#include "image_impl.hpp"
#include "device/device_container.hpp"

namespace tp {

Timeout Timeout::Seconds(float seconds) {
    return Timeout(safeRoundToUint(seconds * 1.0e9f));
}

Timeout Timeout::Milliseconds(float milliseconds) {
    return Timeout(safeRoundToUint(milliseconds * 1.0e6f));
}

SwapchainSetup::SwapchainSetup(
    VkSurfaceKHR vkSurfaceHandle,
    PresentMode presentMode,
    uint32_t minImageCount,
    ImageUsageMask imageUsage,
    Format imageFormat,
    Extent2D imageExtent,
    uint32_t imageArrayLayerCount,
    ArrayView<Format> imageCompatibleFormatsKHR,
    bool clipped,
    SurfaceTransform preTransform,
    CompositeAlpha compositeAlpha,
    SwapchainFlagMask flags,
    void* vkCreateInfoExtPtr)
    : vkSurfaceHandle(vkSurfaceHandle),
      presentMode(presentMode),
      minImageCount(minImageCount),
      imageUsage(imageUsage),
      imageFormat(imageFormat),
      imageExtent(imageExtent),
      imageArrayLayerCount(imageArrayLayerCount),
      imageCompatibleFormatsKHR(imageCompatibleFormatsKHR),
      clipped(clipped),
      preTransform(preTransform),
      compositeAlpha(compositeAlpha),
      vkCreateInfoExtPtr(vkCreateInfoExtPtr),
      flags(flags) {}

SwapchainImpl::SwapchainImpl(
    DeviceContainer* deviceImpl,
    const SwapchainSetup& setup,
    Lifeguard<VkSwapchainHandleKHR> swapchainHandle,
    ArrayParameter<const VkImageHandle> vkSwapchainImageHandles,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      deviceImpl(deviceImpl),
      swapchainHandle(std::move(swapchainHandle)),
      status(SwapchainStatus::Optimal) {
    setupSwapchainImages(setup, vkSwapchainImageHandles);
    setupAcquireSemaphores(vkSwapchainImageHandles.size());
}

std::optional<AcquiredImageInfo> SwapchainImpl::acquireNextImage_(Timeout timeout) {
    // Round-robin acquire semaphores
    VkSemaphoreHandle acquireSemaphore = acquireSemaphores[nextFreeAcquireSemaphoreIndex++].vkGetHandle();
    if (nextFreeAcquireSemaphoreIndex == acquireSemaphores.size())
        nextFreeAcquireSemaphoreIndex = 0;

    VkResult result;
    uint32_t imageIndex;
    try {
        // On some platforms the acquire can return with VK_NOT_READY even when timeout is nonzero.
        // When the user specifies indefinite timeout, make sure we only return when the acquire is
        // actually successful or an error is thrown.
        do {
            result = deviceImpl->getLogicalDevice()->acquireNextImageKHR(
                swapchainHandle.vkGetHandle(), timeout, acquireSemaphore, &imageIndex);
        } while (timeout.isIndefinite() && (result == VK_NOT_READY || result == VK_TIMEOUT));
    } catch (const OutOfDateError&) {
        status = SwapchainStatus::OutOfDate;
        throw;
    } catch (const SurfaceLostError&) {
        status = SwapchainStatus::SurfaceLost;
        throw;
    }

    if (result == VK_TIMEOUT || result == VK_NOT_READY) {
        return {};
    } else if (result == VK_SUBOPTIMAL_KHR && status == SwapchainStatus::Optimal) {
        status = SwapchainStatus::Suboptimal;
    }

    SwapchainImage& acquiredImage = images[imageIndex];

    AcquiredImageInfo acquireInfo;
    acquireInfo.image = &acquiredImage.imageView;
    acquireInfo.imageIndex = imageIndex;
    // The two semaphores are binary semaphores, so second argument is 0
    acquireInfo.acquireSemaphore = { acquireSemaphore, 0 };
    acquireInfo.presentSemaphore = { acquiredImage.presentSemaphore.vkGetHandle(), 0 };

    return acquireInfo;
}

void SwapchainImpl::retire() {
    status = SwapchainStatus::Retired;
}

void SwapchainImpl::submitPresentImages(
    DeviceContainer* deviceImpl,
    uint32_t queueIndex,
    ArrayParameter<Swapchain* const> swapchains,
    ArrayParameter<const uint32_t> imageIndices) {
    ScratchVector<VkSwapchainHandleKHR> vkSwapchainHandles;
    vkSwapchainHandles.reserve(swapchains.size());
    ScratchVector<VkSemaphoreHandle> vkWaitSemaphoreHandles;
    vkWaitSemaphoreHandles.reserve(swapchains.size());

    for (std::size_t i = 0; i < swapchains.size(); i++) {
        SwapchainImpl* swapchainImpl = static_cast<SwapchainImpl*>(swapchains[i]);
        uint32_t presentImageIndex = imageIndices[i];
        SwapchainImage& imageToPresent = swapchainImpl->images[imageIndices[i]];

        vkSwapchainHandles.push_back(swapchainImpl->swapchainHandle.vkGetHandle());
        vkWaitSemaphoreHandles.push_back(imageToPresent.presentSemaphore.vkGetHandle());
    }

    ScratchVector<VkResult> vkResults(swapchains.size());
    auto updateSwapchainStatuses = [&]() {
        for (std::size_t i = 0; i < swapchains.size(); i++) {
            SwapchainImpl* swapchainImpl = static_cast<SwapchainImpl*>(swapchains[i]);
            switch (vkResults[i]) {
            case VK_ERROR_OUT_OF_DATE_KHR:
                swapchainImpl->status = SwapchainStatus::OutOfDate;
                break;
            case VK_ERROR_SURFACE_LOST_KHR:
                swapchainImpl->status = SwapchainStatus::SurfaceLost;
                break;
            case VK_SUBOPTIMAL_KHR:
                if (swapchainImpl->status == SwapchainStatus::Optimal) {
                    swapchainImpl->status = SwapchainStatus::Suboptimal;
                }
                break;
            }
        }
    };

    try {
        deviceImpl->getLogicalDevice()->queuePresentKHR(
            queueIndex, view(vkSwapchainHandles), imageIndices, view(vkWaitSemaphoreHandles), view(vkResults));
    } catch (const RuntimeError&) {
        updateSwapchainStatuses();
        throw;
    }

    updateSwapchainStatuses();
}

std::string formSwapchainResourceName(const char* swapchainName, const char* resourceName, uint64_t index) {
    return "Swapchain " + (swapchainName == nullptr ? "" : "'" + std::string(swapchainName) + "' ") + resourceName +
        " " + std::to_string(index);
}

void SwapchainImpl::setupSwapchainImages(
    const SwapchainSetup& setup,
    ArrayParameter<const VkImageHandle> vkSwapchainImageHandles) {
    for (std::size_t i = 0; i < vkSwapchainImageHandles.size(); i++) {
        auto imageHandleLifeguard = Lifeguard<VkImageHandle>::NonOwning(vkSwapchainImageHandles[i]);
        ImageSetup imageSetup = ImageSetup(
            ImageType::Image2D,
            setup.imageUsage,
            setup.imageFormat,
            Extent3D(setup.imageExtent.width, setup.imageExtent.height, 1),
            1,
            setup.imageArrayLayerCount,
            MultisampleLevel::x1,
            setup.imageCompatibleFormatsKHR);

        SwapchainImage image;
        std::string imageName = formSwapchainResourceName(debugTarget.getObjectName(), "image", i);
        auto imageDebugTarget = DebugTarget(&debugTarget, "Image", imageName.c_str());
        image.imageOwner = std::make_unique<ImageImpl>(
            deviceImpl,
            imageSetup,
            std::move(imageHandleLifeguard),
            Lifeguard<VmaAllocationHandle>(),
            std::move(imageDebugTarget));
        deviceImpl->getLogicalDevice()->setObjectDebugName(vkSwapchainImageHandles[i], imageName.c_str());
        image.imageView = image.imageOwner->getDefaultView();

        image.presentSemaphore = deviceImpl->vkMakeHandleLifeguard(
            deviceImpl->getLogicalDevice()->createSemaphore(false));

        std::string semaphoreName = formSwapchainResourceName(debugTarget.getObjectName(), "present semaphore", i);
        deviceImpl->getLogicalDevice()->setObjectDebugName(image.presentSemaphore.vkGetHandle(), semaphoreName.c_str());
        images.push_back(std::move(image));
    }
}

void SwapchainImpl::setupAcquireSemaphores(uint64_t imagesCount) {
    // Use one more acquire semaphore than images to prevent unnecessary waiting
    uint64_t syncCount = imagesCount + 1;
    for (uint64_t i = 0; i < syncCount; i++) {
        Lifeguard<VkSemaphoreHandle> acquireSemaphore = deviceImpl->vkMakeHandleLifeguard(
            deviceImpl->getLogicalDevice()->createSemaphore(false));

        std::string semaphoreName = formSwapchainResourceName(debugTarget.getObjectName(), "acquire semaphore", i);
        deviceImpl->getLogicalDevice()->setObjectDebugName(acquireSemaphore.vkGetHandle(), semaphoreName.c_str());
        acquireSemaphores.push_back(std::move(acquireSemaphore));
    }
}

std::optional<AcquiredImageInfo> Swapchain::acquireNextImage(Timeout timeout) {
    SwapchainImpl* swapchainImpl = static_cast<SwapchainImpl*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(swapchainImpl->getDebugTarget(), "acquireNextImage", nullptr);
    return swapchainImpl->acquireNextImage_(timeout);
}

SwapchainStatus Swapchain::getStatus() const {
    const SwapchainImpl* swapchainImpl = static_cast<const SwapchainImpl*>(this);
    return swapchainImpl->getStatus_();
}

VkSwapchainHandleKHR Swapchain::vkGetSwapchainHandle() const {
    const SwapchainImpl* swapchainImpl = static_cast<const SwapchainImpl*>(this);
    return swapchainImpl->vkGetSwapchainHandle_();
}

}
