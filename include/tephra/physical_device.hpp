#pragma once

#include <tephra/version.hpp>
#include <tephra/format.hpp>
#include <tephra/memory.hpp>
#include <tephra/tools/structure_map.hpp>
#include <tephra/common.hpp>
#include <shared_mutex>
#include <mutex>

namespace tp {

/// Collection of device extensions that are either specific to Tephra, or are Vulkan device
/// extensions with built-in support in Tephra. Vulkan extensions outside of the ones defined here
/// may be used, but their support may be limited.
/// @remarks
///     Most Vulkan extensions also have their associated feature struct with features that also need to be enabled
///     to use their functionality. For extensions defined here, this is done for you.
/// @see tp::DeviceSetup
namespace DeviceExtension {
    /// Allows the creation and use of tp::Swapchain to display images onto a @vksymbol{VkSurface} object.
    /// Requires the tp::ApplicationExtension::KHR_Surface extension to be present and enabled.
    /// @see @vksymbol{VK_KHR_swapchain}
    const char* const KHR_Swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    /// Allows the use of non-empty parameter `imageCompatibleFormats` in tp::SwapchainSetup to create a tp::Swapchain
    /// object whose images can be viewed with different formats than what they were created as.
    /// @see @vksymbol{VK_KHR_swapchain_mutable_format}
    const char* const KHR_SwapchainMutableFormat = VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME;
    const char* const KHR_AccelerationStructure = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
    const char* const KHR_RayQuery = VK_KHR_RAY_QUERY_EXTENSION_NAME;
    /// Adds support for querying the actual amount of memory used by the process as well as the estimated budget of
    /// how much total memory the current process can use at any given time.
    /// @see tp::MemoryHeapStatistics.
    /// @see @vksymbol{VK_EXT_memory_budget}
    const char* const EXT_MemoryBudget = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
}

/// The named vendor of a physical device.
/// @see tp::PhysicalDevice::vendor
enum class DeviceVendor {
    Unknown, // PCI ids:
    AMD, // 0x1002
    NVIDIA, // 0x10DE
    INTEL, // 0x8086
    ARM, // 0x13B5
    ImgTec, // 0x1010
    Qualcomm, // 0x5143
    Apple, // 0x106B
};

/// The type of a device queue, defining the operations supported on it as well as its performance characteristics.
/// @see @vksymbol{VkQueueFlagBits}
enum class QueueType : uint32_t {
    /// An invalid queue type.
    Undefined,
    /// A queue type that only supports transfer operations.
    /// @remarks
    ///     Devices can typically execute commands submitted to transfer queues asynchronously to commands in other
    ///     queues. The transfer speeds may be lower, however. It is recommended to use transfer queues for copying
    ///     low priority data asynchronously.
    Transfer,
    /// A queue type that supports compute and transfer operations.
    /// @remarks
    ///     Devices can typically execute commands submitted to compute queues asynchronously to commands in other
    ///     compute and graphics queues. The resources are shared, however, and profiling is recommended.
    Compute,
    /// A queue type that supports graphics, compute and transfer operations.
    /// @remarks
    ///     A queue of this type is not guaranteed to be supported.
    /// @remarks
    ///     Devices typically don't benefit from submitting commands to multiple graphics queues in parallel.
    Graphics,
    /// An external queue type not managed by Vulkan.
    /// @remarks
    ///     Queues of this type cannot be created. The only valid use of this type is for the `targetQueueType`
    ///     parameter of tp::Job::cmdExportResource.
    External
};
TEPHRA_MAKE_CONTIGUOUS_ENUM_VIEW(QueueTypeEnumView, QueueType, External);

/// Information about the physical device queues for a particular queue type.
/// @see @vksymbol{VkQueueFamilyProperties}
struct QueueTypeInfo {
    /// The Vulkan physical device queue family index that the queue type maps to.
    /// @see @vksymbol{vkGetPhysicalDeviceQueueFamilyProperties}
    uint32_t queueFamilyIndex;
    /// The number of queues of this type exposed by the device in the chosen family.
    uint32_t queueCount;
    /// For queues of type tp::QueueType::Transfer defines the required offset alignment and size of any transfer
    /// operations performed on images. If (0,0,0), only whole mip levels must be transferred.
    Extent3D minImageTransferGranularity;
};

/// Information about the physical device memory location.
/// @see @vksymbol{VkPhysicalDeviceMemoryProperties}
struct MemoryLocationInfo {
    /// The Vulkan memory heap index that the memory location maps to, or ~0 if not available on the device.
    uint32_t memoryHeapIndex;
    /// The Vulkan memory type index that the memory location maps to, or ~0 if not available on the device.
    uint32_t memoryTypeIndex;
    /// The total size in bytes exposed by the device for this memory location.
    uint64_t sizeBytes;
};

/// Describes the capabilities of a @vksymbol{VkSurfaceKHR} and the capabilities of a physical device to present to
/// that surface.
/// @see @vksymbol{VkSurfaceCapabilitiesKHR}
struct SurfaceCapabilities {
    /// An array of queue types that support present operations for this surface.
    ArrayView<QueueType> supportedQueueTypes;
    /// An array of supported present modes. The FIFO mode is required to be supported.
    ArrayView<PresentMode> supportedPresentModes;
    /// An array of formats supported by the surface for the sRGB color space.
    ArrayView<Format> supportedFormatsSRGB;
    /// The minimum allowed number of swapchain images.
    uint32_t minImageCount;
    /// The maximum allowed number of swapchain images, or 0 if there is no limit.
    uint32_t maxImageCount;
    /// The current extent of the surface, or (~0, ~0) if it will be determined by the swapchain.
    Extent2D currentExtent;
    /// The minimum allowed image extent.
    Extent2D minImageExtent;
    /// The maximum allowed image extent.
    Extent2D maxImageExtent;
    /// The maximum allowed number of image array layers.
    uint32_t maxImageArrayLayers;
    /// The current transform of the surface.
    SurfaceTransform currentTransform;
    /// A mask of all supported transforms of the surface.
    SurfaceTransformMask supportedTransforms;
    /// A mask of supported composite alpha modes.
    CompositeAlphaMask supportedCompositeAlphas;
    /// A mask of supported image usages. ColorAttachment usage is always supported.
    ImageUsageMask supportedImageUsages;

    /// Returns `true` if the surface is supported and a swapchain can be created for it.
    bool isSupported() const;
};

class VulkanPhysicalDeviceInterface;
class VulkanPhysicalDeviceSurfaceInterfaceKHR;

/// Represents a read-only interface for the physical device for identification and querying its properties and
/// capabilities.
/// @see tp::Application::getPhysicalDevices
/// @see @vksymbol{VkPhysicalDevice}
class PhysicalDevice {
public:
    PhysicalDevice(
        const VulkanPhysicalDeviceInterface& vkiPhysicalDevice,
        const VulkanPhysicalDeviceSurfaceInterfaceKHR& vkiSurface,
        VkPhysicalDeviceHandle vkPhysicalDeviceHandle);

    /// The human readable name of the device.
    const char* name;
    /// The type of the device, hinting at its performance characteristics.
    DeviceType type;
    /// The device vendor, if identified. Otherwise, see `vendorID`.
    DeviceVendor vendor;
    /// The device vendor ID.
    uint32_t vendorID;
    /// A universally unique identifier for the device. Can be used for identifying pipeline caches as they can only
    /// be used with a particular device.
    std::byte pipelineCacheUUID[VK_UUID_SIZE];
    /// The highest Vulkan API version that the device implements.
    Version apiVersion;
    /// The driver version number.
    Version driverVersion;

    /// Returns the details of a particular queue type and what Vulkan queue family it maps to.
    QueueTypeInfo getQueueTypeInfo(QueueType type) const;

    /// Returns the details of the given memory location and what Vulkan memory types and heaps it maps to.
    MemoryLocationInfo getMemoryLocationInfo(MemoryLocation location) const;

    /// Returns `true` when the device extension is available on the device and can be enabled.
    bool isExtensionAvailable(const char* extension) const;

    /// Returns the capabilities for the given tp::Format.
    FormatCapabilities queryFormatCapabilities(Format format) const;

    /// Returns the capabilities of the device relating to the given @vksymbol{VkSurfaceKHR} Vulkan handle.
    SurfaceCapabilities querySurfaceCapabilitiesKHR(VkSurfaceKHR vkSurface) const;

    /// Queries the device for features defined in the Vulkan feature structure given as the template argument, for
    /// example `VkPhysicalDeviceFeatures`.
    /// @remarks
    ///     If the structure depends on some extension, it should first be confirmed that the extension is available
    ///     with tp::PhysicalDevice::isExtensionAvailable.
    template <typename T>
    const T& vkQueryFeatures() const;

    /// Queries the device for properties defined in the Vulkan property structure given as the template argument,
    /// for example `VkPhysicalDeviceProperties`.
    /// @remarks
    ///     If the structure depends on some extension, it should first be confirmed that the extension is available
    ///     with tp::PhysicalDevice::isExtensionAvailable.
    template <typename T>
    const T& vkQueryProperties() const;

    /// Returns the associated Vulkan @vksymbol{VkPhysicalDevice} handle.
    VkPhysicalDeviceHandle vkGetPhysicalDeviceHandle() const {
        return vkPhysicalDeviceHandle;
    }

    TEPHRA_MAKE_NONCOPYABLE(PhysicalDevice);
    TEPHRA_MAKE_MOVABLE(PhysicalDevice);
    virtual ~PhysicalDevice();

private:
    struct PhysicalDeviceDataCache;

    VkPhysicalDeviceHandle vkPhysicalDeviceHandle;
    const VulkanPhysicalDeviceInterface* vkiPhysicalDevice;
    const VulkanPhysicalDeviceSurfaceInterfaceKHR* vkiSurface;

    mutable std::unique_ptr<PhysicalDeviceDataCache> dataCache;

    std::shared_lock<std::shared_mutex> acquireStructuresReadLock() const;
    std::unique_lock<std::shared_mutex> acquireStructuresWriteLock() const;

    VkFeatureMap& getFeatureStructureMap() const;
    VkPropertyMap& getPropertyStructureMap() const;

    void vkQueryFeatureStruct(void* structPtr) const;
    void vkQueryPropertyStruct(void* structPtr) const;
};

template <typename T>
const T& PhysicalDevice::vkQueryFeatures() const {
    VkFeatureMap& featureMap = getFeatureStructureMap();
    T* vkFeatures = nullptr;
    {
        auto readLock = acquireStructuresReadLock();
        if (featureMap.contains<T>()) {
            vkFeatures = &featureMap.get<T>();
        }
    }
    if (vkFeatures == nullptr) {
        auto writeLock = acquireStructuresWriteLock();
        vkFeatures = &featureMap.get<T>();
        vkQueryFeatureStruct(static_cast<void*>(vkFeatures));
    }
    return *vkFeatures;
}

template <typename T>
const T& PhysicalDevice::vkQueryProperties() const {
    VkPropertyMap& propertyMap = getPropertyStructureMap();
    T* vkProperties = nullptr;
    {
        auto readLock = acquireStructuresReadLock();
        if (propertyMap.contains<T>()) {
            vkProperties = &propertyMap.get<T>();
        }
    }
    if (vkProperties == nullptr) {
        auto writeLock = acquireStructuresWriteLock();
        vkProperties = &propertyMap.get<T>();
        vkQueryPropertyStruct(static_cast<void*>(vkProperties));
    }
    return *vkProperties;
}

}
