#pragma once

#include <tephra/job.hpp>
#include <tephra/image.hpp>
#include <tephra/semaphore.hpp>
#include <tephra/common.hpp>
#include <optional>

namespace tp {

/// Used as configuration for creating a new tp::Swapchain object.
/// @see tp::Device::createSwapchainKHR
/// @see @vksymbol{VkSwapchainCreateInfoKHR}
struct SwapchainSetup {
    VkSurfaceHandleKHR vkSurfaceHandle;
    PresentMode presentMode;
    uint32_t minImageCount;
    ImageUsageMask imageUsage;
    Format imageFormat;
    Extent2D imageExtent;
    uint32_t imageArrayLayerCount;
    ArrayView<Format> imageCompatibleFormatsKHR;
    bool clipped;
    SurfaceTransform preTransform;
    CompositeAlpha compositeAlpha;
    SwapchainFlagMask flags;
    void* vkCreateInfoExtPtr;

    /// @param vkSurfaceHandle
    ///     The surface onto which images will be presented.
    /// @param presentMode
    ///     Determines how present operations will be processed and queued internally.
    /// @param minImageCount
    ///     The minimum number of swapchain images that the application needs. More images can be created if
    ///     necessary.
    /// @param imageUsage
    ///     A mask of tp::ImageUsage specifying the permitted set of usages of the swapchain images.
    /// @param imageFormat
    ///     The format the data will be interpreted as.
    /// @param imageExtent
    ///     The extent of the swapchain images in two dimensions.
    /// @param imageArrayLayerCount
    ///     The number of array layers the swapchain images should have.
    /// @param imageCompatibleFormatsKHR
    ///     The list of additional compatible formats that the tp::ImageView objects viewing the swapchain images are
    ///     permitted to have. Requires DeviceExtension::KHR_SwapchainMutableFormat.
    /// @param clipped
    ///     Specifies whether the implementation is allowed to discard rendering operations that affect regions of the
    ///     surface that are not visible.
    /// @param preTransform
    ///     Describes the transform applied to image content prior to presentation.
    /// @param compositeAlpha
    ///     The alpha compositing mode.
    /// @param flags
    ///     Additional flags for creation of the swapchain.
    /// @param vkCreateInfoExtPtr
    ///     A pointer to additional Vulkan structures to be passed in `pNext` of @vksymbol{VkSwapchainCreateInfoKHR}.
    /// @remarks
    ///     The values that can be provided to create a valid Swapchain object must conform to the surface capabilities
    ///     queried by tp::PhysicalDevice::querySurfaceCapabilitiesKHR for the used Vulkan surface handle.
    SwapchainSetup(
        VkSurfaceKHR vkSurfaceHandle,
        PresentMode presentMode,
        uint32_t minImageCount,
        ImageUsageMask imageUsage,
        Format imageFormat,
        Extent2D imageExtent,
        uint32_t imageArrayLayerCount = 1,
        ArrayView<Format> imageCompatibleFormatsKHR = {},
        bool clipped = true,
        SurfaceTransform preTransform = SurfaceTransform::UseCurrentTransform,
        CompositeAlpha compositeAlpha = CompositeAlpha::Opaque,
        SwapchainFlagMask flags = SwapchainFlagMask::None(),
        void* vkCreateInfoExtPtr = nullptr);
};

/// Describes the details of an image acquired from a tp::Swapchain and its required synchronization
/// semaphores with the acquire and present operations.
struct AcquiredImageInfo {
    /// The view of the swapchain image that was acquired.
    const ImageView* image;
    /// The index of the acquired image within the swapchain.
    uint32_t imageIndex;
    /// The semaphore for the Job accessing the swapchain image to wait upon.
    ExternalSemaphore acquireSemaphore;
    /// The semaphore to be signalled by a Job and waited upon by the present operation.
    ExternalSemaphore presentSemaphore;
};

/// Describes the status of a tp::Swapchain object and/or the reason why the last operation may have failed.
enum class SwapchainStatus {
    /// The swapchain is ready to be used for presentation and it matches surface properties.
    Optimal,
    /// The swapchain is ready to be used for presentation, but it does not match surface properties exactly.
    /// The user may choose to recreate the swapchain when it's convenient.
    Suboptimal,
    /// The underlying surface has changed in such a way that it is no longer compatible with the swapchain.
    /// Further presentation requests will fail.
    OutOfDate,
    /// The underlying surface has been lost and needs to be recreated, along with the swapchain.
    /// Further presentation requests will fail.
    SurfaceLost,
    /// Indicates that the swapchain has been used to create a new swapchain and that already acquired images
    /// can still be presented, but new images cannot be acquired.
    Retired
};

/// Manages presenting the rendering results to a surface.
/// @remarks
///     On either of the acquire or present operations, the status of the swapchain can change.
///     This indicates whether the swapchain needs to be recreated and which operations are supported.
/// @remarks
///     The usual workflow for presenting an image is as follows:
///         1. Acquire the next image through tp::Swapchain::acquireNextImage, receiving the synchronization
///            primitives as well as the image itself.
///         TODO: Clarivy that we don't actually need to discard
///         2. In a tp::Job, call tp::Job::cmdDiscardContents on the acquired image, unless its previous contents
///            are needed.
///         3. Populate the swapchain image with data for the next frame.
///         4. Export the swapchain image with tp::Job::cmdExportResource, specifying the
///            tp::ReadAccess::ImagePresentKHR read access and the queue the present operation will take place in
///            if it's different from the queue the job will be submitted to.
///         5. Submit the job(s) accessing the swapchain image such that the first one waits on the
///            tp::AcquiredImageInfo::acquireSemaphore external semaphore and the last one signals the
///            tp::AcquiredImageInfo::presentSemaphore semaphore.
///         6. Present the image by calling tp::Device::submitPresentImagesKHR with the matching swapchain
///            and image index.
/// @see tp::Device::createSwapchainKHR
/// @see @vksymbol{VkSwapchainKHR}
class Swapchain : public Ownable {
public:
    /// Acquires the next available image in the swapchain.
    /// @param timeout
    ///     The time limit for waiting.
    /// @returns
    ///     The details of the acquired image, unless the operation timed out.
    std::optional<AcquiredImageInfo> acquireNextImage(Timeout timeout = Timeout::Indefinite());

    /// Returns the current status of the swapchain.
    SwapchainStatus getStatus() const;

    /// Returns the Vulkan @vksymbol{VkSwapchainKHR} handle.
    VkSwapchainHandleKHR vkGetSwapchainHandle() const;

    TEPHRA_MAKE_INTERFACE(Swapchain);

protected:
    Swapchain() {}
};

}
