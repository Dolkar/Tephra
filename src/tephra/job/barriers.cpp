#include "barriers.hpp"

namespace tp {

VkPipelineStageFlags combineFlags(ArrayParameter<const VkPipelineStageFlags> flags) {
    VkPipelineStageFlags mask = 0;
    for (auto stageBit : flags) {
        mask |= stageBit;
    }
    return mask;
}

// Logical order of pipeline stages excluding top-of-pipe and bottom-of-pipe
const uint64_t GraphicsPipelineStageCount = 10;
const VkPipelineStageFlags GraphicsPipelineStagesArray[GraphicsPipelineStageCount] = {
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
};
const ArrayView<const VkPipelineStageFlags> GraphicsPipelineStages{ GraphicsPipelineStagesArray,
                                                                    GraphicsPipelineStageCount };
const VkPipelineStageFlags GraphicsPipelineStagesMask = combineFlags(GraphicsPipelineStages);

const uint64_t ComputePipelineStageCount = 2;
const VkPipelineStageFlags ComputePipelineStagesArray[ComputePipelineStageCount] = {
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
};
const ArrayView<const VkPipelineStageFlags> ComputePipelineStages{ ComputePipelineStagesArray,
                                                                   ComputePipelineStageCount };
const VkPipelineStageFlags ComputePipelineStagesMask = combineFlags(ComputePipelineStages);

VkBufferMemoryBarrier BufferDependency::toMemoryBarrier() const {
    TEPHRA_ASSERT((srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) == (srcQueueFamilyIndex == dstQueueFamilyIndex));

    VkBufferMemoryBarrier memoryBarrier;
    memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = srcAccess.accessMask;
    memoryBarrier.dstAccessMask = dstAccess.accessMask;
    memoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    memoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    memoryBarrier.buffer = vkBufferHandle;
    memoryBarrier.offset = range.offset;
    memoryBarrier.size = range.size;
    return memoryBarrier;
}

void ImageDependency::toImageBarriers(ScratchVector<VkImageMemoryBarrier>& barriers) const {
    TEPHRA_ASSERT((srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) == (srcQueueFamilyIndex == dstQueueFamilyIndex));

    VkImageMemoryBarrier memoryBarrier;
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = srcAccess.accessMask;
    memoryBarrier.dstAccessMask = dstAccess.accessMask;
    memoryBarrier.oldLayout = srcLayout;
    // Transition image layout only when needed - Undefined layout means reuse previous layout
    memoryBarrier.newLayout = dstLayout != VK_IMAGE_LAYOUT_UNDEFINED ? dstLayout : srcLayout;
    memoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    memoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    memoryBarrier.image = vkImageHandle;
    memoryBarrier.subresourceRange.baseArrayLayer = range.baseArrayLayer;
    memoryBarrier.subresourceRange.layerCount = range.arrayLayerCount;
    memoryBarrier.subresourceRange.aspectMask = vkCastConvertibleEnumMask(range.aspectMask);

    // Multiple barriers may need to be inserted for disjoint mip level masks
    uint32_t mipLevelMask = range.mipLevelMask;
    uint32_t mipLevelOffset = 0;
    int barrierCount = 0;
    while (mipLevelMask != 0) {
        while (mipLevelMask != 0 && (mipLevelMask & 0x1) == 0) {
            mipLevelOffset++;
            mipLevelMask >>= 1;
        }
        uint32_t mipLevelRangeOffset = mipLevelOffset;
        while (mipLevelMask != 0 && (mipLevelMask & 0x1) == 1) {
            mipLevelOffset++;
            mipLevelMask >>= 1;
        }
        uint32_t mipLevelRangeSize = mipLevelOffset - mipLevelRangeOffset;

        if (mipLevelRangeSize > 0) {
            memoryBarrier.subresourceRange.baseMipLevel = mipLevelRangeOffset;
            memoryBarrier.subresourceRange.levelCount = mipLevelRangeSize;

            barriers.push_back(memoryBarrier);
            barrierCount++;
        }
    }
    TEPHRA_ASSERT(barrierCount >= 1);
}

uint32_t Barrier::addDependency(const BufferDependency& dependency) {
    TEPHRA_ASSERT(!dependency.range.isNull());
    TEPHRA_ASSERT(dependency.srcAccess.stageMask != 0 && dependency.dstAccess.stageMask != 0);

    srcStageMask |= dependency.srcAccess.stageMask;
    dstStageMask |= dependency.dstAccess.stageMask;

    if (!containsAllBits(extSrcStageMask, srcStageMask) || !containsAllBits(extDstStageMask, dstStageMask)) {
        updateExtendedStageMasks();
    }

    // Only add a new memory dependency if needed (W->R, W->W, QFOT)
    bool needsQueueOwnershipTransfer = dependency.srcQueueFamilyIndex != dependency.dstQueueFamilyIndex;
    if (!dependency.srcAccess.isReadOnly() || needsQueueOwnershipTransfer) {
        bufferDependencies.push_back(dependency);
        return static_cast<uint32_t>(bufferDependencies.size() - 1);
    }
    return ~0;
}

uint32_t Barrier::addDependency(const ImageDependency& dependency) {
    TEPHRA_ASSERT(!dependency.range.isNull());
    TEPHRA_ASSERT(dependency.srcAccess.stageMask != 0 && dependency.dstAccess.stageMask != 0);

    srcStageMask |= dependency.srcAccess.stageMask;
    dstStageMask |= dependency.dstAccess.stageMask;

    if (!containsAllBits(extSrcStageMask, srcStageMask) || !containsAllBits(extDstStageMask, dstStageMask)) {
        updateExtendedStageMasks();
    }

    // Only add a new memory dependency if needed (W->R, W->W, QFOT, layout transition)
    bool needsQueueOwnershipTransfer = dependency.srcQueueFamilyIndex != dependency.dstQueueFamilyIndex;
    bool needsLayoutTransition = dependency.srcLayout != dependency.dstLayout &&
        dependency.dstLayout != VK_IMAGE_LAYOUT_UNDEFINED;
    if (!dependency.srcAccess.isReadOnly() || needsQueueOwnershipTransfer || needsLayoutTransition) {
        imageDependencies.push_back(dependency);
        return static_cast<uint32_t>(imageDependencies.size() - 1);
    }
    return ~0;
}

void Barrier::extendMemoryDependency(const BufferDependency& dependency, uint32_t memoryDependencyIndex) {
    // The user of this class wants to extend a specific memory barrier, which might not be valid or compatible
    // Let's double check
    TEPHRA_ASSERT(!dependency.range.isNull());
    TEPHRA_ASSERT(dependency.srcAccess.stageMask != 0 && dependency.dstAccess.stageMask != 0);
    TEPHRA_ASSERT(memoryDependencyIndex < bufferDependencies.size());
    BufferDependency& extendedDependency = bufferDependencies[memoryDependencyIndex];

    srcStageMask |= dependency.srcAccess.stageMask;
    dstStageMask |= dependency.dstAccess.stageMask;

    if (!containsAllBits(extSrcStageMask, srcStageMask) || !containsAllBits(extDstStageMask, dstStageMask)) {
        updateExtendedStageMasks();
    }

    TEPHRA_ASSERT(dependency.vkBufferHandle == extendedDependency.vkBufferHandle);
    TEPHRA_ASSERT(dependency.range.getStartPoint() >= extendedDependency.range.getStartPoint());
    TEPHRA_ASSERT(dependency.range.getEndPoint() <= extendedDependency.range.getEndPoint());

    TEPHRA_ASSERT(dependency.srcQueueFamilyIndex == extendedDependency.srcQueueFamilyIndex);
    TEPHRA_ASSERT(dependency.dstQueueFamilyIndex == extendedDependency.dstQueueFamilyIndex);

    extendedDependency.srcAccess |= dependency.srcAccess;
    extendedDependency.dstAccess |= dependency.dstAccess;
}

void Barrier::extendMemoryDependency(const ImageDependency& dependency, uint32_t memoryDependencyIndex) {
    // The user of this class wants to extend specific memory barriers, which might not be valid or compatible
    TEPHRA_ASSERT(!dependency.range.isNull());
    TEPHRA_ASSERT(dependency.srcAccess.stageMask != 0 && dependency.dstAccess.stageMask != 0);
    TEPHRA_ASSERT(memoryDependencyIndex < imageDependencies.size());
    ImageDependency& extendedDependency = imageDependencies[memoryDependencyIndex];

    srcStageMask |= dependency.srcAccess.stageMask;
    dstStageMask |= dependency.dstAccess.stageMask;

    if (!containsAllBits(extSrcStageMask, srcStageMask) || !containsAllBits(extDstStageMask, dstStageMask)) {
        updateExtendedStageMasks();
    }

    TEPHRA_ASSERT(dependency.vkImageHandle == extendedDependency.vkImageHandle);
    TEPHRA_ASSERT(dependency.range.getStartPoint() >= extendedDependency.range.getStartPoint());
    TEPHRA_ASSERT(dependency.range.getEndPoint() <= extendedDependency.range.getEndPoint());
    TEPHRA_ASSERT(containsAllBits(extendedDependency.range.mipLevelMask, dependency.range.mipLevelMask));
    TEPHRA_ASSERT(extendedDependency.range.aspectMask.containsAll(dependency.range.aspectMask));

    TEPHRA_ASSERT(
        dependency.srcLayout == extendedDependency.srcLayout || dependency.srcLayout == dependency.dstLayout ||
        dependency.srcLayout == VK_IMAGE_LAYOUT_UNDEFINED);
    // This one usually triggers when a single command has two overlapping usages that need incompatible layouts
    TEPHRA_ASSERT(dependency.dstLayout == extendedDependency.dstLayout);

    TEPHRA_ASSERT(dependency.srcQueueFamilyIndex == extendedDependency.srcQueueFamilyIndex);
    TEPHRA_ASSERT(dependency.dstQueueFamilyIndex == extendedDependency.dstQueueFamilyIndex);

    extendedDependency.srcAccess |= dependency.srcAccess;
    extendedDependency.dstAccess |= dependency.dstAccess;
}

void Barrier::clear() {
    srcStageMask = 0;
    dstStageMask = 0;
    extSrcStageMask = 0;
    extDstStageMask = 0;
    bufferDependencies.clear();
    imageDependencies.clear();
}

void Barrier::updateExtendedStageMasks() {
    // This includes non pipelined stages like VK_PIPELINE_STAGE_HOST_BIT
    extSrcStageMask = srcStageMask;
    extDstStageMask = dstStageMask;

    // Handle top-of-pipe & bottom-of-pipe
    if (containsAllBits(srcStageMask, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT))
        extSrcStageMask = ~0;
    if (srcStageMask != 0)
        extSrcStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (containsAllBits(dstStageMask, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT))
        extDstStageMask = ~0;
    if (dstStageMask != 0)
        extDstStageMask |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkPipelineStageFlags accumMask = 0;
    for (VkPipelineStageFlags stage : GraphicsPipelineStages) {
        if (containsAllBits(dstStageMask, stage)) {
            extDstStageMask |= GraphicsPipelineStagesMask & (~accumMask);
        }
        accumMask |= stage;
        if (containsAllBits(srcStageMask, stage)) {
            extSrcStageMask |= accumMask;
        }
    }
    accumMask = 0;
    for (VkPipelineStageFlags stage : ComputePipelineStages) {
        if (containsAllBits(dstStageMask, stage)) {
            extDstStageMask |= ComputePipelineStagesMask & (~accumMask);
        }
        accumMask |= stage;
        if (containsAllBits(srcStageMask, stage)) {
            extSrcStageMask |= accumMask;
        }
    }
}

template <typename TResourceDependency>
BarrierReference BarrierList::synchronizeDependency(
    const TResourceDependency& dependency,
    uint32_t commandIndex,
    uint32_t firstReusableBarrierIndex,
    bool wasExported) {
    if (wasExported)
        firstReusableBarrierIndex = tp::max(firstReusableBarrierIndex, exportReusableBarrierIndex);

    // Find an existing barrier with an already matching execution dependency
    for (uint32_t barrierIndex = getBarrierCount(); barrierIndex-- > firstReusableBarrierIndex;) {
        Barrier& barrier = barriers[barrierIndex];
        if (containsAllBits(barrier.extSrcStageMask, dependency.srcAccess.stageMask) &&
            containsAllBits(barrier.extDstStageMask, dependency.dstAccess.stageMask)) {
            uint32_t memoryBarrierIndex = barrier.addDependency(dependency);
            return BarrierReference(barrierIndex, memoryBarrierIndex);
        }
    }

    // Failing that, go for the last existing barrier
    if (firstReusableBarrierIndex < getBarrierCount()) {
        uint32_t memoryBarrierIndex = barriers[firstReusableBarrierIndex].addDependency(dependency);
        return BarrierReference(firstReusableBarrierIndex, memoryBarrierIndex);
    }

    // Failing that too, create a new barrier
    uint32_t barrierIndex = getBarrierCount();
    barriers.emplace_back(commandIndex);
    uint32_t memoryBarrierIndex = barriers[barrierIndex].addDependency(dependency);
    return BarrierReference(barrierIndex, memoryBarrierIndex);
}

template <typename TResourceDependency>
BarrierReference BarrierList::synchronizeDependency(
    const TResourceDependency& dependency,
    BarrierReference reusedBarrier) {
    TEPHRA_ASSERT(reusedBarrier.pipelineBarrierIndex < barriers.size());
    Barrier& barrier = barriers[reusedBarrier.pipelineBarrierIndex];

    if (reusedBarrier.hasMemoryBarrier()) {
        barrier.extendMemoryDependency(dependency, reusedBarrier.memoryBarrierIndex);
        return reusedBarrier;
    } else {
        uint32_t memoryBarrierIndex = barrier.addDependency(dependency);
        return BarrierReference(reusedBarrier.pipelineBarrierIndex, memoryBarrierIndex);
    }
}

template BarrierReference BarrierList::synchronizeDependency<BufferDependency>(
    const BufferDependency&,
    uint32_t,
    uint32_t,
    bool);
template BarrierReference BarrierList::synchronizeDependency<ImageDependency>(
    const ImageDependency&,
    uint32_t,
    uint32_t,
    bool);

template BarrierReference BarrierList::synchronizeDependency<BufferDependency>(
    const BufferDependency&,
    BarrierReference);
template BarrierReference BarrierList::synchronizeDependency<ImageDependency>(const ImageDependency&, BarrierReference);

}
