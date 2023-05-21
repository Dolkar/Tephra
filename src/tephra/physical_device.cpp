
#include <tephra/physical_device.hpp>
#include <tephra/tools/structure_map.hpp>
#include "device/logical_device.hpp"
#include "device/memory_allocator.hpp"
#include "vulkan/interface.hpp"
#include "common_impl.hpp"

namespace tp {

constexpr const uint32_t QueueFamilyUndefined = ~0u;

bool SurfaceCapabilities::isSupported() const {
    return !supportedQueueTypes.empty() && maxImageExtent.width > 0 && maxImageExtent.height > 0;
}

struct SurfaceCapabilitiesCacheEntry {
    std::vector<QueueType> queueTypes;
    std::vector<PresentMode> presentModes;
    std::vector<Format> supportedFormatsSRGB;
};

struct PhysicalDevice::PhysicalDeviceDataCache {
    // Maps of generic feature and property structures
    std::shared_mutex structuresMutex;
    VkFeatureMap featureMap;
    VkPropertyMap propertyMap;

    // Storage of returned surface capabilities
    Mutex surfaceCapabilityCacheMutex;
    std::unordered_map<VkSurfaceHandleKHR, std::unique_ptr<SurfaceCapabilitiesCacheEntry>> surfaceCapabilityCache;

    // Items of data cache that are populated on initialization
    QueueTypeInfo queueTypeInfos[QueueTypeEnumView::size()];
    MemoryLocationInfo memoryLocationInfos[MemoryLocationEnumView::size()];
    std::vector<VkExtensionProperties> extensions;
};

DeviceVendor decodeDeviceVendor(uint32_t vendorID) {
    switch (vendorID) {
    case 0x1002:
        return DeviceVendor::AMD;
    case 0x10DE:
        return DeviceVendor::NVIDIA;
    case 0x8086:
        return DeviceVendor::INTEL;
    case 0x13B5:
        return DeviceVendor::ARM;
    case 0x1010:
        return DeviceVendor::ImgTec;
    case 0x5143:
        return DeviceVendor::Qualcomm;
    case 0x106B:
        return DeviceVendor::Apple;
    default:
        return DeviceVendor::Unknown;
    }
}

void assignQueueFamilies(
    ArrayView<const VkQueueFamilyProperties> queueProperties,
    uint32_t* graphicsFamilyIndex,
    uint32_t* computeFamilyIndex,
    uint32_t* transferFamilyIndex) {
    *graphicsFamilyIndex = ~0;
    *computeFamilyIndex = ~0;
    *transferFamilyIndex = ~0;

    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueProperties.size(); queueFamilyIndex++) {
        auto& queueFamilyProperties = queueProperties[queueFamilyIndex];

        if (containsAllBits(queueFamilyProperties.queueFlags, VK_QUEUE_GRAPHICS_BIT)) {
            if (*graphicsFamilyIndex == ~0)
                *graphicsFamilyIndex = queueFamilyIndex;
        } else if (containsAllBits(queueFamilyProperties.queueFlags, VK_QUEUE_COMPUTE_BIT)) {
            if (*computeFamilyIndex == ~0)
                *computeFamilyIndex = queueFamilyIndex;
        } else if (containsAllBits(queueFamilyProperties.queueFlags, VK_QUEUE_TRANSFER_BIT)) {
            if (*transferFamilyIndex == ~0)
                *transferFamilyIndex = queueFamilyIndex;
        }
    }

    if (*computeFamilyIndex == ~0) {
        // Reuse the main family queues for async compute, since there's no dedicated compute queue family
        *computeFamilyIndex = *graphicsFamilyIndex;
    }
    if (*transferFamilyIndex == ~0) {
        // Same deal, but maybe reuse the compute one?
        *transferFamilyIndex = *computeFamilyIndex;
    }

    TEPHRA_ASSERT(*graphicsFamilyIndex != ~0);
    TEPHRA_ASSERT(*computeFamilyIndex != ~0);
    TEPHRA_ASSERT(*transferFamilyIndex != ~0);
}

void assignMemoryLocations(
    const VkPhysicalDeviceMemoryProperties& vkMemoryProperties,
    ArrayParameter<MemoryLocationInfo> locationInfos) {
    for (size_t i = 0; i < locationInfos.size(); i++) {
        MemoryLocation location = static_cast<MemoryLocation>(i);
        MemoryLocationInfo& info = locationInfos[i];

        info.memoryHeapIndex = ~0;
        info.memoryTypeIndex = ~0;
        info.sizeBytes = 0;

        // Find the first viable memory type - the memory types by Vulkan spec must be sorted by capability and
        // performance in a way that the first viable type should always be chosen.
        for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < vkMemoryProperties.memoryTypeCount; memoryTypeIndex++) {
            const VkMemoryType& vkMemoryType = vkMemoryProperties.memoryTypes[memoryTypeIndex];
            MemoryLocation typeLocation = MemoryAllocator::memoryTypeFlagsToMemoryLocation(vkMemoryType.propertyFlags);

            if (typeLocation == location) {
                info.memoryHeapIndex = vkMemoryType.heapIndex;
                info.memoryTypeIndex = memoryTypeIndex;
                const VkMemoryHeap vkMemoryHeap = vkMemoryProperties.memoryHeaps[vkMemoryType.heapIndex];
                info.sizeBytes = vkMemoryHeap.size;
                break;
            }
        }
    }
}

PhysicalDevice::PhysicalDevice(
    const VulkanPhysicalDeviceInterface& vkiPhysicalDevice,
    const VulkanPhysicalDeviceSurfaceInterfaceKHR& vkiSurface,
    VkPhysicalDeviceHandle vkPhysicalDeviceHandle)
    : vkPhysicalDeviceHandle(vkPhysicalDeviceHandle),
      vkiPhysicalDevice(&vkiPhysicalDevice),
      vkiSurface(&vkiSurface),
      dataCache(std::make_unique<PhysicalDeviceDataCache>()) {
    // Initialize main properties
    auto& vkProperties2 = dataCache->propertyMap.get<VkPhysicalDeviceProperties2>();
    vkiPhysicalDevice.getPhysicalDeviceProperties2(vkPhysicalDeviceHandle, &vkProperties2);
    const VkPhysicalDeviceProperties& vkProperties = vkProperties2.properties;

    name = vkProperties.deviceName;
    type = vkCastConvertibleEnum(vkProperties.deviceType);
    vendor = decodeDeviceVendor(vkProperties.vendorID);
    vendorID = vkProperties.vendorID;
    memcpy(pipelineCacheUUID, vkProperties.pipelineCacheUUID, VK_UUID_SIZE);
    apiVersion = Version(vkProperties.apiVersion);
    driverVersion = Version(vkProperties.driverVersion);

    // Extract queue info and assign queue families
    ScratchVector<VkQueueFamilyProperties> vkQueueProperties;
    uint32_t familyCount;
    vkiPhysicalDevice.getPhysicalDeviceQueueFamilyProperties(vkPhysicalDeviceHandle, &familyCount, nullptr);
    vkQueueProperties.resize(familyCount);
    vkiPhysicalDevice.getPhysicalDeviceQueueFamilyProperties(
        vkPhysicalDeviceHandle, &familyCount, vkQueueProperties.data());
    vkQueueProperties.resize(familyCount);

    uint32_t queueFamilyIndices[QueueTypeEnumView::size()];
    queueFamilyIndices[static_cast<uint32_t>(QueueType::Undefined)] = QueueFamilyUndefined;
    queueFamilyIndices[static_cast<uint32_t>(QueueType::External)] = VK_QUEUE_FAMILY_EXTERNAL;
    assignQueueFamilies(
        view(vkQueueProperties),
        &queueFamilyIndices[static_cast<uint32_t>(QueueType::Graphics)],
        &queueFamilyIndices[static_cast<uint32_t>(QueueType::Compute)],
        &queueFamilyIndices[static_cast<uint32_t>(QueueType::Transfer)]);

    for (QueueType queueType : QueueTypeEnumView()) {
        uint32_t typeIndex = static_cast<uint32_t>(queueType);
        QueueTypeInfo& queueTypeInfo = dataCache->queueTypeInfos[typeIndex];
        queueTypeInfo = QueueTypeInfo{ 0 };

        queueTypeInfo.queueFamilyIndex = queueFamilyIndices[typeIndex];
        if (queueTypeInfo.queueFamilyIndex < vkQueueProperties.size()) {
            const VkQueueFamilyProperties& vkQueueFamilyProperties = vkQueueProperties[queueTypeInfo.queueFamilyIndex];
            queueTypeInfo.queueCount = vkQueueFamilyProperties.queueCount;
            queueTypeInfo.minImageTransferGranularity = vkQueueFamilyProperties.minImageTransferGranularity;
        }
    }

    // Extract memory properties and assign memory locations
    auto& vkMemoryProperties2 = dataCache->propertyMap.get<VkPhysicalDeviceMemoryProperties2>();
    vkiPhysicalDevice.getPhysicalDeviceMemoryProperties2(vkPhysicalDeviceHandle, &vkMemoryProperties2);
    const VkPhysicalDeviceMemoryProperties& vkMemoryProperties = vkMemoryProperties2.memoryProperties;
    assignMemoryLocations(vkMemoryProperties, tp::view(dataCache->memoryLocationInfos));

    // Store extensions
    uint32_t extCount;
    throwRetcodeErrors(
        vkiPhysicalDevice.enumerateDeviceExtensionProperties(vkPhysicalDeviceHandle, nullptr, &extCount, nullptr));
    dataCache->extensions.resize(extCount);
    throwRetcodeErrors(vkiPhysicalDevice.enumerateDeviceExtensionProperties(
        vkPhysicalDeviceHandle, nullptr, &extCount, dataCache->extensions.data()));
    dataCache->extensions.resize(extCount);

    // Ask for the base features as its a commonly requested structure
    auto& vkFeatures2 = dataCache->featureMap.get<VkPhysicalDeviceFeatures2>();
    vkiPhysicalDevice.getPhysicalDeviceFeatures2(vkPhysicalDeviceHandle, &vkFeatures2);
}

QueueTypeInfo PhysicalDevice::getQueueTypeInfo(QueueType type) const {
    return dataCache->queueTypeInfos[static_cast<uint32_t>(type)];
}

MemoryLocationInfo PhysicalDevice::getMemoryLocationInfo(MemoryLocation location) const {
    return dataCache->memoryLocationInfos[static_cast<uint32_t>(location)];
}

bool PhysicalDevice::isExtensionAvailable(const char* extension) const {
    for (const VkExtensionProperties& extInfo : dataCache->extensions) {
        if (strcmp(extension, extInfo.extensionName) == 0) {
            return true;
        }
    }
    return false;
}

FormatCapabilities PhysicalDevice::queryFormatCapabilities(Format format) const {
    VkFormatProperties fmtProperties;
    vkiPhysicalDevice->getPhysicalDeviceFormatProperties(
        vkPhysicalDeviceHandle, static_cast<VkFormat>(format), &fmtProperties);

    FormatUsageMask usageMask = FormatUsageMask::None();
    VkFormatFeatureFlags imgFeatures = fmtProperties.optimalTilingFeatures;
    if (containsAllBits(imgFeatures, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
        containsAllBits(imgFeatures, VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        usageMask |= FormatUsage::SampledImage;

    if (containsAllBits(imgFeatures, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        usageMask |= FormatUsage::StorageImage;

    if (containsAllBits(imgFeatures, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) &&
        containsAllBits(imgFeatures, VK_FORMAT_FEATURE_BLIT_DST_BIT))
        usageMask |= FormatUsage::ColorAttachment;

    if (containsAllBits(imgFeatures, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        usageMask |= FormatUsage::DepthStencilAttachment;

    VkFormatFeatureFlags bufFeatures = fmtProperties.bufferFeatures;
    if (containsAllBits(bufFeatures, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        usageMask |= FormatUsage::TexelBuffer;

    if (containsAllBits(bufFeatures, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        usageMask |= FormatUsage::StorageTexelBuffer;

    if (containsAllBits(bufFeatures, VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))
        usageMask |= FormatUsage::VertexBuffer;

    FormatFeatureMask featureMask = FormatFeatureMask::None();

    bool linearSupport = usageMask.contains(FormatUsage::SampledImage) ||
        usageMask.contains(FormatUsage::ColorAttachment);
    if (usageMask.contains(FormatUsage::SampledImage) &&
        !containsAllBits(imgFeatures, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        linearSupport = false;

    if (usageMask.contains(FormatUsage::ColorAttachment) &&
        !containsAllBits(imgFeatures, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT))
        linearSupport = false;

    if (linearSupport)
        featureMask |= FormatFeature::LinearFiltering;

    bool atomicSupport = usageMask.contains(FormatUsage::StorageImage) ||
        usageMask.contains(FormatUsage::StorageTexelBuffer);
    if (usageMask.contains(FormatUsage::StorageImage) &&
        !containsAllBits(imgFeatures, VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT))
        atomicSupport = false;

    if (usageMask.contains(FormatUsage::StorageTexelBuffer) &&
        !containsAllBits(bufFeatures, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT))
        atomicSupport = false;

    if (atomicSupport)
        featureMask |= FormatFeature::AtomicOperations;

    return FormatCapabilities(usageMask, featureMask);
}

SurfaceCapabilities PhysicalDevice::querySurfaceCapabilitiesKHR(VkSurfaceKHR vkSurface) const {
    VkSurfaceHandleKHR vkSurfaceHandle = VkSurfaceHandleKHR(vkSurface);
    SurfaceCapabilities capabilities = {};

    if (!vkiSurface->isLoaded())
        return capabilities;

    std::lock_guard<Mutex> mutexLock(dataCache->surfaceCapabilityCacheMutex);

    SurfaceCapabilitiesCacheEntry* cacheEntry = nullptr;
    auto hit = dataCache->surfaceCapabilityCache.find(vkSurfaceHandle);
    if (hit == dataCache->surfaceCapabilityCache.end()) {
        dataCache->surfaceCapabilityCache.emplace(
            std::make_pair(vkSurfaceHandle, std::make_unique<SurfaceCapabilitiesCacheEntry>()));
        cacheEntry = dataCache->surfaceCapabilityCache[vkSurfaceHandle].get();

        // Check supported queue types
        cacheEntry->queueTypes.reserve(QueueTypeEnumView::size());
        for (QueueType queueType : QueueTypeEnumView()) {
            uint32_t queueFamily = dataCache->queueTypeInfos[static_cast<uint32_t>(queueType)].queueFamilyIndex;
            if (queueFamily != QueueFamilyUndefined && queueFamily != VK_QUEUE_FAMILY_EXTERNAL) {
                VkBool32 familySupportsSurface;
                throwRetcodeErrors(vkiSurface->getPhysicalDeviceSurfaceSupportKHR(
                    vkPhysicalDeviceHandle, queueFamily, vkSurfaceHandle, &familySupportsSurface));
                if (familySupportsSurface) {
                    cacheEntry->queueTypes.push_back(queueType);
                }
            }
        }

        if (cacheEntry->queueTypes.empty()) {
            // No supported queues
            return capabilities;
        }

        // Check supported present modes
        uint32_t presentModeCount;
        throwRetcodeErrors(vkiSurface->getPhysicalDeviceSurfacePresentModesKHR(
            vkPhysicalDeviceHandle, vkSurfaceHandle, &presentModeCount, nullptr));
        cacheEntry->presentModes.resize(presentModeCount);
        throwRetcodeErrors(vkiSurface->getPhysicalDeviceSurfacePresentModesKHR(
            vkPhysicalDeviceHandle,
            vkSurfaceHandle,
            &presentModeCount,
            vkCastConvertibleEnumPtr(cacheEntry->presentModes.data())));

        // Check supported sRGB formats
        uint32_t formatCount;
        ScratchVector<VkSurfaceFormatKHR> vkSurfaceFormats;
        throwRetcodeErrors(vkiSurface->getPhysicalDeviceSurfaceFormatsKHR(
            vkPhysicalDeviceHandle, vkSurfaceHandle, &formatCount, nullptr));
        vkSurfaceFormats.resize(formatCount);
        throwRetcodeErrors(vkiSurface->getPhysicalDeviceSurfaceFormatsKHR(
            vkPhysicalDeviceHandle, vkSurfaceHandle, &formatCount, vkSurfaceFormats.data()));

        cacheEntry->supportedFormatsSRGB.reserve(formatCount);
        for (const VkSurfaceFormatKHR& vkSurfaceFormat : vkSurfaceFormats) {
            if (vkSurfaceFormat.colorSpace == VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                cacheEntry->supportedFormatsSRGB.push_back(vkCastConvertibleEnum(vkSurfaceFormat.format));
            }
        }

    } else {
        cacheEntry = hit->second.get();
        if (cacheEntry->queueTypes.empty())
            return capabilities; // Cached unsupported
    }

    VkSurfaceCapabilitiesKHR vkCapabilities;
    throwRetcodeErrors(
        vkiSurface->getPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDeviceHandle, vkSurfaceHandle, &vkCapabilities));

    // Fill out the structure itself
    capabilities.supportedQueueTypes = view(cacheEntry->queueTypes);
    capabilities.supportedPresentModes = view(cacheEntry->presentModes);
    capabilities.supportedFormatsSRGB = view(cacheEntry->supportedFormatsSRGB);
    capabilities.minImageCount = vkCapabilities.minImageCount;
    capabilities.maxImageCount = vkCapabilities.maxImageCount;
    capabilities.currentExtent = Extent2D(vkCapabilities.currentExtent);
    capabilities.minImageExtent = Extent2D(vkCapabilities.minImageExtent);
    capabilities.maxImageExtent = Extent2D(vkCapabilities.maxImageExtent);
    capabilities.maxImageArrayLayers = vkCapabilities.maxImageArrayLayers;
    capabilities.currentTransform = vkCastConvertibleEnum(vkCapabilities.currentTransform);
    capabilities.supportedTransforms = vkCastConvertibleEnumMask<SurfaceTransform, VkSurfaceTransformFlagBitsKHR>(
        vkCapabilities.supportedTransforms);
    capabilities.supportedCompositeAlphas = vkCastConvertibleEnumMask<CompositeAlpha, VkCompositeAlphaFlagBitsKHR>(
        vkCapabilities.supportedCompositeAlpha);
    capabilities.supportedImageUsages = vkCastConvertibleEnumMask<ImageUsage, VkImageUsageFlagBits>(
        vkCapabilities.supportedUsageFlags);

    return capabilities;
}

// Empty move assignment and destructor defined in here so it knows the definition of PhysicalDeviceDataCache
PhysicalDevice::PhysicalDevice(PhysicalDevice&&) noexcept = default;

PhysicalDevice& PhysicalDevice::operator=(PhysicalDevice&&) noexcept = default;

PhysicalDevice::~PhysicalDevice() = default;

std::shared_lock<std::shared_mutex> PhysicalDevice::acquireStructuresReadLock() const {
    return std::shared_lock<std::shared_mutex>(dataCache->structuresMutex);
}

std::unique_lock<std::shared_mutex> PhysicalDevice::acquireStructuresWriteLock() const {
    return std::unique_lock<std::shared_mutex>(dataCache->structuresMutex);
}

VkFeatureMap& PhysicalDevice::getFeatureStructureMap() const {
    return dataCache->featureMap;
}

VkPropertyMap& PhysicalDevice::getPropertyStructureMap() const {
    return dataCache->propertyMap;
}

void PhysicalDevice::vkQueryFeatureStruct(void* structPtr) const {
    // Need to query the generic features again to be able to ask for a specific structure
    VkPhysicalDeviceFeatures2 dummyFeatures;
    dummyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    dummyFeatures.pNext = structPtr;

    vkiPhysicalDevice->getPhysicalDeviceFeatures2(vkPhysicalDeviceHandle, &dummyFeatures);
}

void PhysicalDevice::vkQueryPropertyStruct(void* structPtr) const {
    // Need to query the generic properties again to be able to ask for a specific structure
    VkPhysicalDeviceProperties2 dummyProperties;
    dummyProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    dummyProperties.pNext = structPtr;

    vkiPhysicalDevice->getPhysicalDeviceProperties2(vkPhysicalDeviceHandle, &dummyProperties);
}

}
