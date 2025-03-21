#include "memory_allocator.hpp"
#include "device_container.hpp"
#include "logical_device.hpp"
#include "../application/application_container.hpp"

namespace tp {

enum class UserAllocationFlag : uintptr_t {
    FullyHostCoherent = 1,
};
TEPHRA_MAKE_ENUM_BIT_MASK(UserAllocationFlagMask, UserAllocationFlag);

MemoryAllocator::MemoryAllocator(DeviceContainer* deviceImpl, Instance* instance, const MemoryAllocatorSetup& setup)
    : deviceImpl(deviceImpl),
      vkDeviceHandle(deviceImpl->getLogicalDevice()->vkGetDeviceHandle()),
      outOfMemoryCallback(setup.outOfMemoryCallback) {
    LogicalDevice* device = deviceImpl->getLogicalDevice();

    VmaAllocatorCreateInfo allocInfo;
    allocInfo.flags = 0;
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(Functionality::MemoryBudgetEXT))
        allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(Functionality::BufferDeviceAddress))
        allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    allocInfo.physicalDevice = deviceImpl->getPhysicalDevice()->vkGetPhysicalDeviceHandle();
    allocInfo.device = device->vkGetDeviceHandle();
    allocInfo.preferredLargeHeapBlockSize = setup.preferredLargeHeapBlockSize;
    allocInfo.pAllocationCallbacks = nullptr;
    allocInfo.pDeviceMemoryCallbacks = setup.vmaDeviceMemoryCallbacks;
    allocInfo.pHeapSizeLimit = nullptr;
    allocInfo.instance = instance->vkGetInstanceHandle();
    allocInfo.vulkanApiVersion = Version::getMaxUsedVulkanAPIVersion().pack();
    allocInfo.pTypeExternalMemoryHandleTypes = nullptr;

    // Provide vulkan function pointers
    vkiMemory = device->loadDeviceInterface<VulkanDeviceMemoryInterface>();
    auto& vkiPhysicalDevice = deviceImpl->getParentAppImpl()->getInstance()->getPhysicalDeviceInterface();
    VmaVulkanFunctions functions;
    functions.vkGetPhysicalDeviceProperties = vkiPhysicalDevice.getPhysicalDeviceProperties;
    functions.vkGetPhysicalDeviceMemoryProperties = vkiPhysicalDevice.getPhysicalDeviceMemoryProperties;
    functions.vkAllocateMemory = vkiMemory.allocateMemory;
    functions.vkFreeMemory = vkiMemory.freeMemory;
    functions.vkMapMemory = vkiMemory.mapMemory;
    functions.vkUnmapMemory = vkiMemory.unmapMemory;
    functions.vkFlushMappedMemoryRanges = vkiMemory.flushMappedMemoryRanges;
    functions.vkInvalidateMappedMemoryRanges = vkiMemory.invalidateMappedMemoryRanges;
    functions.vkBindBufferMemory = vkiMemory.bindBufferMemory;
    functions.vkBindImageMemory = vkiMemory.bindImageMemory;
    functions.vkGetBufferMemoryRequirements = vkiMemory.getBufferMemoryRequirements;
    functions.vkGetImageMemoryRequirements = vkiMemory.getImageMemoryRequirements;
    functions.vkCreateBuffer = vkiMemory.createBuffer;
    functions.vkDestroyBuffer = vkiMemory.destroyBuffer;
    functions.vkCreateImage = vkiMemory.createImage;
    functions.vkDestroyImage = vkiMemory.destroyImage;
    functions.vkCmdCopyBuffer = vkiMemory.cmdCopyBuffer;
    functions.vkGetBufferMemoryRequirements2KHR = vkiMemory.getBufferMemoryRequirements2;
    functions.vkGetImageMemoryRequirements2KHR = vkiMemory.getImageMemoryRequirements2;
    functions.vkBindBufferMemory2KHR = vkiMemory.bindBufferMemory2;
    functions.vkBindImageMemory2KHR = vkiMemory.bindImageMemory2;
    functions.vkGetPhysicalDeviceMemoryProperties2KHR = vkiPhysicalDevice.getPhysicalDeviceMemoryProperties2;

    allocInfo.pVulkanFunctions = &functions;

    throwRetcodeErrors(vmaCreateAllocator(&allocInfo, vkCastTypedHandlePtr(&vmaAllocator)));

    // Store useful memory properties
    const PhysicalDevice* physicalDevice = deviceImpl->getPhysicalDevice();
    auto& memoryProperties = physicalDevice->vkQueryProperties<VkPhysicalDeviceMemoryProperties>();

    allMemoryHostCoherent = true;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        memoryTypeFlags[i] = memoryProperties.memoryTypes[i].propertyFlags;
        if ((memoryTypeFlags[i] & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // memory type is host visible, but not coherent
            allMemoryHostCoherent = false;
        }
    }

    for (MemoryLocation location : MemoryLocationEnumView()) {
        MemoryLocationInfo locationInfo = physicalDevice->getMemoryLocationInfo(location);
        memoryLocationTypeIndices[static_cast<uint32_t>(location)] = locationInfo.memoryTypeIndex;
    }
}

std::pair<Lifeguard<VkBufferHandle>, Lifeguard<VmaAllocationHandle>> MemoryAllocator::allocateBuffer(
    const BufferSetup& setup,
    const MemoryPreference& memoryPreference) {
    VkBufferCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.size = setup.size;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;

    createInfo.usage = setup.vkAdditionalUsage;
    // Translate usage flags
    createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (setup.usage.contains(BufferUsage::TexelBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::UniformBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::StorageBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::IndexBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::VertexBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::IndirectBuffer)) {
        createInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    if (setup.usage.contains(BufferUsage::DeviceAddress)) {
        createInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    VkBufferHandle vkBufferHandle = deviceImpl->getLogicalDevice()->createBuffer(createInfo);
    auto bufferHandleLifeguard = deviceImpl->vkMakeHandleLifeguard(vkBufferHandle);

    // Allocate memory according to memory preference
    VmaAllocationCreateInfo allocInfo;
    allocInfo.flags = setup.vmaAdditionalFlags;
    allocInfo.flags |= memoryPreference.createPersistentlyMapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    allocInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
    allocInfo.requiredFlags = 0;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    allocInfo.pool = VK_NULL_HANDLE;
    allocInfo.pUserData = nullptr;

    VmaAllocationHandle allocation;
    for (auto memoryLocation : memoryPreference.locationProgression) {
        uint32_t locationTypeIndex = getMemoryLocationTypeIndex(memoryLocation);

        if (locationTypeIndex != ~0u) {
            allocInfo.memoryTypeBits = 1u << locationTypeIndex;

            while (true) {
                VkResult retcode = vmaAllocateMemoryForBuffer(
                    vmaAllocator, vkBufferHandle, &allocInfo, vkCastTypedHandlePtr(&allocation), nullptr);
                if (retcode >= 0) {
                    retcode = vmaBindBufferMemory(vmaAllocator, allocation, vkBufferHandle);
                    if (retcode >= 0)
                        break;
                }

                // Try to free up some memory, if there was some released, retry the allocation again
                if (retcode == VK_ERROR_OUT_OF_DEVICE_MEMORY && outOfMemoryCallback &&
                    outOfMemoryCallback(memoryLocation)) {
                    // Allow deferredDestructor to run which may actually release some of the memory just released by
                    // the host application
                    deviceImpl->waitForIdle();
                    continue;
                }

                if (retcode != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
                    throwRetcodeErrors(retcode);
                }

                break;
            }

            // Allocation successful, stop searching free memory according to memory preference
            if (!allocation.isNull())
                break;
        }
    }

    if (!allocation.isNull()) {
        VmaAllocationInfo allocInfo = getAllocationInfo(allocation);
        if (memoryPreference.createPersistentlyMapped &&
            (memoryTypeFlags[allocInfo.memoryType] & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
            // Memory is both persistently mapped and host coherent. Store that information in the user data
            vmaSetAllocationUserData(
                vmaAllocator, allocation, reinterpret_cast<void*>(UserAllocationFlag::FullyHostCoherent));
        }
    } else {
        throwRetcodeErrors(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }

    return { std::move(bufferHandleLifeguard), deviceImpl->vkMakeHandleLifeguard(allocation) };
}

std::pair<Lifeguard<VkImageHandle>, Lifeguard<VmaAllocationHandle>> MemoryAllocator::allocateImage(
    const ImageSetup& setup) {
    return createImage(setup, true);
}

VmaAllocationInfo MemoryAllocator::getAllocationInfo(VmaAllocationHandle allocation) const {
    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(vmaAllocator, allocation, &allocInfo);
    return allocInfo;
}

MemoryLocation MemoryAllocator::getAllocationLocation(VmaAllocationHandle allocation) const {
    VmaAllocationInfo allocInfo = getAllocationInfo(allocation);
    return memoryTypeFlagsToMemoryLocation(memoryTypeFlags[allocInfo.memoryType]);
}

bool MemoryAllocator::isAllocationFullyHostCoherent(VmaAllocationHandle allocation) const {
    UserAllocationFlagMask flags = UserAllocationFlagMask(
        reinterpret_cast<uintptr_t>(getAllocationInfo(allocation).pUserData));

    return flags.contains(UserAllocationFlag::FullyHostCoherent);
}

VkMemoryRequirements MemoryAllocator::getImageMemoryRequirements(const ImageSetup& setup) const {
    // Create the image to figure out its requirements, but don't allocate
    // TODO: use vkGetDeviceImageMemoryRequirementsKHR instead to avoid this
    auto [imageHandleLifeguard, allocationHandleLifeguard] = createImage(setup, false);

    VkMemoryRequirements memoryReq;
    vkiMemory.getImageMemoryRequirements(vkDeviceHandle, imageHandleLifeguard.vkGetHandle(), &memoryReq);
    return memoryReq;
}

VmaBudget MemoryAllocator::getMemoryHeapBudget(uint32_t heapIndex) const {
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(vmaAllocator, budgets);
    return budgets[heapIndex];
}

void* MemoryAllocator::mapMemory(VmaAllocationHandle allocation) {
    void* ptr;
    throwRetcodeErrors(vmaMapMemory(vmaAllocator, allocation, &ptr));
    return ptr;
}

void MemoryAllocator::unmapMemory(VmaAllocationHandle allocation) {
    vmaUnmapMemory(vmaAllocator, allocation);
}

void MemoryAllocator::invalidateAllocationMemory(VmaAllocationHandle allocation, VkDeviceSize offset, VkDeviceSize size)
    const {
    vmaInvalidateAllocation(vmaAllocator, allocation, offset, size);
}

void MemoryAllocator::flushAllocationMemory(VmaAllocationHandle allocation, VkDeviceSize offset, VkDeviceSize size)
    const {
    vmaFlushAllocation(vmaAllocator, allocation, offset, size);
}

void MemoryAllocator::freeAllocation(VmaAllocationHandle allocation) {
    vmaFreeMemory(vmaAllocator, allocation);
}

MemoryAllocator::~MemoryAllocator() {
    vmaDestroyAllocator(vmaAllocator);
}

std::pair<Lifeguard<VkImageHandle>, Lifeguard<VmaAllocationHandle>> MemoryAllocator::createImage(
    const ImageSetup& setup,
    bool doAllocate) const {
    // Set up the create info
    VkImageCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = vkCastConvertibleEnumMask(setup.flags);

    switch (setup.type) {
    case ImageType::Image1D:
        createInfo.imageType = VK_IMAGE_TYPE_1D;
        break;
    case ImageType::Image2D:
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case ImageType::Image2DCubeCompatible:
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    case ImageType::Image3D:
        createInfo.imageType = VK_IMAGE_TYPE_3D;
        break;
    case ImageType::Image3D2DArrayCompatible:
        createInfo.imageType = VK_IMAGE_TYPE_3D;
        createInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        break;
    }

    createInfo.format = vkCastConvertibleEnum(setup.format);
    createInfo.extent = setup.extent;
    createInfo.mipLevels = setup.mipLevelCount;
    createInfo.arrayLayers = setup.arrayLayerCount;
    createInfo.samples = vkCastConvertibleEnum(setup.sampleLevel);
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = setup.vkAdditionalUsage | vkCastConvertibleEnumMask(setup.usage);
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    const void** nextExtPtr = &createInfo.pNext;

    // Add additional view compatible formats
    ScratchVector<VkFormat> compatibleFormats;
    VkImageFormatListCreateInfo formatListCreateInfo;
    if (!setup.compatibleFormats.empty()) {
        compatibleFormats.reserve(setup.compatibleFormats.size() + 1);

        bool hadImageFormat = false;
        for (Format format : setup.compatibleFormats) {
            compatibleFormats.push_back(vkCastConvertibleEnum(format));
            if (format == setup.format)
                hadImageFormat = true;
        }

        // Make sure the image's own format is in the compatible list
        if (!hadImageFormat)
            compatibleFormats.push_back(createInfo.format);

        formatListCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
        formatListCreateInfo.pNext = nullptr;
        formatListCreateInfo.viewFormatCount = static_cast<uint32_t>(compatibleFormats.size());
        formatListCreateInfo.pViewFormats = compatibleFormats.data();
        *nextExtPtr = &formatListCreateInfo;
        nextExtPtr = &formatListCreateInfo.pNext;
        createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    VkImageHandle vkImageHandle;
    VmaAllocationHandle vmaAllocationHandle;

    if (doAllocate) {
        // Images are always allocated from device local memory, don't use memory progression
        VmaAllocationCreateInfo allocInfo;
        allocInfo.flags = setup.vmaAdditionalFlags;
        allocInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        allocInfo.preferredFlags = 0;
        allocInfo.memoryTypeBits = UINT32_MAX;
        allocInfo.pool = VK_NULL_HANDLE;
        allocInfo.pUserData = nullptr;

        while (true) {
            VkResult retcode = vmaCreateImage(
                vmaAllocator,
                &createInfo,
                &allocInfo,
                vkCastTypedHandlePtr(&vkImageHandle),
                vkCastTypedHandlePtr(&vmaAllocationHandle),
                nullptr);

            // Try to free up some memory, if there was some released, retry the allocation again
            if (retcode == VK_ERROR_OUT_OF_DEVICE_MEMORY && outOfMemoryCallback &&
                outOfMemoryCallback(MemoryLocation::DeviceLocal)) {
                // Allow deferredDestructors to run which may actually release some of the memory just released by the
                // host application
                deviceImpl->waitForIdle();
                continue;
            }

            throwRetcodeErrors(retcode);
            break;
        }
    } else {
        vkImageHandle = deviceImpl->getLogicalDevice()->createImage(createInfo);
    }

    return { deviceImpl->vkMakeHandleLifeguard(vkImageHandle), deviceImpl->vkMakeHandleLifeguard(vmaAllocationHandle) };
}

uint32_t MemoryAllocator::getMemoryLocationTypeIndex(MemoryLocation memoryLocation) const {
    return memoryLocationTypeIndices[static_cast<uint32_t>(memoryLocation)];
}

MemoryLocation MemoryAllocator::memoryTypeFlagsToMemoryLocation(VkMemoryPropertyFlags typeFlags) {
    // Filter only the bits we care about
    typeFlags &=
        (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
         VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);

    switch (typeFlags) {
    case VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:
        return MemoryLocation::HostVisible;
    case VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT:
        return MemoryLocation::HostCached;
    case VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:
        return MemoryLocation::DeviceLocal;
    case VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:
        return MemoryLocation::DeviceLocalHostVisible;
    case VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT:
        return MemoryLocation::DeviceLocalHostCached;
    default:
        return MemoryLocation::Undefined;
    }
}

}
