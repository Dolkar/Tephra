#include "local_buffer_allocator.hpp"
#include "../device/device_container.hpp"
#include "../buffer_impl.hpp"
#include <tephra/job.hpp>
#include <algorithm>

namespace tp {

JobLocalBufferAllocator::JobLocalBufferAllocator(
    DeviceContainer* deviceImpl,
    const OverallocationBehavior& overallocationBehavior,
    JobResourcePoolFlagMask poolFlags)
    : deviceImpl(deviceImpl), overallocationBehavior(overallocationBehavior), poolFlags(poolFlags) {}

void JobLocalBufferAllocator::allocateJobBuffers(
    JobLocalBuffers* bufferResources,
    uint64_t currentTimestamp,
    const char* jobName) {
    ScratchVector<AssignInfo> assignInfos;
    assignInfos.reserve(bufferResources->buffers.size());

    // Process the requested buffers
    uint64_t bufferBytesRequested = 0;
    for (int i = 0; i < bufferResources->buffers.size(); i++) {
        const JobLocalBufferImpl& localBuffer = bufferResources->buffers[i];
        const BufferSetup& bufferSetup = localBuffer.getBufferSetup();
        bufferBytesRequested += bufferSetup.size;

        const ResourceUsageRange& bufLocalUsage = bufferResources->usageRanges[i];
        if (bufLocalUsage.firstUsage == ~0) {
            // Buffer is never used so ignore it
            continue;
        }

        AssignInfo assignInfo;
        assignInfo.firstUsage = bufLocalUsage.firstUsage;
        assignInfo.lastUsage = bufLocalUsage.lastUsage;
        assignInfo.size = bufferSetup.size;
        assignInfo.usageMask = bufferSetup.usage;
        assignInfo.resourcePtr = &bufferResources->buffers[i];
        assignInfos.push_back(assignInfo);
    }

    if (assignInfos.empty())
        return;

    // Allocate and assign the job buffers
    uint64_t bufferBytesCommitted;
    if (!poolFlags.contains(JobResourcePoolFlag::DisableSuballocation))
        bufferBytesCommitted = allocateJobBufferGroup(view(assignInfos), currentTimestamp);
    else
        bufferBytesCommitted = allocateJobBufferGroupNoAlias(view(assignInfos), currentTimestamp);

    if constexpr (StatisticEventsEnabled) {
        reportStatisticEvent(StatisticEventType::JobLocalBufferRequestedBytes, bufferBytesRequested, jobName);
        reportStatisticEvent(StatisticEventType::JobLocalBufferCommittedBytes, bufferBytesCommitted, jobName);
    }

    bufferResources->createPendingBufferViews();
}

void JobLocalBufferAllocator::trim(uint64_t upToTimestamp) {
    auto removeIt = std::remove_if(
        backingBuffers.begin(),
        backingBuffers.end(),
        [&totalAllocationSize = this->totalAllocationSize,
         &totalAllocationCount = this->totalAllocationCount,
         upToTimestamp](const auto& el) {
            const auto& [backingBuffer, lastUseTimestamp] = el;
            bool trimmable = lastUseTimestamp <= upToTimestamp;

            if (trimmable) {
                TEPHRA_ASSERT(totalAllocationSize >= backingBuffer->getSize());
                TEPHRA_ASSERT(totalAllocationCount >= 1);
                totalAllocationSize -= backingBuffer->getSize();
                totalAllocationCount--;
                // Destroy the handles immediately, since we already know the buffer isn't being used
                static_cast<BufferImpl*>(backingBuffer.get())->destroyHandles(true);
            }
            return trimmable;
        });
    backingBuffers.erase(removeIt, backingBuffers.end());
}

std::unique_ptr<Buffer> JobLocalBufferAllocator::allocateBackingBuffer(
    DeviceContainer* deviceImpl,
    uint64_t sizeToAllocate,
    const MemoryPreference& memoryPreference) {
    // Assume that buffer usage only affects alignment, meaning it's ok to include usages that aren't actually needed,
    // provided that the allocated buffers are large enough.
    BufferUsageMask usageMask = BufferUsage::ImageTransfer | BufferUsage::HostMapped | BufferUsage::TexelBuffer |
        BufferUsage::UniformBuffer | BufferUsage::StorageBuffer | BufferUsage::IndexBuffer | BufferUsage::VertexBuffer |
        BufferUsage::IndirectBuffer;
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(tp::Functionality::BufferDeviceAddress))
        usageMask |= BufferUsage::DeviceAddress;

    BufferSetup backingBufferSetup = BufferSetup(sizeToAllocate, usageMask);
    auto [bufferHandleLifeguard, allocationHandleLifeguard] = deviceImpl->getMemoryAllocator()->allocateBuffer(
        backingBufferSetup, memoryPreference);

    return std::make_unique<BufferImpl>(
        deviceImpl,
        backingBufferSetup,
        std::move(bufferHandleLifeguard),
        std::move(allocationHandleLifeguard),
        DebugTarget::makeSilent());
}

uint64_t JobLocalBufferAllocator::allocateJobBufferGroup(
    ArrayView<AssignInfo> buffersToAlloc,
    uint64_t currentTimestamp) {
    // Suballocate the buffers from the backing allocations with aliasing
    ScratchVector<uint64_t> backingBufferSizes;
    backingBufferSizes.reserve(backingBuffers.size());
    for (const auto& [backingBuffer, lastUseTimestamp] : backingBuffers) {
        backingBufferSizes.push_back(backingBuffer->getSize());
    }

    AliasingSuballocator suballocator(view(backingBufferSizes));

    // Sort buffers in descending order by their size for more efficient memory allocations
    std::sort(buffersToAlloc.begin(), buffersToAlloc.end(), [](const AssignInfo& left, const AssignInfo& right) {
        return left.size > right.size;
    });

    // Index and offset of leftover buffers that didn't fit
    ScratchVector<std::pair<int, uint64_t>> leftoverBuffers;
    leftoverBuffers.reserve(buffersToAlloc.size());
    uint64_t leftoverSize = 0;

    for (int i = 0; i < buffersToAlloc.size(); i++) {
        uint64_t requiredAlignment = BufferImpl::getRequiredViewAlignment_(deviceImpl, buffersToAlloc[i].usageMask);

        auto [backingBufferIndex, offset] = suballocator.allocate(
            buffersToAlloc[i].size, ResourceUsageRange(buffersToAlloc[i]), requiredAlignment);
        if (backingBufferIndex < backingBuffers.size()) {
            // The allocation fits - assign and update timestamp
            auto& [backingBuffer, lastUseTimestamp] = backingBuffers[backingBufferIndex];
            buffersToAlloc[i].resourcePtr->assignUnderlyingBuffer(backingBuffer.get(), offset);
            lastUseTimestamp = currentTimestamp;
        } else {
            // It doesn't, remember it so we can allocate a new backing buffer for it
            leftoverBuffers.emplace_back(i, offset);
            leftoverSize = tp::max(leftoverSize, offset + buffersToAlloc[i].size);
        }
    }

    if (leftoverBuffers.empty())
        return suballocator.getUsedSize();

    // Some of the buffers still haven't been assigned. Create a new backing buffer to host them.
    uint64_t currentBackingGroupSize = 0;
    for (auto& [backingBuffer, lastUseTimestamp] : backingBuffers) {
        currentBackingGroupSize += backingBuffer->getSize();
    }

    // TODO: Handle out of memory exception, fallback to allocating a smaller buffer
    uint64_t sizeToAlloc = overallocationBehavior.apply(leftoverSize, currentBackingGroupSize);
    std::pair<std::unique_ptr<Buffer>, uint64_t> newEntry = std::make_pair(
        allocateBackingBuffer(deviceImpl, sizeToAlloc, MemoryPreference::Device), currentTimestamp);
    Buffer* newBackingBuffer = newEntry.first.get();
    totalAllocationSize += newBackingBuffer->getSize();
    totalAllocationCount++;

    // Insert the new backing buffer to the list so that the largest buffer appears first
    auto pos = std::find_if(backingBuffers.begin(), backingBuffers.end(), [sizeToAlloc](const auto& entry) {
        return entry.first->getSize() < sizeToAlloc;
    });
    backingBuffers.insert(pos, std::move(newEntry));

    // Assign the leftover resources to the new backing buffer
    for (auto& [bufferIndex, offset] : leftoverBuffers) {
        buffersToAlloc[bufferIndex].resourcePtr->assignUnderlyingBuffer(newBackingBuffer, offset);
    }

    return suballocator.getUsedSize();
}

uint64_t JobLocalBufferAllocator::allocateJobBufferGroupNoAlias(
    ArrayView<AssignInfo> buffersToAlloc,
    uint64_t currentTimestamp) {
    // Sort buffers in descending order by their size for more efficient memory allocations
    std::sort(buffersToAlloc.begin(), buffersToAlloc.end(), [](const AssignInfo& left, const AssignInfo& right) {
        return left.size > right.size;
    });

    ScratchVector<std::unique_ptr<Buffer>> newBackingBuffers;
    newBackingBuffers.reserve(buffersToAlloc.size());
    uint64_t totalSize = 0;

    int backingBufferIndex = 0;
    for (AssignInfo& bufferToAlloc : buffersToAlloc) {
        Buffer* backingBuffer;
        if (backingBufferIndex < backingBuffers.size() &&
            bufferToAlloc.size <= backingBuffers[backingBufferIndex].first->getSize()) {
            // Can reuse existing backing buffer
            backingBuffer = backingBuffers[backingBufferIndex].first.get();
            backingBuffers[backingBufferIndex].second = currentTimestamp;
            backingBufferIndex++;
        } else {
            // Create a new backing buffer of the exact size as requested
            newBackingBuffers.emplace_back(
                allocateBackingBuffer(deviceImpl, bufferToAlloc.size, MemoryPreference::Device));
            backingBuffer = newBackingBuffers.back().get();

            totalAllocationCount++;
            totalAllocationSize += backingBuffer->getSize();
        }

        deviceImpl->getLogicalDevice()->setObjectDebugName(
            backingBuffer->vkGetBufferHandle(), bufferToAlloc.resourcePtr->getDebugTarget()->getObjectName());
        bufferToAlloc.resourcePtr->assignUnderlyingBuffer(backingBuffer, 0);
        totalSize += backingBuffer->getSize();
    }

    // Insert the new backing buffers to the list so that the largest buffer appears first
    for (std::unique_ptr<Buffer>& newBackingBuffer : newBackingBuffers) {
        auto pos = std::find_if(backingBuffers.begin(), backingBuffers.end(), [&newBackingBuffer](const auto& entry) {
            return entry.first->getSize() < newBackingBuffer->getSize();
        });
        backingBuffers.insert(pos, std::make_pair(std::move(newBackingBuffer), currentTimestamp));
    }

    return totalSize;
}
}
