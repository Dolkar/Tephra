#pragma once

#include "../common_impl.hpp"
#include <tephra/utils/growable_ring_buffer.hpp>

namespace tp {

// Allocator for job preinitialized buffers. Part of the job resource pool, it holds allocations for multiple jobs.
// Uses GrowableRingBuffer to suballocate buffers for every usage and memory preference combination.
// Allocations for a job are freed all at once to be reused.
class PreinitializedBufferAllocator {
public:
    PreinitializedBufferAllocator(
        DeviceContainer* deviceImpl,
        const OverallocationBehavior& overallocationBehavior,
        JobResourcePoolFlagMask poolFlags);

    // Allocates the requested buffer for the given job id
    BufferView allocateJobBuffer(
        uint64_t jobId,
        const BufferSetup& bufferSetup,
        const MemoryPreference& memoryPreference,
        const char* debugName);

    // Notifies the allocator that no more allocations for this job id will come
    void finalizeJobAllocations(uint64_t jobId, const char* jobName);

    // Frees all allocations made with this job id to be reused
    void freeJobAllocations(uint64_t jobId);

    // Frees memory from unused backing buffers
    void trim();

    uint32_t getAllocationCount() const {
        return totalAllocationCount;
    }

    uint64_t getTotalSize() const {
        return totalAllocationSize;
    }

private:
    // Specifies that no recording job is making use of that group
    static constexpr uint64_t NoJobRecordingId = ~0ull;

    struct BackingBufferGroup {
        MemoryPreference memoryPreference;
        uint64_t recordingJobId = NoJobRecordingId;
        std::vector<utils::GrowableRingBuffer> ringBuffers;
        std::vector<std::unique_ptr<Buffer>> backingBuffers;
        uint64_t recordingJobRequestedBytes = 0;
    };

    struct BufferAllocation {
        uint32_t backingGroupIndex;
        uint32_t locationIndex;
    };

    DeviceContainer* deviceImpl;
    OverallocationBehavior overallocationBehavior;
    JobResourcePoolFlagMask poolFlags;
    std::vector<BackingBufferGroup> backingBufferGroups;
    std::vector<std::pair<uint64_t, std::vector<BufferAllocation>>> jobAllocationsList;
    uint64_t totalAllocationSize = 0;
    uint32_t totalAllocationCount = 0;

    // Satisfy a buffer allocation request from a specific backing group, returns also the index of the ring buffer
    // used
    std::pair<BufferView, uint32_t> allocateBufferFromGroup(
        BackingBufferGroup& backingGroup,
        uint64_t jobId,
        const BufferSetup& bufferSetup,
        const char* debugName,
        bool dontSuballocate);
};

}
