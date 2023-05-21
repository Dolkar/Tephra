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
    // Split the buffer resources into groups by usage and process each group separately
    ScratchVector<bool> processed;
    processed.resize(bufferResources->buffers.size(), false);
    ScratchVector<AssignInfo> groupAssignInfos;
    groupAssignInfos.reserve(bufferResources->buffers.size());

    uint64_t bufferBytesRequested = 0;
    uint64_t bufferBytesCommitted = 0;

    for (int i = 0; i < bufferResources->buffers.size(); i++) {
        if (processed[i])
            continue;

        // Find all other local buffer resources with the same usage masks
        const JobLocalBufferImpl& firstLocalBuffer = bufferResources->buffers[i];
        BufferUsageMask groupUsageMask = firstLocalBuffer.getBufferSetup().usage;

        for (int j = i; j < bufferResources->buffers.size(); j++) {
            const JobLocalBufferImpl& localBuffer = bufferResources->buffers[j];
            const BufferSetup& bufferSetup = localBuffer.getBufferSetup();
            if (bufferSetup.usage != groupUsageMask) {
                continue;
            }

            processed[j] = true;
            bufferBytesRequested += bufferSetup.size;

            const ResourceUsageRange& bufLocalUsage = bufferResources->usageRanges[j];
            if (bufLocalUsage.firstUsage == ~0) {
                // Buffer is never used so ignore it
                continue;
            }

            AssignInfo assignInfo;
            assignInfo.firstUsage = bufLocalUsage.firstUsage;
            assignInfo.lastUsage = bufLocalUsage.lastUsage;
            assignInfo.size = bufferSetup.size;
            assignInfo.resourcePtr = &bufferResources->buffers[j];
            groupAssignInfos.push_back(assignInfo);
        }
        if (groupAssignInfos.empty())
            continue;

        // Find a matching group of backing buffers or create one
        BackingBufferGroup* backingGroup = nullptr;
        for (auto& group : backingBufferGroups) {
            if (group.usageMask == groupUsageMask) {
                backingGroup = &group;
                break;
            }
        }
        if (backingGroup == nullptr) {
            backingBufferGroups.emplace_back();
            backingGroup = &backingBufferGroups.back();
            backingGroup->usageMask = groupUsageMask;
        }

        // Assign job buffers to this group
        if (!poolFlags.contains(JobResourcePoolFlag::DisableSuballocation))
            bufferBytesCommitted += allocateJobBufferGroup(backingGroup, view(groupAssignInfos), currentTimestamp);
        else
            bufferBytesCommitted += allocateJobBufferGroupNoAlias(
                backingGroup, view(groupAssignInfos), currentTimestamp);
        groupAssignInfos.clear();
    }

    if constexpr (StatisticEventsEnabled) {
        reportStatisticEvent(StatisticEventType::JobLocalBufferRequestedBytes, bufferBytesRequested, jobName);
        reportStatisticEvent(StatisticEventType::JobLocalBufferCommittedBytes, bufferBytesCommitted, jobName);
    }

    bufferResources->createPendingBufferViews();
}

void JobLocalBufferAllocator::trim(uint64_t upToTimestamp) {
    for (BackingBufferGroup& backingGroup : backingBufferGroups) {
        auto removeIt = std::remove_if(
            backingGroup.buffers.begin(),
            backingGroup.buffers.end(),
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
        backingGroup.buffers.erase(removeIt, backingGroup.buffers.end());
    }
}

std::unique_ptr<Buffer> JobLocalBufferAllocator::allocateBackingBuffer(
    DeviceContainer* deviceImpl,
    uint64_t sizeToAllocate,
    BufferUsageMask usageMask,
    const MemoryPreference& memoryPreference) {
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
    BackingBufferGroup* backingGroup,
    ArrayView<AssignInfo> buffersToAlloc,
    uint64_t currentTimestamp) {
    // Suballocate the buffers from the backing allocations with aliasing
    ScratchVector<uint64_t> backingBufferSizes;
    backingBufferSizes.reserve(backingGroup->buffers.size());
    for (const auto& [backingBuffer, lastUseTimestamp] : backingGroup->buffers) {
        backingBufferSizes.push_back(backingBuffer->getSize());
    }

    AliasingSuballocator suballocator(view(backingBufferSizes));

    // Sort buffers in descending order by their size for more efficient memory allocations
    std::sort(buffersToAlloc.begin(), buffersToAlloc.end(), [](const AssignInfo& left, const AssignInfo& right) {
        return left.size > right.size;
    });

    uint64_t requiredAlignment = BufferImpl::getRequiredViewAlignment_(deviceImpl, backingGroup->usageMask);

    // Index and offset of leftover buffers that didn't fit
    ScratchVector<std::pair<int, uint64_t>> leftoverBuffers;
    leftoverBuffers.reserve(buffersToAlloc.size());
    uint64_t leftoverSize = 0;

    for (int i = 0; i < buffersToAlloc.size(); i++) {
        auto [backingBufferIndex, offset] = suballocator.allocate(
            buffersToAlloc[i].size, ResourceUsageRange(buffersToAlloc[i]), requiredAlignment);
        if (backingBufferIndex < backingGroup->buffers.size()) {
            // The allocation fits - assign and update timestamp
            auto& [backingBuffer, lastUseTimestamp] = backingGroup->buffers[backingBufferIndex];
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
    for (auto& [backingBuffer, lastUseTimestamp] : backingGroup->buffers) {
        currentBackingGroupSize += backingBuffer->getSize();
    }

    // TODO: Handle out of memory exception, fallback to allocating a smaller buffer
    uint64_t sizeToAlloc = overallocationBehavior.apply(leftoverSize, currentBackingGroupSize);
    std::pair<std::unique_ptr<Buffer>, uint64_t> newEntry = std::make_pair(
        allocateBackingBuffer(deviceImpl, sizeToAlloc, backingGroup->usageMask, MemoryPreference::Device),
        currentTimestamp);
    Buffer* newBackingBuffer = newEntry.first.get();
    totalAllocationSize += newBackingBuffer->getSize();
    totalAllocationCount++;

    // Insert the new backing buffer to the list so that the largest buffer appears first
    auto pos = std::find_if(
        backingGroup->buffers.begin(), backingGroup->buffers.end(), [sizeToAlloc](const auto& entry) {
            return entry.first->getSize() < sizeToAlloc;
        });
    backingGroup->buffers.insert(pos, std::move(newEntry));

    // Assign the leftover resources to the new backing buffer
    for (auto& [bufferIndex, offset] : leftoverBuffers) {
        buffersToAlloc[bufferIndex].resourcePtr->assignUnderlyingBuffer(newBackingBuffer, offset);
    }

    return suballocator.getUsedSize();
}

uint64_t JobLocalBufferAllocator::allocateJobBufferGroupNoAlias(
    BackingBufferGroup* backingGroup,
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
        if (backingBufferIndex < backingGroup->buffers.size() &&
            bufferToAlloc.size <= backingGroup->buffers[backingBufferIndex].first->getSize()) {
            // Can reuse existing backing buffer
            backingBuffer = backingGroup->buffers[backingBufferIndex].first.get();
            backingGroup->buffers[backingBufferIndex].second = currentTimestamp;
            backingBufferIndex++;
        } else {
            // Create a new backing buffer of the exact size as requested
            newBackingBuffers.emplace_back(allocateBackingBuffer(
                deviceImpl, bufferToAlloc.size, backingGroup->usageMask, MemoryPreference::Device));
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
        auto pos = std::find_if(
            backingGroup->buffers.begin(), backingGroup->buffers.end(), [&newBackingBuffer](const auto& entry) {
                return entry.first->getSize() < newBackingBuffer->getSize();
            });
        backingGroup->buffers.insert(pos, std::make_pair(std::move(newBackingBuffer), currentTimestamp));
    }

    return totalSize;
}

}
