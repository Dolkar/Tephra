#pragma once

#include "../common_impl.hpp"
#include "../vulkan/interface.hpp"
#include <tephra/application.hpp>
#include <tephra/device.hpp>

namespace tp {
class Instance;
class LogicalDevice;

class MemoryAllocator {
public:
    MemoryAllocator(DeviceContainer* deviceImpl, Instance* instance, const MemoryAllocatorSetup& setup);

    std::pair<Lifeguard<VkBufferHandle>, Lifeguard<VmaAllocationHandle>> allocateBuffer(
        const BufferSetup& setup,
        const MemoryPreference& memoryPreference);

    std::pair<Lifeguard<VkImageHandle>, Lifeguard<VmaAllocationHandle>> allocateImage(const ImageSetup& setup);

    VmaAllocationInfo getAllocationInfo(VmaAllocationHandle allocation) const;

    MemoryLocation getAllocationLocation(VmaAllocationHandle allocation) const;

    // If true, all host visible memory on this device is also host coherent
    bool isAllMemoryHostCoherent() const {
        return allMemoryHostCoherent;
    }

    // If true, the allocation is both host coherent and persistently mapped. This means r/w is a no-op and thread safe
    // for distinct regions
    bool isAllocationFullyHostCoherent(VmaAllocationHandle allocation) const;

    VkMemoryRequirements getImageMemoryRequirements(const ImageSetup& setup) const;

    VmaBudget getMemoryHeapBudget(uint32_t heapIndex) const;

    void* mapMemory(VmaAllocationHandle allocation);

    void unmapMemory(VmaAllocationHandle allocation);

    void invalidateAllocationMemory(VmaAllocationHandle allocation, VkDeviceSize offset, VkDeviceSize size) const;

    void flushAllocationMemory(VmaAllocationHandle allocation, VkDeviceSize offset, VkDeviceSize size) const;

    void freeAllocation(VmaAllocationHandle allocation);

    VmaAllocatorHandle vmaGetAllocatorHandle() const {
        return vmaAllocator;
    }

    TEPHRA_MAKE_NONCOPYABLE(MemoryAllocator);
    TEPHRA_MAKE_NONMOVABLE(MemoryAllocator);
    ~MemoryAllocator();

    static MemoryLocation memoryTypeFlagsToMemoryLocation(VkMemoryPropertyFlags typeFlags);

private:
    DeviceContainer* deviceImpl;
    VkDeviceHandle vkDeviceHandle;
    VmaAllocatorHandle vmaAllocator;
    VulkanDeviceMemoryInterface vkiMemory;

    uint32_t memoryLocationTypeIndices[MemoryLocationEnumView::size()] = { ~0u };
    VkMemoryPropertyFlags memoryTypeFlags[VK_MAX_MEMORY_TYPES] = { 0 };
    bool allMemoryHostCoherent;

    std::pair<Lifeguard<VkImageHandle>, Lifeguard<VmaAllocationHandle>> createImage(
        const ImageSetup& setup,
        bool doAllocate) const;
    uint32_t getMemoryLocationTypeIndex(MemoryLocation memoryLocation) const;
};
}
