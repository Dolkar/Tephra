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

    // Allocates the requested buffers
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
        const MemoryPreference& memoryPreference);

private:
    struct AssignInfo : ResourceUsageRange {
        uint64_t size;
        uint64_t alignment;
        JobLocalBufferImpl* resourcePtr;
    };

    DeviceContainer* deviceImpl;
    OverallocationBehavior overallocationBehavior;
    JobResourcePoolFlagMask poolFlags;
    // Pointers to the backing buffers along with the last used timestamp
    std::vector<std::pair<std::unique_ptr<Buffer>, uint64_t>> backingBuffers;
    uint64_t totalAllocationSize = 0;
    uint32_t totalAllocationCount = 0;

    // Allocate requested buffers, returns the number of bytes used
    uint64_t allocateJobBufferGroup(ArrayView<AssignInfo> buffersToAlloc, uint64_t currentTimestamp);

    // Allocate requested buffers, returns the number of bytes used
    uint64_t allocateJobBufferGroupNoAlias(ArrayView<AssignInfo> buffersToAlloc, uint64_t currentTimestamp);
};

}
