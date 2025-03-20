#pragma once

#include "job_data.hpp"
#include "local_buffer_allocator.hpp"
#include "preinit_buffer_allocator.hpp"
#include "local_image_allocator.hpp"
#include "../descriptor_pool_impl.hpp"
#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"
#include <tephra/job.hpp>
#include <tephra/device.hpp>

namespace tp {

struct JobData;
// TODO: Separate functionality into JobPool

class JobResourcePoolContainer : public JobResourcePool {
public:
    JobResourcePoolContainer(DeviceContainer* deviceImpl, const JobResourcePoolSetup& setup, DebugTarget debugTarget);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    uint32_t getBaseQueueIndex() const {
        return baseQueueIndex;
    }

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    DeviceContainer* getParentDeviceImpl() {
        return deviceImpl;
    }

    PreinitializedBufferAllocator* getPreinitializedBufferPool() {
        return &preinitBufferPool;
    }

    DescriptorPoolImpl* getLocalDescriptorPool() {
        return &localDescriptorPool;
    }

    uint64_t trim_(const JobSemaphore& latestTrimmed);

    JobResourcePoolStatistics getStatistics_() const;

    Job acquireJob(JobFlagMask flags, const char* jobName);

    static JobData* getJobData(Job& job) {
        return job.jobData;
    }
    static const JobData* getJobData(const Job& job) {
        return job.jobData;
    }

    static void allocateJobResources(Job& job);

    static void queueReleaseJob(JobData* jobData);

    static const DebugTarget* getJobDebugTarget(const Job& job) {
        return job.debugTarget.get();
    }

    ~JobResourcePoolContainer();

private:
    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    uint32_t baseQueueIndex;
    uint64_t jobsAcquiredCount;

    JobLocalBufferAllocator localBufferPool;
    JobLocalImageAllocator localImagePool;
    PreinitializedBufferAllocator preinitBufferPool;
    DescriptorPoolImpl localDescriptorPool;

    ObjectPool<JobData> jobDataPool;
    // Access to the resource pool as a whole should be externally synchronized, but submitting and destroying jobs
    // should still be thread safe
    Mutex jobReleaseQueueMutex;
    std::deque<JobData*> jobReleaseQueue;

    void tryFreeSubmittedJobs();
};

}
