#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <deque>

namespace tp {

class BufferImpl;

// Specifies a memory dependency on a buffer range between two accesses
struct BufferDependency {
    VkBufferHandle vkBufferHandle;
    BufferAccessRange range;
    ResourceAccess srcAccess;
    ResourceAccess dstAccess;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;

    BufferDependency(
        VkBufferHandle vkBufferHandle,
        BufferAccessRange range,
        ResourceAccess srcAccess,
        ResourceAccess dstAccess,
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
        : vkBufferHandle(vkBufferHandle),
          range(std::move(range)),
          srcAccess(std::move(srcAccess)),
          dstAccess(std::move(dstAccess)),
          srcQueueFamilyIndex(srcQueueFamilyIndex),
          dstQueueFamilyIndex(dstQueueFamilyIndex) {}

    // Translates the dependency to a Vulkan memory barrier
    VkBufferMemoryBarrier toMemoryBarrier() const;
};

// Specifies a memory dependency on an image subresource range between two accesses and optionally defines a layout
// transition
struct ImageDependency {
    VkImageHandle vkImageHandle;
    ImageAccessRange range;
    ResourceAccess srcAccess;
    ResourceAccess dstAccess;
    VkImageLayout srcLayout;
    VkImageLayout dstLayout;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;

    ImageDependency(
        VkImageHandle vkImageHandle,
        ImageAccessRange range,
        ResourceAccess srcAccess,
        ResourceAccess dstAccess,
        VkImageLayout srcLayout,
        VkImageLayout dstLayout,
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
        : vkImageHandle(vkImageHandle),
          range(std::move(range)),
          srcAccess(std::move(srcAccess)),
          dstAccess(std::move(dstAccess)),
          srcLayout(srcLayout),
          dstLayout(dstLayout),
          srcQueueFamilyIndex(srcQueueFamilyIndex),
          dstQueueFamilyIndex(dstQueueFamilyIndex) {}

    // Translates the dependency to Vulkan memory barriers (mip mask can be disjoint)
    void toImageBarriers(ScratchVector<VkImageMemoryBarrier>& barriers) const;
};

// Represents a Vulkan barrier for synchronizing accesses
struct Barrier {
    // The index of the first command that depends on this barrier
    uint32_t commandIndex;

    // The source and destination stage masks forming the execution dependency
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;

    // Stage masks extended to cover all stages that the barrier logically covers in the pipeline stage order
    VkPipelineStageFlags extSrcStageMask;
    VkPipelineStageFlags extDstStageMask;

    // Resource memory dependencies that translate to Vulkan memory barriers
    ScratchVector<BufferDependency> bufferDependencies;
    ScratchVector<ImageDependency> imageDependencies;

    explicit Barrier(uint32_t commandIndex)
        : commandIndex(commandIndex), srcStageMask(0), dstStageMask(0), extSrcStageMask(0), extDstStageMask(0) {}

    // Extends the barrier by the given buffer dependency, returning its index if one was added, ~0 otherwise.
    uint32_t addDependency(const BufferDependency& dependency);

    // Extends the barrier by the given image dependency, returning its index if one was added, ~0 otherwise.
    uint32_t addDependency(const ImageDependency& dependency);

    // Extends an existing buffer memory dependency by the given dependency
    void extendMemoryDependency(const BufferDependency& dependency, uint32_t memoryDependencyIndex);

    // Extends an existing image memory dependency by the given dependency
    void extendMemoryDependency(const ImageDependency& dependency, uint32_t memoryDependencyIndex);

    void clear();

private:
    // Update extended stage masks to reflect changes made to the actual stage masks
    void updateExtendedStageMasks();
};

// Translates known dependencies into barriers to be inserted into the command buffer
class BarrierList {
public:
    explicit BarrierList(uint64_t jobId) : jobId(jobId) {}

    uint64_t getJobId() const {
        return jobId;
    }

    uint32_t getBarrierCount() const {
        return static_cast<uint32_t>(barriers.size());
    }

    Barrier& getBarrier(uint32_t barrierIndex) {
        return barriers[barrierIndex];
    }

    const Barrier& getBarrier(uint32_t barrierIndex) const {
        return barriers[barrierIndex];
    }

    // Synchronize a dependency with a barrier, attempting to reuse any with index greater than
    // firstReusableBarrierIndex
    template <typename TResourceDependency>
    BarrierReference synchronizeDependency(
        const TResourceDependency& dependency,
        uint32_t commandIndex,
        uint32_t firstReusableBarrierIndex);

    // Synchronize a dependency with a barrier, reusing a specific barrier
    template <typename TResourceDependency>
    BarrierReference synchronizeDependency(const TResourceDependency& dependency, BarrierReference reusedBarrier);

private:
    uint64_t jobId = 0;
    ScratchDeque<Barrier> barriers;
};

}
