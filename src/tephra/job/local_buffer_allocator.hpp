#pragma once

#include "local_buffers.hpp"
#include "../common_impl.hpp"

namespace tp {

struct JobResourcePoolSetup;

class JobLocalBufferAllocator {
public:
    JobLocalBufferAllocator(
        DeviceContainer* deviceImpl,
        const OverallocationBehavior& overallocationBehavior,
        JobResourcePoolFlagMask poolFlags);

    // Allocates the requested buffer
    void allocateJobBuffers(JobLocalBuffers* bufferResources, uint64_t currentTimestamp, const char* jobName);

    // Frees all backing buffers that were last used up to the given timestamp
    void trim(uint64_t upToTimestamp);

    uint32_t getAllocationCount() const {
        return totalAllocationCount;
    }

    uint64_t getTotalSize() const {
        return totalAllocationSize;
    }

    // Helper function to allocate an internal backing buffer
    static std::unique_ptr<Buffer> allocateBackingBuffer(
        DeviceContainer* deviceImpl,
        uint64_t sizeToAllocate,
        BufferUsageMask usageMask,
        const MemoryPreference& memoryPreference);

private:
    struct BackingBufferGroup {
        BufferUsageMask usageMask;
        // Pointers to the backing buffers along with the last used timestamp
        std::vector<std::pair<std::unique_ptr<Buffer>, uint64_t>> buffers;
    };

    struct AssignInfo : ResourceUsageRange {
        uint64_t size;
        JobLocalBufferImpl* resourcePtr;
    };

    DeviceContainer* deviceImpl;
    OverallocationBehavior overallocationBehavior;
    JobResourcePoolFlagMask poolFlags;
    std::vector<BackingBufferGroup> backingBufferGroups;
    uint64_t totalAllocationSize = 0;
    uint32_t totalAllocationCount = 0;

    // Allocate requested buffers from the given backing group, returns the number of bytes used
    uint64_t allocateJobBufferGroup(
        BackingBufferGroup* backingGroup,
        ArrayView<AssignInfo> buffersToAlloc,
        uint64_t currentTimestamp);

    // Allocate requested buffers from the given backing group, returns the number of bytes used
    uint64_t allocateJobBufferGroupNoAlias(
        BackingBufferGroup* backingGroup,
        ArrayView<AssignInfo> buffersToAlloc,
        uint64_t currentTimestamp);
};

}
