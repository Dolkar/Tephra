#include "resource_pool_container.hpp"
#include "job_compile.hpp"
#include "job_data.hpp"
#include "command_recording.hpp"
#include "accesses.hpp"
#include "barriers.hpp"
#include <unordered_map>
#include <deque>

namespace tp {

// Helper class for handling of resource export commands and batching them until the last possible moment
class ResourceExportHandler {
public:
    ResourceExportHandler(BarrierList* barriers, QueueSyncState* queueSyncState, uint32_t currentQueueFamilyIndex)
        : barriers(barriers), queueSyncState(queueSyncState), currentQueueFamilyIndex(currentQueueFamilyIndex) {}

    void addExport(JobRecordStorage::ExportBufferData& exportData) {
        auto [vkBufferHandle, range] = resolveBufferAccess(exportData.buffer);

        if (exportData.dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            exportData.dstQueueFamilyIndex != currentQueueFamilyIndex) {
            qfotBufferExports.emplace_back(
                NewBufferAccess(vkBufferHandle, range, exportData.access), exportData.dstQueueFamilyIndex);
        } else {
            queuedBufferExports.emplace_back(vkBufferHandle, range, exportData.access);
        }
    }

    void addExport(JobRecordStorage::ExportImageData& exportData) {
        ImageAccessRange range = exportData.range;
        VkImageHandle vkImageHandle = resolveImageAccess(exportData.image, &range);

        if (exportData.dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            exportData.dstQueueFamilyIndex != currentQueueFamilyIndex) {
            qfotImageExports.emplace_back(
                NewImageAccess(vkImageHandle, range, exportData.access, exportData.vkImageLayout),
                exportData.dstQueueFamilyIndex);
        } else {
            queuedImageExports.emplace_back(vkImageHandle, range, exportData.access, exportData.vkImageLayout);
        }
    }

    // Synchronize exports before a command that might use the exported resources
    void flushExports(uint32_t cmdIndex, VkPipelineStageFlags stageMask) {
        auto flushBufferExport = [this, cmdIndex, stageMask](NewBufferAccess& access) {
            // Ignore accesses that aren't part of the requested stages
            if ((stageMask & access.stageMask) == 0) {
                return false;
            }

            VkBufferHandle handle = access.vkResourceHandle;
            auto mapHit = queueSyncState->bufferResourceMap.find(handle);
            if (mapHit == queueSyncState->bufferResourceMap.end())
                mapHit = queueSyncState->bufferResourceMap.emplace(handle, BufferAccessMap(handle)).first;

            // The exports are treated like a special access
            mapHit->second.synchronizeNewAccess(access, cmdIndex, *barriers);
            mapHit->second.insertNewAccess(access, barriers->getBarrierCount(), false, true);
            return true;
        };

        auto flushImageExport = [this, cmdIndex, stageMask](NewImageAccess& access) {
            // Ignore accesses that aren't part of the requested stages
            if ((stageMask & access.stageMask) == 0) {
                return false;
            }

            VkImageHandle handle = access.vkResourceHandle;
            auto mapHit = queueSyncState->imageResourceMap.find(handle);
            if (mapHit == queueSyncState->imageResourceMap.end())
                mapHit = queueSyncState->imageResourceMap.emplace(handle, ImageAccessMap(handle)).first;

            // The exports are treated like a special access
            mapHit->second.synchronizeNewAccess(access, cmdIndex, *barriers);
            mapHit->second.insertNewAccess(access, barriers->getBarrierCount(), false, true);
            return true;
        };

        auto removeItBuffer = std::remove_if(queuedBufferExports.begin(), queuedBufferExports.end(), flushBufferExport);
        queuedBufferExports.erase(removeItBuffer, queuedBufferExports.end());

        auto removeItImage = std::remove_if(queuedImageExports.begin(), queuedImageExports.end(), flushImageExport);
        queuedImageExports.erase(removeItImage, queuedImageExports.end());
    }

    void finishSubmit() {
        ResourceAccess bottomOfPipeAccess = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0 };
        // Flush all remaining exports
        flushExports(~0, ~0);

        // Split the cross-queue exports into two barriers. The first transitions the resources to a known state, while
        // the second only does the queue family ownership transfer. This is a bit wasteful as sometimes we could have
        // combined these barriers together, but for imports in the destination queue (which may have already happened)
        // we need to know the exact range and layout of exported resources to insert a matching barrier.
        for (const auto& [access, dstQueueFamilyIndex] : qfotBufferExports) {
            VkBufferHandle handle = access.vkResourceHandle;
            auto mapHit = queueSyncState->bufferResourceMap.find(access.vkResourceHandle);
            if (mapHit == queueSyncState->bufferResourceMap.end())
                mapHit = queueSyncState->bufferResourceMap.emplace(handle, BufferAccessMap(handle)).first;

            // Use bottom of pipe access as it can be used in all queues
            auto exportAccess = NewBufferAccess(access.vkResourceHandle, access.range, bottomOfPipeAccess);
            mapHit->second.synchronizeNewAccess(exportAccess, ~0, *barriers);
            mapHit->second.insertNewAccess(exportAccess, barriers->getBarrierCount());
        }

        for (const auto& [access, dstQueueFamilyIndex] : qfotImageExports) {
            VkImageHandle handle = access.vkResourceHandle;
            auto mapHit = queueSyncState->imageResourceMap.find(access.vkResourceHandle);
            if (mapHit == queueSyncState->imageResourceMap.end())
                mapHit = queueSyncState->imageResourceMap.emplace(handle, ImageAccessMap(handle)).first;

            // Use bottom of pipe access as it can be used in all queues
            auto exportAccess = NewImageAccess(
                access.vkResourceHandle, access.range, bottomOfPipeAccess, access.layout);
            mapHit->second.synchronizeNewAccess(exportAccess, ~0, *barriers);
            // The image can now only be accessed from this queue by discarding its contents, so set undefined layout
            exportAccess.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            mapHit->second.insertNewAccess(exportAccess, barriers->getBarrierCount());
        }

        // Add pure QFOT release barriers
        for (const auto& [access, dstQueueFamilyIndex] : qfotBufferExports) {
            auto qfotDependency = BufferDependency(
                access.vkResourceHandle,
                access.range,
                bottomOfPipeAccess,
                bottomOfPipeAccess,
                currentQueueFamilyIndex,
                dstQueueFamilyIndex);
            barriers->synchronizeDependency(qfotDependency, ~0, barriers->getBarrierCount(), false);
        }

        for (const auto& [access, dstQueueFamilyIndex] : qfotImageExports) {
            auto qfotDependency = ImageDependency(
                access.vkResourceHandle,
                access.range,
                bottomOfPipeAccess,
                bottomOfPipeAccess,
                access.layout,
                access.layout,
                currentQueueFamilyIndex,
                dstQueueFamilyIndex);
            barriers->synchronizeDependency(qfotDependency, ~0, barriers->getBarrierCount(), false);
        }

        qfotBufferExports.clear();
        qfotImageExports.clear();
    }

    void processIncomingExports(ArrayParameter<const CrossQueueSync::ExportEntry> incomingExports) {
        TEPHRA_ASSERT(barriers->getBarrierCount() == 0);
        ResourceAccess topOfPipeAccess = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 };
        uint32_t nextBarrierIndex = 1; // First barrier will be the QFOT acquire barrier

        for (auto& exportEntry : incomingExports) {
            TEPHRA_ASSERT(exportEntry.dstQueueFamilyIndex == currentQueueFamilyIndex);

            if (std::holds_alternative<NewBufferAccess>(exportEntry.access)) {
                const NewBufferAccess& access = std::get<NewBufferAccess>(exportEntry.access);
                VkBufferHandle handle = access.vkResourceHandle;

                // Add the exported access
                auto mapHit = queueSyncState->bufferResourceMap.find(handle);
                if (mapHit == queueSyncState->bufferResourceMap.end())
                    mapHit = queueSyncState->bufferResourceMap.emplace(handle, BufferAccessMap(handle)).first;
                mapHit->second.insertNewAccess(access, nextBarrierIndex, true, true);

                // Add the QFOT acquire barrier
                auto qfotDependency = BufferDependency(
                    access.vkResourceHandle,
                    access.range,
                    topOfPipeAccess,
                    access,
                    exportEntry.currentQueueFamilyIndex,
                    exportEntry.dstQueueFamilyIndex);
                barriers->synchronizeDependency(qfotDependency, 0, 0, false);

            } else {
                const NewImageAccess& access = std::get<NewImageAccess>(exportEntry.access);
                VkImageHandle handle = access.vkResourceHandle;

                // Add the exported access
                auto mapHit = queueSyncState->imageResourceMap.find(handle);
                if (mapHit == queueSyncState->imageResourceMap.end())
                    mapHit = queueSyncState->imageResourceMap.emplace(handle, ImageAccessMap(handle)).first;
                mapHit->second.insertNewAccess(access, nextBarrierIndex, true, true);

                // Add the QFOT acquire barrier
                auto qfotDependency = ImageDependency(
                    access.vkResourceHandle,
                    access.range,
                    topOfPipeAccess,
                    access,
                    access.layout,
                    access.layout,
                    exportEntry.currentQueueFamilyIndex,
                    exportEntry.dstQueueFamilyIndex);
                barriers->synchronizeDependency(qfotDependency, 0, 0, false);
            }
        }
    }

private:
    BarrierList* barriers;
    QueueSyncState* queueSyncState;
    uint32_t currentQueueFamilyIndex;

    // Queued exports with their destination queue family (VK_QUEUE_FAMILY_IGNORED if local export)
    ScratchVector<NewBufferAccess> queuedBufferExports;
    ScratchVector<NewImageAccess> queuedImageExports;
    // Queued exports that still need a queue family ownership transfer
    ScratchVector<std::pair<NewBufferAccess, uint32_t>> qfotBufferExports;
    ScratchVector<std::pair<NewImageAccess, uint32_t>> qfotImageExports;
};

// Process the accesses of a single command or export, updating barriers and access map as necessary
void processAccesses(
    uint32_t cmdIndex,
    ArrayView<const NewBufferAccess> newBufferAccesses,
    ArrayView<const NewImageAccess> newImageAccesses,
    BarrierList& barriers,
    QueueSyncState* queueSyncState) {
    // Update barriers pass
    for (const NewBufferAccess& newAccess : newBufferAccesses) {
        VkBufferHandle handle = newAccess.vkResourceHandle;
        auto mapHit = queueSyncState->bufferResourceMap.find(handle);
        if (mapHit == queueSyncState->bufferResourceMap.end())
            mapHit = queueSyncState->bufferResourceMap.emplace(handle, BufferAccessMap(handle)).first;
        mapHit->second.synchronizeNewAccess(newAccess, cmdIndex, barriers);
    }
    for (const NewImageAccess& newAccess : newImageAccesses) {
        VkImageHandle handle = newAccess.vkResourceHandle;
        auto mapHit = queueSyncState->imageResourceMap.find(handle);
        if (mapHit == queueSyncState->imageResourceMap.end())
            mapHit = queueSyncState->imageResourceMap.emplace(handle, ImageAccessMap(handle)).first;
        mapHit->second.synchronizeNewAccess(newAccess, cmdIndex, barriers);
    }

    // Update accesses pass
    for (const NewBufferAccess& newAccess : newBufferAccesses) {
        auto mapHit = queueSyncState->bufferResourceMap.find(newAccess.vkResourceHandle);
        mapHit->second.insertNewAccess(newAccess, barriers.getBarrierCount());
    }
    for (const NewImageAccess& newAccess : newImageAccesses) {
        auto mapHit = queueSyncState->imageResourceMap.find(newAccess.vkResourceHandle);
        mapHit->second.insertNewAccess(newAccess, barriers.getBarrierCount());
    }
}

void prepareBarriers(
    const JobData* job,
    QueueSyncState* queueSyncState,
    ResourceExportHandler& resourceExportHandler,
    BarrierList& barriers) {
    // Temporary storage for new accesses of each command
    ScratchVector<NewBufferAccess> newBufferAccesses;
    ScratchVector<NewImageAccess> newImageAccesses;

    // Process commands
    auto* cmd = job->record.firstCommandPtr;
    uint32_t cmdIndex = 0;
    while (cmd != nullptr) {
        // Some commands need special handling here
        switch (cmd->commandType) {
        case JobCommandTypes::ExportBuffer: {
            // Queue accesses from export buffer operation until the next compute / render pass or end of job
            // No queue family ownership transfer handling here
            auto* data = getCommandData<JobRecordStorage::ExportBufferData>(cmd);
            resourceExportHandler.addExport(*data);
            break;
        }
        case JobCommandTypes::ExportImage: {
            // Queue accesses from export image operation until the next compute / render pass or end of job
            auto* data = getCommandData<JobRecordStorage::ExportImageData>(cmd);
            resourceExportHandler.addExport(*data);
            break;
        }
        case JobCommandTypes::DiscardImageContents: {
            // Discard image subresource range - mark the range with VK_IMAGE_LAYOUT_UNDEFINED layout
            auto* data = getCommandData<JobRecordStorage::DiscardImageContentsData>(cmd);
            ImageAccessRange range = data->range;
            VkImageHandle vkImageHandle = resolveImageAccess(data->image, &range);

            auto mapHit = queueSyncState->imageResourceMap.find(vkImageHandle);
            if (mapHit != queueSyncState->imageResourceMap.end()) {
                mapHit->second.discardContents(range);
            }
            break;
        }
        case JobCommandTypes::ImportExternalBuffer: {
            // Overwrite subresource range with the given access
            auto* data = getCommandData<JobRecordStorage::ImportExternalBufferData>(cmd);
            auto [vkBufferHandle, range] = resolveBufferAccess(data->buffer);

            auto mapHit = queueSyncState->bufferResourceMap.find(vkBufferHandle);
            if (mapHit != queueSyncState->bufferResourceMap.end()) {
                mapHit->second.insertNewAccess(
                    { vkBufferHandle, range, data->access }, barriers.getBarrierCount(), true, true);
            }
            break;
        }
        case JobCommandTypes::ImportExternalImage: {
            // Overwrite subresource range with the given access
            auto* data = getCommandData<JobRecordStorage::ImportExternalImageData>(cmd);
            ImageAccessRange range = data->range;
            VkImageHandle vkImageHandle = resolveImageAccess(data->image, &range);

            auto mapHit = queueSyncState->imageResourceMap.find(vkImageHandle);
            if (mapHit != queueSyncState->imageResourceMap.end()) {
                mapHit->second.insertNewAccess(
                    { vkImageHandle, range, data->access, data->vkImageLayout }, barriers.getBarrierCount(), true, true);
            }
            break;
        }
        case JobCommandTypes::ExecuteComputePass: {
            // Flush export operations for resources that can be used in the compute pass
            resourceExportHandler.flushExports(cmdIndex, getComputePipelineStageMask());

            identifyCommandResourceAccesses(cmd, newBufferAccesses, newImageAccesses);
            processAccesses(cmdIndex, view(newBufferAccesses), view(newImageAccesses), barriers, queueSyncState);
            barriers.markExportedResourceUsage();
            break;
        }
        case JobCommandTypes::ExecuteRenderPass: {
            // Flush export operations for resources that can be used in the render pass
            resourceExportHandler.flushExports(cmdIndex, getGraphicsPipelineStageMask());

            // Process regular accesses
            identifyCommandResourceAccesses(cmd, newBufferAccesses, newImageAccesses);
            processAccesses(cmdIndex, view(newBufferAccesses), view(newImageAccesses), barriers, queueSyncState);
            barriers.markExportedResourceUsage();
            break;
        }
        case JobCommandTypes::BuildAccelerationStructures:
        case JobCommandTypes::BuildAccelerationStructuresIndirect: {
            // Flush export operations for resources that can be used for acceleration structure build
            resourceExportHandler.flushExports(cmdIndex, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

            // Process regular accesses
            identifyCommandResourceAccesses(cmd, newBufferAccesses, newImageAccesses);
            processAccesses(cmdIndex, view(newBufferAccesses), view(newImageAccesses), barriers, queueSyncState);
            break;
        }
        default: {
            // All other commands only get their usual accesses handled
            identifyCommandResourceAccesses(cmd, newBufferAccesses, newImageAccesses);
            processAccesses(cmdIndex, view(newBufferAccesses), view(newImageAccesses), barriers, queueSyncState);
        }
        }

        cmd = cmd->nextCommand;
        cmdIndex++;
    }

    resourceExportHandler.finishSubmit();
}

void recordBarrier(PrimaryBufferRecorder& recorder, const Barrier& barrier) {
    ScratchVector<VkBufferMemoryBarrier> bufferBarriers;
    bufferBarriers.reserve(barrier.bufferDependencies.size());
    ScratchVector<VkImageMemoryBarrier> imageBarriers;
    // Reserve slight excess for image barriers with disjoint mip levels
    imageBarriers.reserve(barrier.imageDependencies.size() + (barrier.imageDependencies.size() >> 2));

    // Translate resource dependencies to memory barriers
    for (const BufferDependency& dependency : barrier.bufferDependencies) {
        bufferBarriers.push_back(dependency.toMemoryBarrier());
    }

    for (const ImageDependency& dependency : barrier.imageDependencies) {
        dependency.toImageBarriers(imageBarriers);
    }

    recorder.getVkiCommands().cmdPipelineBarrier(
        recorder.requestBuffer(),
        barrier.srcStageMask,
        barrier.dstStageMask,
        0,
        0,
        nullptr,
        static_cast<uint32_t>(bufferBarriers.size()),
        bufferBarriers.data(),
        static_cast<uint32_t>(imageBarriers.size()),
        imageBarriers.data());
}

void recordCommandBuffers(
    DeviceContainer* deviceImpl,
    PrimaryBufferRecorder& recorder,
    const JobData* job,
    const BarrierList& barriers) {
    // Prepare query recording
    recorder.getQueryRecorder().setJobSemaphore(job->semaphores.jobSignal);

    auto* cmd = job->record.firstCommandPtr;
    uint32_t cmdIndex = 0;
    uint32_t barrierIndex = 0;

    while (true) {
        // Record the next barriers
        while (barrierIndex < barriers.getBarrierCount() &&
               barriers.getBarrier(barrierIndex).commandIndex <= cmdIndex) {
            recordBarrier(recorder, barriers.getBarrier(barrierIndex));
            barrierIndex++;
        }

        // Record the next command
        if (cmd != nullptr) {
            recordCommand(job, recorder, cmd);
            cmd = cmd->nextCommand;
            cmdIndex++;
        } else {
            // End of job, record remaining barriers
            for (; barrierIndex < barriers.getBarrierCount(); barrierIndex++) {
                recordBarrier(recorder, barriers.getBarrier(barrierIndex));
            }
            break;
        }
    }

    // Compile query batches from primary and secondary buffers and notify the manager about them
    ScratchVector<QueryBatch*> queryBatches;
    recorder.getQueryRecorder().retrieveBatchesAndReset(queryBatches);
    for (CommandPool* secondaryPool : job->resources.commandPools) {
        secondaryPool->getQueryRecorder().retrieveBatchesAndReset(queryBatches);
    }
    deviceImpl->getQueryManager()->registerBatches(view(queryBatches), job->semaphores.jobSignal);
}

void compileJob(
    JobCompilationContext& context,
    const Job& job,
    ArrayParameter<const CrossQueueSync::ExportEntry> incomingExports) {
    const JobData* jobData = JobResourcePoolContainer::getJobData(job);
    const char* jobName = JobResourcePoolContainer::getJobDebugTarget(job)->getObjectName();

    // Discard contents of local images
    for (const auto& localImage : jobData->resources.localImages.getImages()) {
        if (!localImage.hasUnderlyingImage())
            continue;

        const Image* underlyingImage = localImage.getUnderlyingImage();
        auto mapHit = context.queueSyncState->imageResourceMap.find(underlyingImage->vkGetImageHandle());
        if (mapHit != context.queueSyncState->imageResourceMap.end()) {
            mapHit->second.discardContents(underlyingImage->getWholeRange());
        }
    }

    TEPHRA_ASSERT(!jobData->semaphores.jobSignal.isNull());
    BarrierList barriers(jobData->semaphores.jobSignal.timestamp);
    {
        // Setup barriers and handle incoming exports
        auto queueInfos = jobData->resourcePoolImpl->getParentDeviceImpl()->getQueueMap()->getQueueInfos();
        uint32_t currentQueueFamilyIndex = queueInfos[jobData->resourcePoolImpl->getBaseQueueIndex()].queueFamilyIndex;

        ResourceExportHandler resourceExportHandler(&barriers, context.queueSyncState, currentQueueFamilyIndex);
        resourceExportHandler.processIncomingExports(incomingExports);

        // Insert barriers based on previous accesses and local accesses from commands within the job
        prepareBarriers(jobData, context.queueSyncState, resourceExportHandler, barriers);
    }

    // Record the vulkan command buffers, inserting the prepared barriers
    std::size_t commandBuffersBefore = context.recorder->getCommandBufferCount();
    recordCommandBuffers(context.deviceImpl, *context.recorder, jobData, barriers);

    if constexpr (StatisticEventsEnabled) {
        // Report statistics
        reportStatisticEvent(
            StatisticEventType::JobPrimaryCommandBuffersUsed,
            context.recorder->getCommandBufferCount() - commandBuffersBefore,
            jobName);
        reportStatisticEvent(StatisticEventType::JobPipelineBarriersInserted, barriers.getBarrierCount(), jobName);

        uint64_t bufferBarriers = 0;
        uint64_t imageBarriers = 0;
        for (uint32_t i = 0; i < barriers.getBarrierCount(); i++) {
            bufferBarriers += barriers.getBarrier(i).bufferDependencies.size();
            // In general, a single image dependency can result in multiple memory barriers, but let's simplify
            imageBarriers += barriers.getBarrier(i).imageDependencies.size();
        }
        reportStatisticEvent(StatisticEventType::JobBufferMemoryBarriersInserted, bufferBarriers, jobName);
        reportStatisticEvent(StatisticEventType::JobImageMemoryBarriersInserted, imageBarriers, jobName);
    }
}

}
