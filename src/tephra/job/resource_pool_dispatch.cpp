#include "resource_pool_container.hpp"
#include "../device/timeline_manager.hpp"
#include "../device/device_container.hpp"

namespace tp {

constexpr const char* JobTypeName = "Job";

JobResourcePoolSetup::JobResourcePoolSetup(
    DeviceQueue queue,
    JobResourcePoolFlagMask flags,
    OverallocationBehavior bufferOverallocationBehavior,
    OverallocationBehavior preinitBufferOverallocationBehavior,
    OverallocationBehavior descriptorOverallocationBehavior)
    : queue(queue),
      flags(flags),
      bufferOverallocationBehavior(bufferOverallocationBehavior),
      preinitBufferOverallocationBehavior(preinitBufferOverallocationBehavior),
      descriptorOverallocationBehavior(descriptorOverallocationBehavior) {}

Job JobResourcePool::createJob(JobFlagMask flags, const char* debugName) {
    auto poolImpl = static_cast<JobResourcePoolContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(poolImpl->getDebugTarget(), "createJob", debugName);

    return poolImpl->acquireJob(flags, debugName);
}

uint64_t JobResourcePool::trim(const JobSemaphore& latestTrimmed) {
    auto poolImpl = static_cast<JobResourcePoolContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(poolImpl->getDebugTarget(), "trim", nullptr);

    return poolImpl->trim_(latestTrimmed);
}

JobResourcePoolStatistics JobResourcePool::getStatistics() const {
    auto poolImpl = static_cast<const JobResourcePoolContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(poolImpl->getDebugTarget(), "getStatistics", nullptr);

    return poolImpl->getStatistics_();
}

JobResourcePoolContainer::JobResourcePoolContainer(
    DeviceContainer* deviceImpl,
    const JobResourcePoolSetup& setup,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      deviceImpl(deviceImpl),
      baseQueueIndex(deviceImpl->getQueueMap()->getQueueUniqueIndex(setup.queue)),
      jobsAcquiredCount(0),
      localBufferPool(deviceImpl, setup.bufferOverallocationBehavior, setup.flags),
      localImagePool(deviceImpl, setup.flags),
      preinitBufferPool(deviceImpl, setup.preinitBufferOverallocationBehavior, setup.flags),
      localDescriptorPool(
          deviceImpl,
          DescriptorPoolSetup(setup.descriptorOverallocationBehavior),
          baseQueueIndex,
          DebugTarget::makeSilent()) {}

uint64_t JobResourcePoolContainer::trim_(const JobSemaphore& latestTrimmed) {
    uint64_t upToTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestamp(baseQueueIndex);
    if (!latestTrimmed.isNull()) {
        upToTimestamp = tp::min(upToTimestamp, latestTrimmed.timestamp);
    }

    uint64_t startSize = getStatistics_().getTotalAllocationBytes();

    tryFreeSubmittedJobs();
    localBufferPool.trim(upToTimestamp);
    localImagePool.trim(upToTimestamp);
    // Preinitialized buffers don't support time limited trimming, will just free everything unused
    preinitBufferPool.trim();
    // Descriptor set pool doesn't support trimming at all

    uint64_t endSize = getStatistics_().getTotalAllocationBytes();
    TEPHRA_ASSERT(endSize <= startSize);
    return startSize - endSize;
}

JobResourcePoolStatistics JobResourcePoolContainer::getStatistics_() const {
    JobResourcePoolStatistics stats;
    stats.bufferAllocationCount = localBufferPool.getAllocationCount();
    stats.bufferAllocationBytes = localBufferPool.getTotalSize();
    stats.imageAllocationCount = localImagePool.getAllocationCount();
    stats.imageAllocationBytes = localImagePool.getTotalSize();
    stats.preinitBufferAllocationCount = preinitBufferPool.getAllocationCount();
    stats.preinitBufferAllocationBytes = preinitBufferPool.getTotalSize();
    return stats;
}

Job JobResourcePoolContainer::acquireJob(JobFlagMask flags, const char* jobName) {
    // Try free some preinitialized buffers
    tryFreeSubmittedJobs();

    JobData* jobData = jobDataPool.acquireExisting();
    if (jobData == nullptr) {
        jobData = jobDataPool.acquireNew(this);
    }
    jobData->jobIdInPool = jobsAcquiredCount++;
    jobData->flags = flags;

    auto jobDebugTarget = DebugTarget(deviceImpl->getDebugTarget(), JobTypeName, jobName);
    return Job(jobData, std::move(jobDebugTarget));
}

void JobResourcePoolContainer::allocateJobResources(Job& job) {
    JobData* jobData = job.jobData;
    const char* jobName = job.debugTarget->getObjectName();
    TEPHRA_ASSERT(jobData != nullptr);
    TEPHRA_ASSERT(jobData->resourcePoolImpl != nullptr);

    JobResourcePoolContainer* resourcePool = jobData->resourcePoolImpl;
    uint64_t jobTimestamp = jobData->semaphores.jobSignal.timestamp;

    job.finalize();
    resourcePool->tryFreeSubmittedJobs();
    resourcePool->localBufferPool.allocateJobBuffers(&jobData->resources.localBuffers, jobTimestamp, jobName);
    resourcePool->localImagePool.allocateJobImages(&jobData->resources.localImages, jobTimestamp, jobName);
    resourcePool->preinitBufferPool.finalizeJobAllocations(jobData->jobIdInPool, jobName);
    jobData->resources.localDescriptorSets.allocatePreparedDescriptorSets();

    // After allocations, resolve attachments of render passes
    for (std::size_t i = 0; i < jobData->record.renderPassCount; i++) {
        jobData->record.renderPassStorage[i].resolveAttachmentViews();
    }
}

void JobResourcePoolContainer::queueReleaseSubmittedJob(Job job) {
    // The only point of a Job object is to hold JobData and handle destruction while it's not yet submitted
    // Clearing the pointer will ensure it won't attempt to free its resources immediately once destroyed in this
    // method.
    JobData* jobData = job.jobData;
    TEPHRA_ASSERT(jobData != nullptr);
    TEPHRA_ASSERT(jobData->resourcePoolImpl != nullptr);
    job.jobData = nullptr;
    TEPHRA_ASSERT(!jobData->semaphores.jobSignal.isNull());

    queueReleaseJob(jobData);
}

void JobResourcePoolContainer::queueReleaseJob(JobData* jobData) {
    JobResourcePoolContainer* resourcePool = jobData->resourcePoolImpl;
    if (resourcePool == nullptr)
        return; // Orphaned job, nothing to do

    std::lock_guard<Mutex> mutexLock(resourcePool->jobReleaseQueueMutex);
    // Keep the queue approximately sorted by how early we can release the jobs
    if (jobData->semaphores.jobSignal.isNull())
        resourcePool->jobReleaseQueue.push_front(jobData);
    else
        resourcePool->jobReleaseQueue.push_back(jobData);
}

JobResourcePoolContainer::~JobResourcePoolContainer() {
    TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(&debugTarget);

    // Turn all child jobs into orphans
    for (JobData& jobData : jobDataPool.getAllocatedObjects()) {
        jobData.resourcePoolImpl = nullptr;
    }
}

void JobResourcePoolContainer::tryFreeSubmittedJobs() {
    // Cannot use callbacks because the job resource pool can be destroyed by the user
    ScratchVector<JobData*> jobsToRelease;

    {
        std::lock_guard<Mutex> mutexLock(jobReleaseQueueMutex);
        while (!jobReleaseQueue.empty()) {
            JobData* jobData = jobReleaseQueue.front();

            if (!jobData->semaphores.jobSignal.isNull() &&
                !deviceImpl->getTimelineManager()->wasTimestampReachedInQueue(
                    baseQueueIndex, jobData->semaphores.jobSignal.timestamp)) {
                break;
            }
            jobsToRelease.push_back(jobData);
            jobReleaseQueue.pop_front();
        }
    }

    // Free the allocated resources and put the jobs back in the pool for reuse outside of the lock
    for (JobData* jobData : jobsToRelease) {
        // Release the job's resources - preinitialized buffers, descriptor sets and command pools
        preinitBufferPool.freeJobAllocations(jobData->jobIdInPool);
        jobData->resources.localDescriptorSets.freeAllocatedDescriptorSets();

        for (CommandPool* commandPool : jobData->resources.commandPools) {
            getParentDeviceImpl()->getCommandPoolPool()->releasePool(commandPool);
        }
        jobData->resources.commandPools.clear();

        jobDataPool.release(jobData);
    }
}

}
