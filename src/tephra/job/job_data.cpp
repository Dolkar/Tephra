#include "job_data.hpp"
#include "resource_pool_container.hpp"
#include "../device/device_container.hpp"

namespace tp {

JobResourceStorage::JobResourceStorage(JobResourcePoolContainer* resourcePoolImpl)
    : localBuffers(resourcePoolImpl),
      localImages(resourcePoolImpl),
      localDescriptorSets(resourcePoolImpl->getLocalDescriptorPool()) {}

void JobResourceStorage::clear() {
    localBuffers.clear();
    localImages.clear();
    localDescriptorSets.clear();
    // Command pools must be released explicitly back to their pool, a clear won't do
    TEPHRA_ASSERT(commandPools.empty());
}

void JobRecordStorage::clear() {
    commandCount = 0;
    cmdBuffer.clear();
    firstCommandPtr = nullptr;
    lastCommandPtr = nullptr;

    computePassCount = 0;
    renderPassCount = 0;
}

JobData::JobData(JobResourcePoolContainer* resourcePoolImpl)
    : jobIdInPool(~0), resourcePoolImpl(resourcePoolImpl), resources(resourcePoolImpl) {}

void JobData::clear() {
    jobIdInPool = ~0;
    record.clear();
    resources.clear();
    waitJobSemaphores.clear();
    signalJobSemaphore = JobSemaphore();
    waitExternalSemaphores.clear();
    signalExternalSemaphores.clear();
}

}
