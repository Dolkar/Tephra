#include "preinit_buffer_allocator.hpp"

#include "local_buffer_allocator.hpp"
#include "resource_pool_container.hpp"
#include "../device/device_container.hpp"

namespace tp {

PreinitializedBufferAllocator::PreinitializedBufferAllocator(
    DeviceContainer* deviceImpl,
    const OverallocationBehavior& overallocationBehavior,
    JobResourcePoolFlagMask poolFlags)
    : deviceImpl(deviceImpl), overallocationBehavior(overallocationBehavior), poolFlags(poolFlags) {
    if (poolFlags.contains(JobResourcePoolFlag::DisableSuballocation)) {
        // change overallocation behavior to exact allocations since we can't suballocoate
        this->overallocationBehavior = OverallocationBehavior::Exact();
    }
}

BufferView PreinitializedBufferAllocator::allocateJobBuffer(
    uint64_t jobId,
    const BufferSetup& bufferSetup,
    const MemoryPreference& memoryPreference,
    const char* debugName) {
    TEPHRA_ASSERT(jobId != NoJobRecordingId);

    // Find the right backing buffer group for this allocation
    std::size_t backingGroupIndex = 0;
    for (; backingGroupIndex < backingBufferGroups.size(); backingGroupIndex++) {
        auto& group = backingBufferGroups[backingGroupIndex];
        if (group.memoryPreference.hash == memoryPreference.hash && group.usageMask == bufferSetup.usage &&
            (group.recordingJobId == NoJobRecordingId || group.recordingJobId == jobId)) {
            break;
        }
    }
    if (backingGroupIndex == backingBufferGroups.size()) {
        backingBufferGroups.emplace_back();
        BackingBufferGroup& backingGroup = backingBufferGroups.back();
        backingGroup.usageMask = bufferSetup.usage;
        backingGroup.memoryPreference = memoryPreference;

        // Create one ring buffer for each memory location, by order of progression
        int locationIndex = 0;
        for (; locationIndex < MemoryLocationEnumView::size(); locationIndex++) {
            if (backingGroup.memoryPreference.locationProgression[locationIndex] == MemoryLocation::Undefined)
                break;
        }
        backingGroup.ringBuffers.resize(locationIndex);
    }

    bool dontSuballocate = poolFlags.contains(JobResourcePoolFlag::DisableSuballocation);
    std::pair<BufferView, uint32_t> allocation = allocateBufferFromGroup(
        backingBufferGroups[backingGroupIndex], jobId, bufferSetup, debugName, dontSuballocate);

    if (dontSuballocate) {
        // Without suballocation we can name the buffer
        uint64_t allocationOffset;
        deviceImpl->getLogicalDevice()->setObjectDebugName(
            allocation.first.vkResolveBufferHandle(&allocationOffset), debugName);
        TEPHRA_ASSERT(allocationOffset == 0);
    }

    // Remember the allocation
    BufferAllocation allocationInfo;
    allocationInfo.backingGroupIndex = static_cast<uint32_t>(backingGroupIndex);
    allocationInfo.locationIndex = allocation.second;

    std::vector<BufferAllocation>* jobAllocations = nullptr;
    for (auto& item : jobAllocationsList) {
        if (item.first == NoJobRecordingId || item.first == jobId) {
            item.first = jobId;
            jobAllocations = &item.second;
            break;
        }
    }
    if (jobAllocations == nullptr) {
        jobAllocationsList.emplace_back(jobId, std::vector<BufferAllocation>());
        jobAllocations = &jobAllocationsList.back().second;
    }

    jobAllocations->push_back(allocationInfo);

    return allocation.first;
}

void PreinitializedBufferAllocator::finalizeJobAllocations(uint64_t jobId, const char* jobName) {
    TEPHRA_ASSERT(jobId != NoJobRecordingId);

    uint64_t bufferBytesRequested = 0;

    // The job has been enqueued and no more allocations will be made for it,
    // so we can reuse the ring buffers for other jobs.
    for (auto& group : backingBufferGroups) {
        if (group.recordingJobId == jobId) {
            group.recordingJobId = NoJobRecordingId;

            if constexpr (StatisticEventsEnabled) {
                bufferBytesRequested += group.recordingJobRequestedBytes;
                group.recordingJobRequestedBytes = 0;
            }
        }
    }

    if constexpr (StatisticEventsEnabled) {
        reportStatisticEvent(StatisticEventType::JobPreinitBufferRequestedBytes, bufferBytesRequested, jobName);
    }
}

void PreinitializedBufferAllocator::freeJobAllocations(uint64_t jobId) {
    // Pop all allocations from the ring buffers
    for (auto& item : jobAllocationsList) {
        if (item.first == jobId) {
            for (const BufferAllocation& allocation : item.second) {
                auto& ringBuffer = backingBufferGroups[allocation.backingGroupIndex]
                                       .ringBuffers[allocation.locationIndex];
                ringBuffer.pop();
            }
            item.first = NoJobRecordingId;
            item.second.clear();
            // No break here, it's not guaranteed that allocations of one job are all in the same item
        }
    }
}

void PreinitializedBufferAllocator::trim() {
    for (BackingBufferGroup& backingGroup : backingBufferGroups) {
        // Don't free buffers for jobs that we're still recording
        if (backingGroup.recordingJobId != NoJobRecordingId)
            continue;

        auto& backingBuffers = backingGroup.backingBuffers;
        for (utils::GrowableRingBuffer& ringBuffer : backingGroup.ringBuffers) {
            while (Buffer* freedBuffer = ringBuffer.shrink()) {
                TEPHRA_ASSERT(totalAllocationSize >= freedBuffer->getSize());
                TEPHRA_ASSERT(totalAllocationCount >= 1);
                totalAllocationSize -= freedBuffer->getSize();
                totalAllocationCount--;

                // Destroy the handles immediately, since we already know the buffer isn't being used
                static_cast<BufferImpl*>(freedBuffer)->destroyHandles(true);

                auto freedBufferIt = std::find_if(
                    backingBuffers.begin(), backingBuffers.end(), [freedBuffer](const std::unique_ptr<Buffer>& el) {
                        return el.get() == freedBuffer;
                    });
                TEPHRA_ASSERT(freedBufferIt != backingBuffers.end());
                backingBuffers.erase(freedBufferIt);
            }
        }
    }
}

std::pair<BufferView, uint32_t> PreinitializedBufferAllocator::allocateBufferFromGroup(
    BackingBufferGroup& backingGroup,
    uint64_t jobId,
    const BufferSetup& bufferSetup,
    const char* debugName,
    bool dontSuballocate) {
    // Claim the backing group for this job
    backingGroup.recordingJobId = jobId;
    backingGroup.recordingJobRequestedBytes += bufferSetup.size;

    // Try to allocate from existing ring buffers without growing them
    for (int locationIndex = 0; locationIndex < backingGroup.ringBuffers.size(); locationIndex++) {
        utils::GrowableRingBuffer& ringBuffer = backingGroup.ringBuffers[locationIndex];

        BufferView view;
        if (dontSuballocate)
            view = ringBuffer.pushNoSuballocate(bufferSetup.size);
        else
            view = ringBuffer.push(bufferSetup.size);
        if (!view.isNull()) {
            return std::make_pair(view, locationIndex);
        }
    }

    // Allocation failed, create a new buffer in this group to allocate from
    uint64_t currentBackingGroupSize = 0;
    for (const utils::GrowableRingBuffer& ringBackingBuffer : backingGroup.ringBuffers) {
        currentBackingGroupSize += ringBackingBuffer.getTotalSize();
    }

    // TODO: Handle out of memory exception, fallback to allocating a smaller buffer
    uint64_t sizeToAlloc = overallocationBehavior.apply(bufferSetup.size, currentBackingGroupSize);
    uint64_t backingBufferIndex = backingGroup.backingBuffers.size();
    backingGroup.backingBuffers.push_back(JobLocalBufferAllocator::allocateBackingBuffer(
        deviceImpl, sizeToAlloc, backingGroup.usageMask, backingGroup.memoryPreference));
    Buffer* backingBuffer = backingGroup.backingBuffers[backingBufferIndex].get();
    totalAllocationCount++;
    totalAllocationSize += backingBuffer->getSize();

    // Find the memory location index in the memory preference progression and assign the new backing buffer for
    // this location
    MemoryLocation backingMemoryLocation = backingBuffer->getMemoryLocation();
    int locationIndex = 0;
    for (; locationIndex < MemoryLocationEnumView::size(); locationIndex++) {
        if (backingMemoryLocation == backingGroup.memoryPreference.locationProgression[locationIndex])
            break;
    }
    TEPHRA_ASSERT(locationIndex < backingGroup.ringBuffers.size());

    // Assign the backing buffer to the ring buffer implementation
    utils::GrowableRingBuffer& ringBuffer = backingGroup.ringBuffers[locationIndex];
    ringBuffer.grow(backingBuffer);

    // Allocate buffers from the presized ring buffer
    BufferView view = ringBuffer.push(bufferSetup.size);
    TEPHRA_ASSERTD(!view.isNull(), "Ring buffer allocation failed after growing it.");

    return std::make_pair(view, locationIndex);
}
}
