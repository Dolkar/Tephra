#include "accesses.hpp"
#include "barriers.hpp"

namespace tp {

VkImageLayout vkGetImageLayoutForDescriptor(DescriptorType descriptorType, bool aliasStorageImage) {
    switch (descriptorType) {
    case DescriptorType::CombinedImageSampler:
    case DescriptorType::SampledImage:
        return aliasStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case DescriptorType::StorageImage:
        return VK_IMAGE_LAYOUT_GENERAL;
    case DescriptorType::TexelBuffer:
    case DescriptorType::StorageTexelBuffer:
    case DescriptorType::UniformBuffer:
    case DescriptorType::StorageBuffer:
    case DescriptorType::UniformBufferDynamic:
    case DescriptorType::StorageBufferDynamic:
    case DescriptorType::Sampler:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

void convertReadAccessToVkAccess(ReadAccessMask readMask, VkPipelineStageFlags* stageMask, VkAccessFlags* accessMask) {
    *stageMask = 0;
    *accessMask = 0;

    if (readMask.contains(ReadAccess::DrawIndirect)) {
        *stageMask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        *accessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (readMask.contains(ReadAccess::DrawIndex)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *accessMask |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (readMask.contains(ReadAccess::DrawVertex)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *accessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (readMask.contains(ReadAccess::Transfer)) {
        *stageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        *accessMask |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (readMask.contains(ReadAccess::Host)) {
        *stageMask |= VK_PIPELINE_STAGE_HOST_BIT;
        *accessMask |= VK_ACCESS_HOST_READ_BIT;
    }
    if (readMask.contains(ReadAccess::DepthStencilAttachment)) {
        *stageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        *stageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        *accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    if (readMask.containsAny(ReadAccess::VertexShaderStorage | ReadAccess::VertexShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (readMask.containsAny(
            ReadAccess::TessellationControlShaderStorage | ReadAccess::TessellationControlShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (readMask.containsAny(
            ReadAccess::TessellationEvaluationShaderStorage | ReadAccess::TessellationEvaluationShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (readMask.containsAny(ReadAccess::GeometryShaderStorage | ReadAccess::GeometryShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (readMask.containsAny(ReadAccess::FragmentShaderStorage | ReadAccess::FragmentShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (readMask.containsAny(ReadAccess::ComputeShaderStorage | ReadAccess::ComputeShaderSampled)) {
        *stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
    }

    if (readMask.contains(ReadAccess::VertexShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::TessellationControlShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::TessellationEvaluationShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::GeometryShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::FragmentShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::ComputeShaderUniform)) {
        *stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (readMask.contains(ReadAccess::ImagePresentKHR)) {
        *stageMask |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        *accessMask |= 0;
    }
    if (readMask.contains(ReadAccess::Unknown)) {
        *stageMask |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        *accessMask |= VK_ACCESS_MEMORY_READ_BIT;
    }
}

VkImageLayout vkGetImageLayoutFromReadAccess(ReadAccessMask readMask) {
    if (readMask.contains(ReadAccess::Transfer))
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (readMask.contains(ReadAccess::DepthStencilAttachment))
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (readMask.contains(ReadAccess::ImagePresentKHR))
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    if (readMask.containsAny(
            ReadAccess::VertexShaderSampled | ReadAccess::TessellationControlShaderSampled |
            ReadAccess::TessellationEvaluationShaderSampled | ReadAccess::GeometryShaderSampled |
            ReadAccess::FragmentShaderSampled | ReadAccess::ComputeShaderSampled))
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (readMask.containsAny(
            ReadAccess::VertexShaderStorage | ReadAccess::TessellationControlShaderStorage |
            ReadAccess::TessellationEvaluationShaderStorage | ReadAccess::GeometryShaderStorage |
            ReadAccess::FragmentShaderStorage | ReadAccess::ComputeShaderStorage | ReadAccess::Unknown))
        return VK_IMAGE_LAYOUT_GENERAL;

    TEPHRA_ASSERTD(false, "Invalid read access for an image resource");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

void convertComputeAccessToVkAccess(
    ComputeAccessMask computeMask,
    VkPipelineStageFlags* stageMask,
    VkAccessFlags* accessMask,
    bool* isAtomic) {
    *stageMask = 0;
    *accessMask = 0;
    *isAtomic = true;

    if (computeMask.containsAny(ComputeAccess::ComputeShaderSampledRead | ComputeAccess::ComputeShaderStorageRead)) {
        *stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
        *isAtomic &= false;
    }
    if (computeMask.containsAny(ComputeAccess::ComputeShaderStorageWrite | ComputeAccess::ComputeShaderStorageAtomic)) {
        *stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *accessMask |= VK_ACCESS_SHADER_WRITE_BIT;
        *isAtomic &= computeMask.contains(ComputeAccess::ComputeShaderStorageAtomic);
    }
    if (computeMask.contains(ComputeAccess::ComputeShaderUniformRead)) {
        *stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
        *isAtomic &= false;
    }
}

VkImageLayout vkGetImageLayoutFromComputeAccess(ComputeAccessMask computeMask) {
    if (computeMask.contains(ComputeAccess::ComputeShaderSampledRead)) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else {
        return VK_IMAGE_LAYOUT_GENERAL;
    }
}

void convertRenderAccessToVkAccess(
    RenderAccessMask renderMask,
    VkPipelineStageFlags* stageMask,
    VkAccessFlags* accessMask,
    bool* isAtomic) {
    *stageMask = 0;
    *accessMask = 0;
    *isAtomic = true;

    if (renderMask.contains(RenderAccess::DrawIndexRead)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *accessMask |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (renderMask.contains(RenderAccess::DrawVertexRead)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *accessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (renderMask.contains(RenderAccess::DrawIndirectRead)) {
        *stageMask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        *accessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderSampledRead | RenderAccess::VertexShaderStorageRead |
            RenderAccess::TessellationControlShaderSampledRead | RenderAccess::TessellationControlShaderStorageRead |
            RenderAccess::TessellationEvaluationShaderSampledRead |
            RenderAccess::TessellationEvaluationShaderStorageRead | RenderAccess::FragmentShaderSampledRead |
            RenderAccess::FragmentShaderStorageRead)) {
        *accessMask |= VK_ACCESS_SHADER_READ_BIT;
        *isAtomic &= false;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderStorageWrite | RenderAccess::TessellationControlShaderStorageWrite |
            RenderAccess::TessellationEvaluationShaderStorageWrite | RenderAccess::FragmentShaderStorageWrite)) {
        *accessMask |= VK_ACCESS_SHADER_WRITE_BIT;
        *isAtomic &= false;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderStorageAtomic | RenderAccess::TessellationControlShaderStorageAtomic |
            RenderAccess::TessellationEvaluationShaderStorageAtomic | RenderAccess::FragmentShaderStorageAtomic)) {
        *accessMask |= VK_ACCESS_SHADER_WRITE_BIT;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderUniformRead | RenderAccess::TessellationControlShaderUniformRead |
            RenderAccess::TessellationEvaluationShaderUniformRead | RenderAccess::FragmentShaderUniformRead)) {
        *accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
        *isAtomic &= false;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderSampledRead | RenderAccess::VertexShaderStorageRead |
            RenderAccess::VertexShaderStorageWrite | RenderAccess::VertexShaderStorageAtomic |
            RenderAccess::VertexShaderUniformRead)) {
        *stageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }

    if (renderMask.containsAny(
            RenderAccess::TessellationControlShaderSampledRead | RenderAccess::TessellationControlShaderStorageRead |
            RenderAccess::TessellationControlShaderStorageWrite | RenderAccess::TessellationControlShaderStorageAtomic |
            RenderAccess::TessellationControlShaderUniformRead)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    }

    if (renderMask.containsAny(
            RenderAccess::TessellationEvaluationShaderSampledRead |
            RenderAccess::TessellationEvaluationShaderStorageRead |
            RenderAccess::TessellationEvaluationShaderStorageWrite |
            RenderAccess::TessellationEvaluationShaderStorageAtomic |
            RenderAccess::TessellationEvaluationShaderUniformRead)) {
        *stageMask |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }

    if (renderMask.containsAny(
            RenderAccess::FragmentShaderSampledRead | RenderAccess::FragmentShaderStorageRead |
            RenderAccess::FragmentShaderStorageWrite | RenderAccess::FragmentShaderStorageAtomic |
            RenderAccess::FragmentShaderUniformRead)) {
        *stageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
}

VkImageLayout vkGetImageLayoutFromRenderAccess(RenderAccessMask renderMask) {
    if (renderMask.containsAny(
            RenderAccess::VertexShaderSampledRead | RenderAccess::VertexShaderStorageRead |
            RenderAccess::TessellationControlShaderSampledRead | RenderAccess::TessellationControlShaderStorageRead |
            RenderAccess::TessellationEvaluationShaderSampledRead |
            RenderAccess::TessellationEvaluationShaderStorageRead | RenderAccess::FragmentShaderSampledRead |
            RenderAccess::FragmentShaderStorageRead)) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderStorageWrite | RenderAccess::TessellationControlShaderStorageWrite |
            RenderAccess::TessellationEvaluationShaderStorageWrite | RenderAccess::FragmentShaderStorageWrite)) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    if (renderMask.containsAny(
            RenderAccess::VertexShaderStorageAtomic | RenderAccess::TessellationControlShaderStorageAtomic |
            RenderAccess::TessellationEvaluationShaderStorageAtomic | RenderAccess::FragmentShaderStorageAtomic)) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    TEPHRA_ASSERTD(false, "Invalid render access for an image resource");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

// Calculates the intersection of two subresource ranges
BufferAccessRange getAccessRangeIntersection(const BufferAccessRange& a, const BufferAccessRange& b) {
    BufferAccessRange result = a;

    if (b.offset > result.offset) {
        result.size -= tp::min(b.offset - result.offset, result.size);
        result.offset = b.offset;
    }

    if (b.offset + b.size < result.offset + result.size) {
        result.size = tp::max(b.offset + b.size, result.offset) - result.offset;
    }

    return result;
}

ImageAccessRange getAccessRangeIntersection(const ImageAccessRange& a, const ImageAccessRange& b) {
    ImageAccessRange result = a;

    if (b.baseArrayLayer > result.baseArrayLayer) {
        result.arrayLayerCount -= tp::min(b.baseArrayLayer - result.baseArrayLayer, result.arrayLayerCount);
        result.baseArrayLayer = b.baseArrayLayer;
    }

    if (b.baseArrayLayer + b.arrayLayerCount < result.baseArrayLayer + result.arrayLayerCount) {
        result.arrayLayerCount = tp::max(b.baseArrayLayer + b.arrayLayerCount, result.baseArrayLayer) -
            result.baseArrayLayer;
    }

    result.aspectMask &= b.aspectMask;
    result.mipLevelMask &= b.mipLevelMask;
    return result;
}

// Returns the left subrange of a with the intersection of a and b removed
BufferAccessRange getAccessRangeDifferenceLeft(const BufferAccessRange& a, const BufferAccessRange& b) {
    BufferAccessRange result = a;

    if (b.offset < result.offset + result.size) {
        result.size = tp::max(b.offset, result.offset) - result.offset;
    }

    return result;
}

ImageAccessRange getAccessRangeDifferenceLeft(const ImageAccessRange& a, const ImageAccessRange& b) {
    ImageAccessRange result = a;

    if (b.baseArrayLayer < result.baseArrayLayer + result.arrayLayerCount) {
        result.arrayLayerCount = tp::max(b.baseArrayLayer, result.baseArrayLayer) - result.baseArrayLayer;
    }

    return result;
}

// Returns the right subrange of a with the intersection of a and b removed
BufferAccessRange getAccessRangeDifferenceRight(const BufferAccessRange& a, const BufferAccessRange& b) {
    BufferAccessRange result = a;

    if (b.offset + b.size > result.offset) {
        result.size -= tp::min(b.offset + b.size - result.offset, result.size);
        result.offset = b.offset + b.size;
    }

    return result;
}

ImageAccessRange getAccessRangeDifferenceRight(const ImageAccessRange& a, const ImageAccessRange& b) {
    ImageAccessRange result = a;

    if (b.baseArrayLayer + b.arrayLayerCount > result.baseArrayLayer) {
        result.arrayLayerCount -= tp::min(
            b.baseArrayLayer + b.arrayLayerCount - result.baseArrayLayer, result.arrayLayerCount);
        result.baseArrayLayer = b.baseArrayLayer + b.arrayLayerCount;
    }

    return result;
}

ResourceAccess& ResourceAccess::operator|=(const ResourceAccess& other) {
    stageMask |= other.stageMask;
    accessMask |= other.accessMask;
    return *this;
}

ResourceAccess operator|(const ResourceAccess& a, const ResourceAccess& b) {
    return ResourceAccess(a.stageMask | b.stageMask, a.accessMask | b.accessMask);
}

std::pair<VkBufferHandle, BufferAccessRange> resolveBufferAccess(const BufferView& bufferView) {
    BufferAccessRange range = { 0, bufferView.getSize() };
    VkBufferHandle vkBufferHandle = resolveBufferAccess(bufferView, &range);
    return { vkBufferHandle, range };
}

VkBufferHandle resolveBufferAccess(const BufferView& bufferView, BufferAccessRange* range) {
    uint64_t viewOffset;
    VkBufferHandle vkBufferHandle = bufferView.vkResolveBufferHandle(&viewOffset);
    TEPHRA_ASSERTD(
        !vkBufferHandle.isNull(), "All accessed buffers should have an underlying buffer assigned at this point.");
    TEPHRA_ASSERT(range->offset + range->size <= bufferView.getSize());

    range->offset += viewOffset;
    return vkBufferHandle;
}

VkImageHandle resolveImageAccess(const ImageView& imageView, ImageAccessRange* range) {
    uint32_t viewBaseMipLevel;
    uint32_t viewBaseArrayLevel;
    VkImageHandle vkImageHandle = imageView.vkResolveImageHandle(&viewBaseMipLevel, &viewBaseArrayLevel);
    TEPHRA_ASSERTD(
        !vkImageHandle.isNull(), "All accessed images should have an underlying image assigned at this point.");
    TEPHRA_ASSERT(range->baseArrayLayer + range->arrayLayerCount <= imageView.getWholeRange().arrayLayerCount);
    TEPHRA_ASSERT(tp::log2(range->mipLevelMask) <= imageView.getWholeRange().mipLevelCount);

    range->baseArrayLayer += viewBaseArrayLevel;
    range->mipLevelMask <<= viewBaseMipLevel;
    return vkImageHandle;
}

ImageAccessRange::ImageAccessRange(const ImageSubresourceRange& range)
    : aspectMask(range.aspectMask), baseArrayLayer(range.baseArrayLayer), arrayLayerCount(range.arrayLayerCount) {
    TEPHRA_ASSERT(range.mipLevelCount + range.baseMipLevel < 32);
    mipLevelMask = ((1 << range.mipLevelCount) - 1) << range.baseMipLevel;
}

BufferAccessMap::BufferAccessMap(VkBufferHandle vkBufferHandle) : vkBufferHandle(vkBufferHandle) {
    clear();
}

uint64_t BufferAccessMap::getAccessCount() const {
    return accessMap.size();
}

void BufferAccessMap::synchronizeNewAccess(
    const NewBufferAccess& newAccess,
    uint32_t commandIndex,
    BarrierList& barriers) {
    if (lastJobId != barriers.getJobId()) {
        // Lazy barrier reset
        resetBarriers();
        lastJobId = barriers.getJobId();
    }

    // Iterate over all overlapping ranges
    auto rangeIts = accessMap.equal_range(newAccess.range);

    for (auto& it = rangeIts.first; it != rangeIts.second; ++it) {
        const BufferAccessRange& entryRange = it->first;
        BufferRangeEntry& entry = it->second;
        TEPHRA_ASSERT(areAccessRangesOverlapping(entryRange, newAccess.range));

        if (newAccess.isReadOnly()) {
            // Read accesses have a dependency on the last write access
            if (containsAllBits(entry.lastReadAccesses.stageMask, newAccess.stageMask) &&
                containsAllBits(entry.lastReadAccesses.accessMask, newAccess.accessMask)) {
                // Nothing to synchronize
                continue;
            }

            if (!entry.lastWriteAccess.isNull()) {
                // Define a new read after write dependency over the whole previous access range
                auto readAfterWriteDependency = BufferDependency(
                    vkBufferHandle, entryRange, entry.lastWriteAccess, newAccess);

                if (!entry.barrierAfterWriteAccess.isNull()) {
                    entry.barrierAfterWriteAccess = barriers.synchronizeDependency(
                        readAfterWriteDependency, entry.barrierAfterWriteAccess);
                } else {
                    entry.barrierAfterWriteAccess = barriers.synchronizeDependency(
                        readAfterWriteDependency, commandIndex, entry.barrierIndexAfterWriteAccess, entry.wasExported);
                }
            }
        } else {
            // Write accesses have dependencies on both the previous read accesses and the last write access
            BufferAccessRange intersectionRange = getAccessRangeIntersection(entryRange, newAccess.range);
            TEPHRA_ASSERT(!intersectionRange.isNull());
            BarrierReference lastBarrier;

            if (!entry.lastReadAccesses.isNull()) {
                auto writeAfterReadDependency = BufferDependency(
                    vkBufferHandle, intersectionRange, entry.lastReadAccesses, newAccess);
                lastBarrier = barriers.synchronizeDependency(
                    writeAfterReadDependency, commandIndex, entry.barrierIndexAfterReadAccesses, entry.wasExported);
            }

            if (!entry.lastWriteAccess.isNull()) {
                auto writeAfterWriteDependency = BufferDependency(
                    vkBufferHandle, intersectionRange, entry.lastWriteAccess, newAccess);

                // For the write after write dependency, try to reuse one of the existing barriers
                if (!entry.barrierAfterWriteAccess.isNull()) {
                    barriers.synchronizeDependency(writeAfterWriteDependency, entry.barrierAfterWriteAccess);
                } else if (!lastBarrier.isNull()) {
                    barriers.synchronizeDependency(writeAfterWriteDependency, lastBarrier);
                } else {
                    barriers.synchronizeDependency(
                        writeAfterWriteDependency, commandIndex, entry.barrierIndexAfterWriteAccess, entry.wasExported);
                }
            }
        }
    }
}

void BufferAccessMap::insertNewAccess(
    const NewBufferAccess& newAccess,
    uint32_t nextBarrierIndex,
    bool forceOverwrite,
    bool isExport) {
    TEPHRA_ASSERT(!newAccess.isNull());
    TEPHRA_ASSERT(!newAccess.range.isNull());
    TEPHRA_ASSERT(!isExport || newAccess.isReadOnly());

    // Iterate over all overlapping ranges
    auto rangeIts = accessMap.equal_range(newAccess.range);
    if (newAccess.isReadOnly() && !forceOverwrite) {
        // Read accesses don't subdivide previous accesses, just extend them
        for (auto& it = rangeIts.first; it != rangeIts.second; ++it) {
            BufferRangeEntry& entry = it->second;

            entry.lastReadAccesses = entry.lastReadAccesses | newAccess;
            entry.barrierIndexAfterReadAccesses = nextBarrierIndex;
            entry.wasExported = entry.wasExported || isExport;
        }
    } else {
        // Erase all overlapping ranges
        auto& it = rangeIts.first;
        while (it != rangeIts.second) {
            it = removeOverlappingRange(it, newAccess.range);
        }

        // With the space for the new access free, add its entry
        accessMap.emplace_hint(
            rangeIts.second, newAccess.range, BufferRangeEntry(newAccess, nextBarrierIndex, isExport));
    }
}

void BufferAccessMap::resetBarriers() {
    for (auto& mapPair : accessMap) {
        BufferRangeEntry& entry = mapPair.second;
        entry.barrierIndexAfterReadAccesses = 0;
        entry.barrierIndexAfterWriteAccess = 0;
        entry.barrierAfterWriteAccess = BarrierReference();
    }
}

void BufferAccessMap::clear() {
    accessMap.clear();

    // Initialize the access map to the given access
    // We don't know the actual size of the buffer, so improvise
    auto wholeRange = BufferAccessRange(0, ~0ull);
    auto defaultEntry = BufferRangeEntry({}, 0, false);
    accessMap.emplace(wholeRange, defaultEntry);
}

BufferAccessMap::AccessMapType::iterator BufferAccessMap::removeOverlappingRange(
    const BufferAccessMap::AccessMapType::iterator& entryIt,
    const BufferAccessRange& overlappingRange) {
    BufferAccessRange entryRange = entryIt->first;
    BufferRangeEntry entry = entryIt->second;

    // Erase the overlapping range
    AccessMapType::iterator it = accessMap.erase(entryIt);

    // But keep the non-overlapping parts, splitting the range if necessary
    BufferAccessRange leftRange = getAccessRangeDifferenceLeft(entryRange, overlappingRange);
    if (!leftRange.isNull()) {
        accessMap.emplace_hint(it, leftRange, entry);
    }
    BufferAccessRange rightRange = getAccessRangeDifferenceRight(entryRange, overlappingRange);
    if (!rightRange.isNull()) {
        accessMap.emplace_hint(it, rightRange, entry);
    }

    return it;
}

ImageAccessMap::ImageAccessMap(VkImageHandle vkImageHandle) : vkImageHandle(vkImageHandle) {
    clear();
}

uint64_t ImageAccessMap::getAccessCount() const {
    return accessMap.size();
}

void ImageAccessMap::synchronizeNewAccess(const NewImageAccess& newAccess, uint32_t commandIndex, BarrierList& barriers) {
    if (lastJobId != barriers.getJobId()) {
        // Lazy compact and barrier reset
        compactAndResetBarriers();
        lastJobId = barriers.getJobId();
    }

    // Iterate over all ranges, besides the ones we add
    std::size_t origSize = accessMap.size();
    for (std::size_t i = 0; i < origSize; i++) {
        ImageAccessRange& entryRange = accessMap[i].first;
        ImageRangeEntry& entry = accessMap[i].second;
        if (!areAccessRangesOverlapping(newAccess.range, entryRange)) {
            continue;
        }

        // Treat layout transition accesses as write accesses
        bool needsLayoutTransition = newAccess.layout != entry.layout && newAccess.layout != VK_IMAGE_LAYOUT_UNDEFINED;
        if (newAccess.isReadOnly() && !needsLayoutTransition) {
            // Read accesses have a dependency on the last write access
            if (containsAllBits(entry.lastReadAccesses.stageMask, newAccess.stageMask) &&
                containsAllBits(entry.lastReadAccesses.accessMask, newAccess.accessMask)) {
                // Nothing to synchronize
                continue;
            }

            if (!entry.lastWriteAccess.isNull()) {
                // Define a new read after write dependency over the whole previous access range
                auto readAfterWriteDependency = ImageDependency(
                    vkImageHandle, entryRange, entry.lastWriteAccess, newAccess, entry.layout, newAccess.layout);

                if (!entry.barrierAfterWriteAccess.isNull()) {
                    entry.barrierAfterWriteAccess = barriers.synchronizeDependency(
                        readAfterWriteDependency, entry.barrierAfterWriteAccess);
                } else {
                    entry.barrierAfterWriteAccess = barriers.synchronizeDependency(
                        readAfterWriteDependency, commandIndex, entry.barrierIndexAfterWriteAccess, entry.wasExported);
                }
            }
        } else {
            // Write accesses have dependencies on both the previous read accesses and the last write access
            ImageAccessRange intersectionRange = getAccessRangeIntersection(entryRange, newAccess.range);
            TEPHRA_ASSERT(!intersectionRange.isNull());
            BarrierReference lastBarrier;

            if (!entry.lastReadAccesses.isNull()) {
                auto writeAfterReadDependency = ImageDependency(
                    vkImageHandle, intersectionRange, entry.lastReadAccesses, newAccess, entry.layout, newAccess.layout);
                lastBarrier = barriers.synchronizeDependency(
                    writeAfterReadDependency, commandIndex, entry.barrierIndexAfterReadAccesses, entry.wasExported);
            }

            if (!entry.lastWriteAccess.isNull()) {
                // Don't transition if we're already gonna transition for the read dependency
                auto writeAfterWriteDependency = ImageDependency(
                    vkImageHandle, intersectionRange, entry.lastWriteAccess, newAccess, entry.layout, newAccess.layout);

                // For the write after write dependency, try to reuse one of the existing barriers
                if (!entry.barrierAfterWriteAccess.isNull() && !needsLayoutTransition) {
                    barriers.synchronizeDependency(writeAfterWriteDependency, entry.barrierAfterWriteAccess);
                } else if (!lastBarrier.isNull()) {
                    barriers.synchronizeDependency(writeAfterWriteDependency, lastBarrier);
                } else {
                    lastBarrier = barriers.synchronizeDependency(
                        writeAfterWriteDependency, commandIndex, entry.barrierIndexAfterWriteAccess, entry.wasExported);
                }
            }

            if (needsLayoutTransition) {
                if (lastBarrier.isNull()) {
                    // Layout transition but no previous access to sync against - just transition
                    ResourceAccess noneAccess = ResourceAccess(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);
                    auto transitionDependency = ImageDependency(
                        vkImageHandle, intersectionRange, noneAccess, newAccess, entry.layout, newAccess.layout);
                    lastBarrier = barriers.synchronizeDependency(
                        transitionDependency, commandIndex, entry.barrierIndexAfterWriteAccess, entry.wasExported);
                }

                if (newAccess.isReadOnly()) {
                    TEPHRA_ASSERT(!lastBarrier.isNull());
                    // Read access with layout transition. Synchronized with past accesses as if it was a write access
                    // Normally we can't reuse the barrier used to synchronize a write access, but for a layout
                    // transition we can, so record it for the entry's range.
                    // We split the overlapping range and overwrite its barrier:
                    splitOverlappingRange(i, newAccess.range);
                    accessMap[i].second.barrierAfterWriteAccess = lastBarrier;
                }
            }
        }
    }
}

void ImageAccessMap::insertNewAccess(
    const NewImageAccess& newAccess,
    uint32_t nextBarrierIndex,
    bool forceOverwrite,
    bool isExport) {
    TEPHRA_ASSERT(!newAccess.isNull());
    TEPHRA_ASSERT(!newAccess.range.isNull());
    TEPHRA_ASSERT(!isExport || newAccess.isReadOnly());

    if (newAccess.isReadOnly() && !forceOverwrite) {
        // Read accesses don't subdivide previous accesses, just extend them, except when they need
        // an image layout transition, then they partly act like write accesses
        for (auto& entryPair : accessMap) {
            const ImageAccessRange& entryRange = entryPair.first;
            ImageRangeEntry& entry = entryPair.second;
            if (!areAccessRangesOverlapping(newAccess.range, entryRange))
                continue;

            bool hadLayoutTransition = newAccess.layout != entry.layout &&
                newAccess.layout != VK_IMAGE_LAYOUT_UNDEFINED;
            if (!hadLayoutTransition) {
                entry.lastReadAccesses = entry.lastReadAccesses | newAccess;
                entry.barrierIndexAfterReadAccesses = nextBarrierIndex;
                entry.wasExported = entry.wasExported || isExport;
            } else {
                // Read access with layout transition. Treat the transition as a new write access, but keep the
                // references to the original transition barrier (if it exists), so we can potentially reuse it later.
                entry.lastWriteAccess = static_cast<tp::ResourceAccess>(newAccess);
                entry.lastReadAccesses = static_cast<tp::ResourceAccess>(newAccess);
                entry.barrierIndexAfterReadAccesses = nextBarrierIndex;
                entry.wasExported = isExport;
                entry.layout = newAccess.layout;
            }
        }
    } else {
        // Erase all overlapping ranges and insert the entry
        bool hasAddedEntry = false;
        std::size_t origSize = accessMap.size();
        for (std::size_t i = 0; i < origSize; i++) {
            if (areAccessRangesOverlapping(newAccess.range, accessMap[i].first)) {
                splitOverlappingRange(i, newAccess.range);

                if (!hasAddedEntry) {
                    accessMap[i] = { newAccess.range,
                                     ImageRangeEntry(newAccess, nextBarrierIndex, newAccess.layout, isExport) };
                    hasAddedEntry = true;
                } else {
                    accessMap[i].first = {};
                }
            }
        }
        TEPHRA_ASSERT(hasAddedEntry);
    }
}

void ImageAccessMap::discardContents(const ImageAccessRange& range) {
    std::size_t origSize = accessMap.size();
    for (std::size_t i = 0; i < origSize; i++) {
        if (areAccessRangesOverlapping(range, accessMap[i].first) &&
            accessMap[i].second.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
            // Split the overlapping range and reset its layout
            splitOverlappingRange(i, range);
            accessMap[i].second.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }
}

void ImageAccessMap::clear() {
    accessMap.clear();

    // Initialize the access map to set the layout of the entire image to undefined
    auto defaultEntry = ImageRangeEntry({}, 0, VK_IMAGE_LAYOUT_UNDEFINED, false);
    // We don't know the actual range of the whole image, so improvise
    ImageAccessRange wholeRange = ImageAccessRange(
        ImageAspect::Color | ImageAspect::Depth | ImageAspect::Stencil, 0, ~0u, ~0u);
    accessMap.emplace_back(wholeRange, defaultEntry);
}

void ImageAccessMap::compactAndResetBarriers() {
    auto removeIt = std::remove_if(accessMap.begin(), accessMap.end(), [](auto& el) {
        auto& [entryRange, entry] = el;
        entry.barrierIndexAfterReadAccesses = 0;
        entry.barrierIndexAfterWriteAccess = 0;
        entry.barrierAfterWriteAccess = BarrierReference();

        return entryRange.isNull();
    });

    accessMap.erase(removeIt, accessMap.end());
}

void ImageAccessMap::splitOverlappingRange(std::size_t entryIndex, const ImageAccessRange& overlappingRange) {
    ImageAccessRange entryRange = accessMap[entryIndex].first;
    ImageRangeEntry entry = accessMap[entryIndex].second;

    // Replace the entry's range with the intersecting one
    ImageAccessRange intersectionRange = getAccessRangeIntersection(entryRange, overlappingRange);
    TEPHRA_ASSERT(!intersectionRange.isNull());
    accessMap[entryIndex].first = intersectionRange;

    // But keep the non-overlapping parts, splitting the range if necessary
    if (entryRange.aspectMask != overlappingRange.aspectMask) {
        ImageAccessRange middleAspectRange = intersectionRange;
        middleAspectRange.aspectMask = entryRange.aspectMask & (~overlappingRange.aspectMask);
        accessMap.emplace_back(middleAspectRange, entry);
    }

    if (entryRange.mipLevelMask != overlappingRange.mipLevelMask) {
        ImageAccessRange middleMipRange = intersectionRange;
        middleMipRange.mipLevelMask = entryRange.mipLevelMask & (~overlappingRange.mipLevelMask);
        accessMap.emplace_back(middleMipRange, entry);
    }

    ImageAccessRange leftRange = getAccessRangeDifferenceLeft(entryRange, overlappingRange);
    if (!leftRange.isNull()) {
        accessMap.emplace_back(leftRange, entry);
    }

    ImageAccessRange rightRange = getAccessRangeDifferenceRight(entryRange, overlappingRange);
    if (!rightRange.isNull()) {
        accessMap.emplace_back(rightRange, entry);
    }
}

}
