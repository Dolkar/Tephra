#pragma once

#include "logical_device.hpp"
#include "cross_queue_sync.hpp"
#include "queue_map.hpp"
#include "../job/accesses.hpp"
#include "../job/job_data.hpp"
#include "../common_impl.hpp"
#include <tephra/device.hpp>

namespace tp {

struct QueueSyncState {
    // TODO: Implement value-cached maps to save allocations
    std::unordered_map<VkBufferHandle, BufferAccessMap> bufferResourceMap;
    std::unordered_map<VkImageHandle, ImageAccessMap> imageResourceMap;

    // Storage of awaiting forgets of deleted resources
    Mutex awaitingForgetsMutex;
    std::deque<VkBufferHandle> awaitingBufferForgets;
    std::deque<VkImageHandle> awaitingImageForgets;
};

class QueueState {
public:
    QueueState(DeviceContainer* deviceImpl, uint32_t queueIndex);

    void enqueueJob(Job job);

    // Removes a resource from synchronization state when it's being deleted
    void forgetResource(VkBufferHandle vkBufferHandle);
    void forgetResource(VkImageHandle vkImageHandle);

    void submitQueuedJobs(const JobSemaphore& lastJobToSubmit);

    TEPHRA_MAKE_NONCOPYABLE(QueueState);
    TEPHRA_MAKE_MOVABLE_DEFAULT(QueueState);
    ~QueueState() = default;

private:
    DeviceContainer* deviceImpl;
    uint32_t queueIndex;

    std::deque<Job> queuedJobs;
    // Mutex guarding against simulateneous enqueue and submit
    Mutex queuedJobsMutex;
    std::unique_ptr<QueueSyncState> syncState;
    // For each (other) queue, stores the last timestamp that has been waited on
    std::vector<uint64_t> queueLastQueriedTimestamps;

    // Compiles and submits the given jobs
    void submitJobs(ArrayView<Job*> jobs);

    // Analyze cross-queue export commands in the job and broadcast them
    void broadcastResourceExports(const JobRecordStorage& jobRecord, const JobSemaphore& srcSemaphore);

    // Finds incoming resource exports from other queues
    void queryIncomingExports(const JobData* jobData, ScratchVector<CrossQueueSync::ExportEntry>& incomingExports);

    // Translates semaphores to a Vulkan submit batch
    void resolveSemaphores(const JobData* jobData, SubmitBatch& submitBatch) const;

    // Handles forgotten resources
    void consumeAwaitingForgets();
};

}
