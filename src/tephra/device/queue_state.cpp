
#include "queue_state.hpp"
#include "../job/resource_pool_container.hpp"
#include "../job/job_compile.hpp"
#include "../job/command_recording.hpp"
#include <algorithm>

namespace tp {

QueueState::QueueState(DeviceContainer* deviceImpl, uint32_t queueIndex)
    : deviceImpl(deviceImpl), queueIndex(queueIndex), syncState(std::make_unique<QueueSyncState>()) {
    TEPHRA_ASSERT(queueIndex != ~0);
    queueLastQueriedTimestamps.resize(deviceImpl->getQueueMap()->getQueueInfos().size());
}

void QueueState::enqueueJob(Job job) {
    JobResourcePoolContainer::allocateJobResources(job);

    JobData* jobData = JobResourcePoolContainer::getJobData(job);
    broadcastResourceExports(jobData->record, jobData->signalJobSemaphore);

    queuedJobs.push_back(std::move(job));
}

void QueueState::forgetResource(VkBufferHandle vkBufferHandle) {
    std::lock_guard<Mutex> mutexLock(syncState->awaitingForgetsMutex);
    syncState->awaitingBufferForgets.push_back(vkBufferHandle);
}

void QueueState::forgetResource(VkImageHandle vkImageHandle) {
    std::lock_guard<Mutex> mutexLock(syncState->awaitingForgetsMutex);
    syncState->awaitingImageForgets.push_back(vkImageHandle);
}

void QueueState::submitQueuedJobs() {
    if (queuedJobs.empty()) {
        return;
    }
    consumeAwaitingForgets();

    // Set up for job compilation
    const QueueInfo& queueInfo = deviceImpl->getQueueMap()->getQueueInfos()[queueIndex];
    CommandPool* commandPool = deviceImpl->getCommandPoolPool()->acquirePool(
        queueInfo.identifier.type, queueInfo.name.c_str());

    SubmitBatch submitBatch;
    submitBatch.submitEntries.reserve(queuedJobs.size());

    const auto& vkiCommands = deviceImpl->getCommandPoolPool()->getVkiCommands();
    PrimaryBufferRecorder recorder = PrimaryBufferRecorder(
        commandPool, &vkiCommands, queueInfo.name.c_str(), &submitBatch.vkCommandBuffers);

    JobCompilationContext compilationContext;
    compilationContext.deviceImpl = deviceImpl;
    compilationContext.queueSyncState = syncState.get();
    compilationContext.recorder = &recorder;
    ScratchVector<CrossQueueSync::ExportEntry> incomingResourceExports;

    // Compile queued jobs into vulkan commands while building up submit information
    std::size_t startJobIndex = 0;
    while (startJobIndex < queuedJobs.size()) {
        // Process as many jobs as we can in the same submit
        std::size_t endJobIndex = startJobIndex + 1;
        while (endJobIndex < queuedJobs.size()) {
            Job& job = queuedJobs[endJobIndex];
            JobData* jobData = JobResourcePoolContainer::getJobData(job);

            // Putting this in the same submit would cause the previous jobs to wait, too
            bool hasWaits = !jobData->waitJobSemaphores.empty() || !jobData->waitExternalSemaphores.empty();
            // Jobs always signal a semaphore, but if it's flagged as small, assume it won't significantly delay it
            if (!jobData->flags.contains(tp::JobFlag::Small) || hasWaits)
                break;
        }

        // Populate a new submit entry
        submitBatch.submitEntries.emplace_back();
        SubmitBatch::SubmitEntry& submitEntry = submitBatch.submitEntries.back();
        submitEntry.waitSemaphoreOffset = static_cast<uint32_t>(submitBatch.vkWaitSemaphores.size());
        submitEntry.signalSemaphoreOffset = static_cast<uint32_t>(submitBatch.vkSignalSemaphores.size());
        submitEntry.commandBufferOffset = static_cast<uint32_t>(submitBatch.vkCommandBuffers.size());

        for (std::size_t jobIndex = startJobIndex; jobIndex < endJobIndex; jobIndex++) {
            Job& job = queuedJobs[jobIndex];
            JobData* jobData = JobResourcePoolContainer::getJobData(job);

            // Set up semaphores
            resolveSemaphores(jobData, submitBatch);

            incomingResourceExports.clear();
            queryIncomingExports(jobData, incomingResourceExports);

            // Compile the job to Vulkan command buffers
            compileJob(compilationContext, job, view(incomingResourceExports));
        }

        // Finalize the entry
        recorder.endRecording();
        submitEntry.waitSemaphoreCount = static_cast<uint32_t>(
            submitBatch.vkWaitSemaphores.size() - submitEntry.waitSemaphoreOffset);
        submitEntry.signalSemaphoreCount = static_cast<uint32_t>(
            submitBatch.vkSignalSemaphores.size() - submitEntry.signalSemaphoreOffset);
        submitEntry.commandBufferCount = static_cast<uint32_t>(
            submitBatch.vkCommandBuffers.size() - submitEntry.commandBufferOffset);

        startJobIndex = endJobIndex;
    }

    // Finally submit the batch
    deviceImpl->getLogicalDevice()->queueSubmit(queueIndex, submitBatch);

    // Queue the release of the primary command pool once the jobs finish
    deviceImpl->getTimelineManager()->addCleanupCallback(
        [=]() { deviceImpl->getCommandPoolPool()->releasePool(commandPool); });

    while (!queuedJobs.empty()) {
        JobResourcePoolContainer::queueReleaseSubmittedJob(std::move(queuedJobs.front()));
        queuedJobs.pop_front();
    }

    // TODO: Check as validation perf warning if any resource has too many distinct accesses
}

void QueueState::broadcastResourceExports(const JobRecordStorage& jobRecord, const JobSemaphore& srcSemaphore) {
    // Iterate over export commands and broadcast them through the cross queue sync object
    auto* cmd = jobRecord.firstCommandPtr;
    while (cmd != nullptr) {
        switch (cmd->commandType) {
        case JobCommandTypes::ExportBuffer: {
            auto* data = getCommandData<JobRecordStorage::ExportBufferData>(cmd);
            if (data->dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED ||
                data->dstQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL)
                break;

            auto [vkBufferHandle, range] = resolveBufferAccess(data->buffer);
            ResourceAccess access;
            convertReadAccessToVkAccess(data->readAccessMask, &access.stageMask, &access.accessMask);

            deviceImpl->getCrossQueueSync()->broadcastResourceExport(
                srcSemaphore, NewBufferAccess(vkBufferHandle, range, access), data->dstQueueFamilyIndex);
            break;
        }
        case JobCommandTypes::ExportImage: {
            auto* data = getCommandData<JobRecordStorage::ExportImageData>(cmd);
            if (data->dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED ||
                data->dstQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL)
                break;

            ImageAccessRange range = data->range;
            VkImageHandle vkImageHandle = resolveImageAccess(data->image, &range);
            ResourceAccess access;
            convertReadAccessToVkAccess(data->readAccessMask, &access.stageMask, &access.accessMask);
            VkImageLayout layout = vkGetImageLayoutFromReadAccess(data->readAccessMask);

            deviceImpl->getCrossQueueSync()->broadcastResourceExport(
                srcSemaphore, NewImageAccess(vkImageHandle, range, access, layout), data->dstQueueFamilyIndex);
            break;
        }
        default:
            break;
        }
        cmd = cmd->nextCommand;
    }
}

void QueueState::queryIncomingExports(
    const JobData* jobData,
    ScratchVector<CrossQueueSync::ExportEntry>& incomingExports) {
    ScratchVector<uint64_t> queueDstTimestamps(deviceImpl->getQueueMap()->getQueueInfos().size());

    // Set passed timestamps
    for (uint32_t i = 0; i < queueDstTimestamps.size(); i++) {
        queueDstTimestamps[i] = deviceImpl->getTimelineManager()->getLastReachedTimestamp(i);
    }

    // Set explicitly waited timestamps
    for (const JobSemaphore& jobSemaphore : jobData->waitJobSemaphores) {
        uint32_t semaphoreQueueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(jobSemaphore.queue);
        queueDstTimestamps[semaphoreQueueIndex] = tp::max(
            queueDstTimestamps[semaphoreQueueIndex], jobSemaphore.timestamp);
    }

    // Form timeline periods to query
    ScratchVector<TimelinePeriod> periods;
    periods.reserve(queueDstTimestamps.size());
    for (uint32_t i = 0; i < queueDstTimestamps.size(); i++) {
        if (queueDstTimestamps[i] > queueLastQueriedTimestamps[i]) {
            TimelinePeriod period;
            period.srcQueue = deviceImpl->getQueueMap()->getQueueInfos()[i].identifier;
            period.fromTimestamp = queueLastQueriedTimestamps[i];
            period.toTimestamp = queueDstTimestamps[i];
            periods.push_back(period);
            queueLastQueriedTimestamps[i] = queueDstTimestamps[i];
        }
    }

    uint32_t dstQueueFamilyIndex = deviceImpl->getQueueMap()->getQueueInfos()[queueIndex].queueFamilyIndex;
    deviceImpl->getCrossQueueSync()->queryIncomingExports(view(periods), dstQueueFamilyIndex, incomingExports);
}

void QueueState::resolveSemaphores(const JobData* jobData, SubmitBatch& submitBatch) const {
    // Reduce to one job semaphore per queue
    ScratchVector<uint32_t> queueIndices;
    queueIndices.reserve(jobData->waitJobSemaphores.size());
    ScratchVector<uint64_t> queueTimestamps;
    queueTimestamps.reserve(jobData->waitJobSemaphores.size());

    for (const JobSemaphore& jobSemaphore : jobData->waitJobSemaphores) {
        uint32_t semaphoreQueueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(jobSemaphore.queue);

        bool added = false;
        for (std::size_t i = 0; i < queueIndices.size(); i++) {
            if (queueIndices[i] == semaphoreQueueIndex) {
                queueTimestamps[i] = tp::max(queueTimestamps[i], jobSemaphore.timestamp);

                added = true;
                break;
            }
        }

        if (!added) {
            queueIndices.push_back(semaphoreQueueIndex);
            queueTimestamps.push_back(jobSemaphore.timestamp);
        }
    }

    // Fill the submit batch with job semaphores. Assume the offset has already been set.
    for (std::size_t i = 0; i < queueIndices.size(); i++) {
        VkSemaphoreHandle vkTimelineSemaphore = deviceImpl->getTimelineManager()->vkGetQueueSemaphoreHandle(
            queueIndices[i]);
        submitBatch.vkWaitSemaphores.push_back(vkTimelineSemaphore);
        submitBatch.waitSemaphoreValues.push_back(queueTimestamps[i]);
        // TODO: Determine the latest stage to wait on from job accesses somehow
        submitBatch.vkWaitStageFlags.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

    for (const ExternalSemaphore& externalSemaphore : jobData->waitExternalSemaphores) {
        submitBatch.vkWaitSemaphores.push_back(externalSemaphore.vkSemaphoreHandle);
        submitBatch.waitSemaphoreValues.push_back(externalSemaphore.timestamp);
        submitBatch.vkWaitStageFlags.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

    // Add the one internal signal semaphore
    TEPHRA_ASSERT(queueIndex == deviceImpl->getQueueMap()->getQueueUniqueIndex(jobData->signalJobSemaphore.queue));

    VkSemaphoreHandle vkTimelineSemaphore = deviceImpl->getTimelineManager()->vkGetQueueSemaphoreHandle(queueIndex);
    submitBatch.vkSignalSemaphores.push_back(vkTimelineSemaphore);
    submitBatch.signalSemaphoreValues.push_back(jobData->signalJobSemaphore.timestamp);

    for (const ExternalSemaphore& externalSemaphore : jobData->signalExternalSemaphores) {
        submitBatch.vkSignalSemaphores.push_back(externalSemaphore.vkSemaphoreHandle);
        submitBatch.signalSemaphoreValues.push_back(externalSemaphore.timestamp);
    }
}

void QueueState::consumeAwaitingForgets() {
    std::lock_guard<Mutex> mutexLock(syncState->awaitingForgetsMutex);

    for (VkBufferHandle vkBufferHandle : syncState->awaitingBufferForgets) {
        syncState->bufferResourceMap.erase(vkBufferHandle);
    }
    syncState->awaitingBufferForgets.clear();
    for (VkImageHandle vkImageHandle : syncState->awaitingImageForgets) {
        syncState->imageResourceMap.erase(vkImageHandle);
    }
    syncState->awaitingImageForgets.clear();
}
}
