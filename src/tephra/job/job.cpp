#include "job_data.hpp"
#include "resource_pool_container.hpp"
#include "compute_pass.hpp"
#include "render_pass.hpp"
#include "accesses.hpp"
#include "../device/device_container.hpp"
#include "../swapchain_impl.hpp"
#include "../acceleration_structure_impl.hpp"
#include <tephra/job.hpp>

namespace tp {

constexpr const char* ComputeListTypeName = "ComputeList";
constexpr const char* RenderListTypeName = "RenderList";
constexpr const char* JobLocalBufferTypeName = "JobLocalBuffer";
constexpr const char* JobLocalImageTypeName = "JobLocalImage";
constexpr const char* JobLocalAccelerationStructureTypeName = "JobLocalAccelerationStructure";

template <typename T, typename... TArgs>
std::pair<JobRecordStorage::CommandMetadata*, T*> allocateCommand(
    JobRecordStorage& storage,
    JobCommandTypes type,
    TArgs&&... args) {
    std::size_t allocSize = sizeof(JobRecordStorage::CommandMetadata) + sizeof(T);
    ArrayView<std::byte> bytes = storage.cmdBuffer.allocate(allocSize);

    auto metadataPtr = new (bytes.data()) JobRecordStorage::CommandMetadata;
    metadataPtr->commandType = type;
    metadataPtr->nextCommand = nullptr;

    auto cmdDataPtr = bytes.data() + sizeof(JobRecordStorage::CommandMetadata);
    return { metadataPtr, new (cmdDataPtr) T(std::forward<TArgs>(args)...) };
}

template <typename T, typename... TArgs>
T* recordCommand(JobRecordStorage& storage, JobCommandTypes type, TArgs&&... args) {
    auto [metadataPtr, cmdDataPtr] = allocateCommand<T>(storage, type, std::forward<TArgs>(args)...);
    storage.addCommand(metadataPtr);
    return cmdDataPtr;
}

template <typename T, typename... TArgs>
T* recordDelayedCommand(JobRecordStorage& storage, JobCommandTypes type, TArgs&&... args) {
    auto [metadataPtr, cmdDataPtr] = allocateCommand<T>(storage, type, std::forward<TArgs>(args)...);
    storage.addDelayedCommand(metadataPtr);
    return cmdDataPtr;
}

inline void markResourceUsage(JobData* jobData, const BufferView& buffer, bool isExport = false) {
    TEPHRA_ASSERT(!buffer.isNull());
    if (buffer.viewsJobLocalBuffer()) {
        jobData->resources.localBuffers.markBufferUsage(buffer, jobData->record.nextCommandIndex);
        if (isExport) {
            jobData->resources.localBuffers.markBufferUsage(buffer, ~0);
        }
    }
}

inline void markResourceUsage(JobData* jobData, const ImageView& image, bool isExport = false) {
    TEPHRA_ASSERT(!image.isNull());
    if (image.viewsJobLocalImage()) {
        jobData->resources.localImages.markImageUsage(image, jobData->record.nextCommandIndex);
        if (isExport) {
            jobData->resources.localImages.markImageUsage(image, ~0);
        }
    }
}

inline void markResourceUsage(JobData* jobData, const StoredImageView& image, bool isExport = false) {
    TEPHRA_ASSERT(!image.isNull());
    if (image.getJobLocalView() != nullptr) {
        markResourceUsage(jobData, *image.getJobLocalView());
    }
}

Job::Job(JobData* jobData, DebugTarget debugTarget) : debugTarget(std::move(debugTarget)), jobData(jobData) {
    TEPHRA_ASSERT(jobData != nullptr);
    TEPHRA_ASSERT(jobData->resourcePoolImpl != nullptr);
    if (this->debugTarget->getObjectName() != nullptr)
        cmdBeginDebugLabel(this->debugTarget->getObjectName());
}

void Job::finalize() {
    // Patch delayed commands to the end of the command list
    if (jobData->record.firstDelayedCommandPtr != nullptr) {
        TEPHRA_ASSERT(jobData->record.lastCommandPtr != nullptr);
        jobData->record.lastCommandPtr->nextCommand = jobData->record.firstDelayedCommandPtr;
        jobData->record.lastCommandPtr = jobData->record.lastDelayedCommandPtr;
    }

    if (debugTarget->getObjectName() != nullptr)
        cmdEndDebugLabel();
}

BufferView Job::allocateLocalBuffer(const BufferSetup& setup, const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "allocateLocalBuffer", debugName);

    DebugTarget debugTarget = DebugTarget(
        jobData->resourcePoolImpl->getDebugTarget(), JobLocalBufferTypeName, debugName);
    return jobData->resources.localBuffers.acquireNewBuffer(setup, std::move(debugTarget));
}

ImageView Job::allocateLocalImage(const ImageSetup& setup, const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "allocateLocalImage", debugName);

    DebugTarget debugTarget = DebugTarget(
        jobData->resourcePoolImpl->getDebugTarget(), JobLocalImageTypeName, debugName);
    return jobData->resources.localImages.acquireNewImage(setup, std::move(debugTarget));
}

BufferView Job::allocatePreinitializedBuffer(
    const BufferSetup& setup,
    const MemoryPreference& memoryPreference,
    const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "allocatePreinitializedBuffer", debugName);

    return jobData->resourcePoolImpl->getPreinitializedBufferPool()->allocateJobBuffer(
        jobData->jobIdInPool, setup, memoryPreference, debugName);
}

DescriptorSetView Job::allocateLocalDescriptorSet(
    const DescriptorSetLayout* descriptorSetLayout,
    ArrayParameter<const FutureDescriptor> descriptors,
    const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "allocateLocalDescriptorSet", debugName);
    return jobData->resources.localDescriptorSets.prepareNewDescriptorSet(descriptorSetLayout, descriptors, debugName);
}

AccelerationStructureView Job::allocateLocalAccelerationStructureKHR(
    const AccelerationStructureSetup& setup,
    const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "allocateLocalAccelerationStructureKHR", debugName);

    DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
    AccelerationStructureBuilder* asBuilder = jobData->resourcePoolImpl->getAccelerationStructurePool()->acquireBuilder(
        setup, jobData->jobIdInPool);

    // Create a local backing buffer to hold the AS
    auto backingBufferSetup = BufferSetup(
        asBuilder->getStorageSize(),
        BufferUsageMask::None(),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        256);
    auto backingBuffer = jobData->resources.localBuffers.acquireNewBuffer(
        backingBufferSetup, DebugTarget::makeSilent());

    DebugTarget debugTarget = DebugTarget(
        jobData->resourcePoolImpl->getDebugTarget(), JobLocalAccelerationStructureTypeName, debugName);
    return jobData->resources.localAccelerationStructures.acquireNew(asBuilder, backingBuffer, std::move(debugTarget));
}

CommandPool* Job::createCommandPool(const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "createCommandPool", debugName);

    DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();

    uint32_t baseQueueIndex = jobData->resourcePoolImpl->getBaseQueueIndex();
    QueueType baseQueueType = deviceImpl->getQueueMap()->getQueueInfos()[baseQueueIndex].identifier.type;

    CommandPool* commandPool = deviceImpl->getCommandPoolPool()->acquirePool(baseQueueType, debugName);
    jobData->resources.commandPools.push_back(commandPool);

    return commandPool;
}

void Job::cmdExportResource(const BufferView& buffer, ReadAccessMask readAccessMask, QueueType targetQueueType) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdExportResource", nullptr);

    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    if (targetQueueType != QueueType::Undefined) {
        const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
        queueFamilyIndex = deviceImpl->getPhysicalDevice()->getQueueTypeInfo(targetQueueType).queueFamilyIndex;
    }

    markResourceUsage(jobData, buffer, true);

    ResourceAccess access = convertReadAccessToVkAccess(readAccessMask);
    recordCommand<JobRecordStorage::ExportBufferData>(
        jobData->record, JobCommandTypes::ExportBuffer, buffer, access, queueFamilyIndex);
}

void Job::cmdExportResource(const ImageView& image, ReadAccessMask readAccessMask, QueueType targetQueueType) {
    cmdExportResource(image, image.getWholeRange(), readAccessMask, targetQueueType);
}

void Job::cmdExportResource(
    const ImageView& image,
    const ImageSubresourceRange& range,
    ReadAccessMask readAccessMask,
    QueueType targetQueueType) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdExportResource", nullptr);

    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    if (targetQueueType != QueueType::Undefined) {
        const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
        queueFamilyIndex = deviceImpl->getPhysicalDevice()->getQueueTypeInfo(targetQueueType).queueFamilyIndex;
    }

    markResourceUsage(jobData, image, true);

    ResourceAccess access = convertReadAccessToVkAccess(readAccessMask);
    VkImageLayout vkImageLayout = vkGetImageLayoutFromReadAccess(readAccessMask);
    recordCommand<JobRecordStorage::ExportImageData>(
        jobData->record, JobCommandTypes::ExportImage, image, range, access, vkImageLayout, queueFamilyIndex);
}

void Job::cmdExportResource(
    const AccelerationStructureView& accelerationStructure,
    ReadAccessMask readAccessMask,
    QueueType targetQueueType) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdExportResource", nullptr);

    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    if (targetQueueType != QueueType::Undefined) {
        const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
        queueFamilyIndex = deviceImpl->getPhysicalDevice()->getQueueTypeInfo(targetQueueType).queueFamilyIndex;
    }

    markResourceUsage(jobData, accelerationStructure.getBackingBufferView(), true);

    ResourceAccess access = convertReadAccessToVkAccess(readAccessMask);
    // To avoid adding extra read access flags for each stage, we treat acceleration structure accesses like uniform
    // accesses and translate them here
    if (containsAllBits(access.accessMask, VK_ACCESS_UNIFORM_READ_BIT)) {
        access.accessMask &= ~VK_ACCESS_UNIFORM_READ_BIT;
        access.accessMask |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    }

    // There is no need for a separate "export acceleration structure" command, we can just export its backing buffer
    recordCommand<JobRecordStorage::ExportBufferData>(
        jobData->record,
        JobCommandTypes::ExportBuffer,
        accelerationStructure.getBackingBufferView(),
        access,
        queueFamilyIndex);
}

void Job::cmdDiscardContents(const ImageView& image) {
    cmdDiscardContents(image, image.getWholeRange());
}

void Job::cmdDiscardContents(const ImageView& image, ImageSubresourceRange range) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDiscardContents", nullptr);

    recordCommand<JobRecordStorage::DiscardImageContentsData>(
        jobData->record, JobCommandTypes::DiscardImageContents, image, range);
}

void Job::cmdFillBuffer(const BufferView& dstBuffer, uint32_t value) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdFillBuffer", nullptr);

    markResourceUsage(jobData, dstBuffer);

    recordCommand<JobRecordStorage::FillBufferData>(jobData->record, JobCommandTypes::FillBuffer, dstBuffer, value);
}

void Job::cmdUpdateBuffer(const BufferView& dstBuffer, ArrayParameter<const std::byte> data) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdUpdateBuffer", nullptr);

    markResourceUsage(jobData, dstBuffer);

    auto cmdBufData = jobData->record.cmdBuffer.allocate(data.size());
    memcpy(cmdBufData.data(), data.data(), data.size());

    recordCommand<JobRecordStorage::UpdateBufferData>(
        jobData->record, JobCommandTypes::UpdateBuffer, dstBuffer, cmdBufData);
}

void Job::cmdCopyBuffer(
    const BufferView& srcBuffer,
    const BufferView& dstBuffer,
    ArrayParameter<const BufferCopyRegion> copyRegions) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdCopyBuffer", nullptr);

    markResourceUsage(jobData, srcBuffer);
    markResourceUsage(jobData, dstBuffer);

    auto copyRegionsData = jobData->record.cmdBuffer.allocate<BufferCopyRegion>(copyRegions);
    recordCommand<JobRecordStorage::CopyBufferData>(
        jobData->record, JobCommandTypes::CopyBuffer, srcBuffer, dstBuffer, copyRegionsData);
}

void Job::cmdCopyImage(
    const ImageView& srcImage,
    const ImageView& dstImage,
    ArrayParameter<const ImageCopyRegion> copyRegions) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdCopyImage", nullptr);

    markResourceUsage(jobData, srcImage);
    markResourceUsage(jobData, dstImage);

    auto copyRegionsData = jobData->record.cmdBuffer.allocate<ImageCopyRegion>(copyRegions);
    recordCommand<JobRecordStorage::CopyImageData>(
        jobData->record, JobCommandTypes::CopyImage, srcImage, dstImage, copyRegionsData);
}

void Job::cmdCopyBufferToImage(
    const BufferView& srcBuffer,
    const ImageView& dstImage,
    ArrayParameter<const BufferImageCopyRegion> copyRegions) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdCopyBufferToImage", nullptr);

    if constexpr (TephraValidationEnabled) {
        // Check buffer usage for ImageTransfer
        const tp::BufferSetup* bufSetup;
        if (srcBuffer.viewsJobLocalBuffer()) {
            bufSetup = &JobLocalBufferImpl::getBufferImpl(srcBuffer).getBufferSetup();
        } else {
            bufSetup = &BufferImpl::getBufferImpl(srcBuffer).getBufferSetup();
        }
        if (!bufSetup->usage.contains(BufferUsage::ImageTransfer)) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The source buffer was not created with the BufferUsage::ImageTransfer usage.");
        }
    }

    markResourceUsage(jobData, srcBuffer);
    markResourceUsage(jobData, dstImage);

    auto copyRegionsData = jobData->record.cmdBuffer.allocate<BufferImageCopyRegion>(copyRegions);
    recordCommand<JobRecordStorage::CopyBufferImageData>(
        jobData->record, JobCommandTypes::CopyBufferToImage, srcBuffer, dstImage, copyRegionsData);
}

void Job::cmdCopyImageToBuffer(
    const ImageView& srcImage,
    const BufferView& dstBuffer,
    ArrayParameter<const BufferImageCopyRegion> copyRegions) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdCopyImageToBuffer", nullptr);

    if constexpr (TephraValidationEnabled) {
        // Check buffer usage for ImageTransfer
        const tp::BufferSetup* bufSetup;
        if (dstBuffer.viewsJobLocalBuffer()) {
            bufSetup = &JobLocalBufferImpl::getBufferImpl(dstBuffer).getBufferSetup();
        } else {
            bufSetup = &BufferImpl::getBufferImpl(dstBuffer).getBufferSetup();
        }
        if (!bufSetup->usage.contains(BufferUsage::ImageTransfer)) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The destination buffer was not created with the BufferUsage::ImageTransfer usage.");
        }
    }

    markResourceUsage(jobData, srcImage);
    markResourceUsage(jobData, dstBuffer);

    auto copyRegionsData = jobData->record.cmdBuffer.allocate<BufferImageCopyRegion>(copyRegions);
    recordCommand<JobRecordStorage::CopyBufferImageData>(
        jobData->record, JobCommandTypes::CopyImageToBuffer, dstBuffer, srcImage, copyRegionsData);
}

void Job::cmdBlitImage(
    const ImageView& srcImage,
    const ImageView& dstImage,
    ArrayParameter<const ImageBlitRegion> blitRegions,
    Filter filter) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBlitImage", nullptr);

    markResourceUsage(jobData, srcImage);
    markResourceUsage(jobData, dstImage);

    auto blitRegionsData = jobData->record.cmdBuffer.allocate<ImageBlitRegion>(blitRegions);
    recordCommand<JobRecordStorage::BlitImageData>(
        jobData->record, JobCommandTypes::BlitImage, srcImage, dstImage, blitRegionsData, filter);
}

void Job::cmdClearImage(const ImageView& dstImage, ClearValue value) {
    cmdClearImage(dstImage, value, { dstImage.getWholeRange() });
}

void Job::cmdClearImage(const ImageView& dstImage, ClearValue value, ArrayParameter<const ImageSubresourceRange> ranges) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdClearImage", nullptr);

    markResourceUsage(jobData, dstImage);

    auto rangesData = jobData->record.cmdBuffer.allocate<ImageSubresourceRange>(ranges);
    recordCommand<JobRecordStorage::ClearImageData>(
        jobData->record, JobCommandTypes::ClearImage, dstImage, value, rangesData);
}

void Job::cmdResolveImage(
    const ImageView& srcImage,
    const ImageView& dstImage,
    ArrayParameter<const ImageCopyRegion> resolveRegions) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdResolveImage", nullptr);

    markResourceUsage(jobData, srcImage);
    markResourceUsage(jobData, dstImage);

    // Reuse copy image data since it's identical for resolve
    auto resolveRegionsData = jobData->record.cmdBuffer.allocate<ImageCopyRegion>(resolveRegions);

    recordCommand<JobRecordStorage::CopyImageData>(
        jobData->record, JobCommandTypes::ResolveImage, srcImage, dstImage, resolveRegionsData);
}

void Job::cmdExecuteComputePass(
    const ComputePassSetup& setup,
    std::variant<ArrayView<ComputeList>, ComputeInlineCallback> commandRecording,
    const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdExecuteComputePass", debugName);
    DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();

    // Acquire a free ComputePass
    if (jobData->record.computePassCount == jobData->record.computePassStorage.size()) {
        jobData->record.computePassStorage.emplace_back(jobData->resourcePoolImpl->getParentDeviceImpl());
    }
    ComputePass& computePass = jobData->record.computePassStorage[jobData->record.computePassCount];
    jobData->record.computePassCount++;

    DebugTarget listDebugTarget = DebugTarget(debugTarget.get(), ComputeListTypeName, debugName);
    if (std::holds_alternative<ComputeInlineCallback>(commandRecording)) {
        computePass.assignInline(
            setup, std::move(std::get<ComputeInlineCallback>(commandRecording)), std::move(listDebugTarget));
    } else {
        ArrayView<ComputeList>& computeListsToAssign = std::get<ArrayView<ComputeList>>(commandRecording);
        computePass.assignDeferred(setup, listDebugTarget, computeListsToAssign);
    }

    for (const BufferComputeAccess& entry : setup.bufferAccesses) {
        markResourceUsage(jobData, entry.buffer);
    }
    for (const ImageComputeAccess& entry : setup.imageAccesses) {
        markResourceUsage(jobData, entry.image);
    }

    recordCommand<JobRecordStorage::ExecuteComputePassData>(
        jobData->record, JobCommandTypes::ExecuteComputePass, &computePass);
}

void Job::cmdExecuteRenderPass(
    const RenderPassSetup& setup,
    std::variant<ArrayView<RenderList>, RenderInlineCallback> commandRecording,
    const char* debugName) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdExecuteRenderPass", debugName);
    const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();

    // Acquire a free RenderPass
    if (jobData->record.renderPassCount == jobData->record.renderPassStorage.size()) {
        jobData->record.renderPassStorage.emplace_back(jobData->resourcePoolImpl->getParentDeviceImpl());
    }
    RenderPass& renderPass = jobData->record.renderPassStorage[jobData->record.renderPassCount];
    jobData->record.renderPassCount++;

    DebugTarget listDebugTarget = DebugTarget(debugTarget.get(), RenderListTypeName, debugName);
    if (std::holds_alternative<RenderInlineCallback>(commandRecording)) {
        renderPass.assignInline(
            setup, std::move(std::get<RenderInlineCallback>(commandRecording)), std::move(listDebugTarget));
    } else {
        ArrayView<RenderList>& renderListsToAssign = std::get<ArrayView<RenderList>>(commandRecording);
        renderPass.assignDeferred(setup, listDebugTarget, renderListsToAssign);
    }

    for (const BufferRenderAccess& entry : setup.bufferAccesses) {
        markResourceUsage(jobData, entry.buffer);
    }
    for (const ImageRenderAccess& entry : setup.imageAccesses) {
        markResourceUsage(jobData, entry.image);
    }
    for (const AttachmentAccess& entry : renderPass.getAttachmentAccesses()) {
        if (!entry.imageView.isNull())
            markResourceUsage(jobData, entry.imageView);
    }

    recordCommand<JobRecordStorage::ExecuteRenderPassData>(
        jobData->record, JobCommandTypes::ExecuteRenderPass, &renderPass);
}

void Job::cmdBeginDebugLabel(const char* name, ArrayParameter<const float> color) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBeginDebugLabel", name);
    const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(Functionality::DebugUtilsEXT)) {
        recordCommand<JobRecordStorage::DebugLabelData>(jobData->record, JobCommandTypes::BeginDebugLabel, name, color);
    }
}

void Job::cmdInsertDebugLabel(const char* name, ArrayParameter<const float> color) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdInsertDebugLabel", name);
    const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(Functionality::DebugUtilsEXT))
        recordCommand<JobRecordStorage::DebugLabelData>(
            jobData->record, JobCommandTypes::InsertDebugLabel, name, color);
}

void Job::cmdEndDebugLabel() {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdEndDebugLabel", nullptr);
    const DeviceContainer* deviceImpl = jobData->resourcePoolImpl->getParentDeviceImpl();
    if (deviceImpl->getLogicalDevice()->isFunctionalityAvailable(Functionality::DebugUtilsEXT))
        recordCommand<JobRecordStorage::DebugLabelData>(jobData->record, JobCommandTypes::EndDebugLabel, nullptr);
}

void Job::cmdWriteTimestamp(const TimestampQuery& query, PipelineStage stage) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdWriteTimestamp", nullptr);
    recordCommand<JobRecordStorage::WriteTimestampData>(
        jobData->record, JobCommandTypes::WriteTimestamp, QueryRecorder::getQueryHandle(query), stage);
}

void Job::vkCmdImportExternalResource(
    const BufferView& buffer,
    VkPipelineStageFlags vkStageMask,
    VkAccessFlags vkAccessMask) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "vkCmdImportExternalResource", nullptr);

    markResourceUsage(jobData, buffer);

    recordCommand<JobRecordStorage::ImportExternalBufferData>(
        jobData->record, JobCommandTypes::ImportExternalBuffer, buffer, ResourceAccess(vkStageMask, vkAccessMask));
}

void Job::vkCmdImportExternalResource(
    const ImageView& image,
    VkImageLayout vkImageLayout,
    VkPipelineStageFlags vkStageMask,
    VkAccessFlags vkAccessMask) {
    vkCmdImportExternalResource(image, image.getWholeRange(), vkImageLayout, vkStageMask, vkAccessMask);
}

void Job::vkCmdImportExternalResource(
    const ImageView& image,
    const ImageSubresourceRange& range,
    VkImageLayout vkImageLayout,
    VkPipelineStageFlags vkStageMask,
    VkAccessFlags vkAccessMask) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "vkCmdImportExternalResource", nullptr);

    markResourceUsage(jobData, image);

    recordCommand<JobRecordStorage::ImportExternalImageData>(
        jobData->record,
        JobCommandTypes::ImportExternalImage,
        image,
        range,
        ResourceAccess(vkStageMask, vkAccessMask),
        vkImageLayout);
}

// Command only used internally after AS build
inline void cmdWriteAccelerationStructureSize(tp::JobData* jobData, const AccelerationStructureView& view) {
    const AccelerationStructureQueryKHR& query = AccelerationStructureImpl::getAccelerationStructureImpl(view)
                                                     .getCompactedSizeQuery();

    // Record it as a delayed command
    markResourceUsage(jobData, view.getBackingBufferView(), true);
    recordDelayedCommand<JobRecordStorage::WriteAccelerationStructureSizeData>(
        jobData->record, JobCommandTypes::WriteAccelerationStructureSize, QueryRecorder::getQueryHandle(query), view);
};

using AccelerationStructureBuildData = JobRecordStorage::BuildAccelerationStructuresData::SingleBuild;
inline AccelerationStructureBuildData prepareASBuild(
    tp::JobData* jobData,
    const AccelerationStructureBuildInfo& buildInfo,
    const AccelerationStructureBuildIndirectInfo& indirectInfo) {
    // Mark input buffers as used
    markResourceUsage(jobData, buildInfo.dstView.getBackingBufferView());

    if (!buildInfo.srcView.isNull())
        markResourceUsage(jobData, buildInfo.srcView.getBackingBufferView());

    if (!buildInfo.instanceGeometry.instanceBuffer.isNull())
        markResourceUsage(jobData, buildInfo.instanceGeometry.instanceBuffer);

    for (const AccelerationStructureView& accessedView : buildInfo.instanceGeometry.accessedViews) {
        markResourceUsage(jobData, accessedView.getBackingBufferView());
    }

    for (const TriangleGeometryBuildInfo& triangles : buildInfo.triangleGeometries) {
        markResourceUsage(jobData, triangles.vertexBuffer);
        if (!triangles.indexBuffer.isNull())
            markResourceUsage(jobData, triangles.indexBuffer);
        if (!triangles.transformBuffer.isNull())
            markResourceUsage(jobData, triangles.transformBuffer);
    }

    for (const AABBGeometryBuildInfo& aabbs : buildInfo.aabbGeometries) {
        markResourceUsage(jobData, aabbs.aabbBuffer);
    }

    if (!indirectInfo.buildRangeBuffer.isNull())
        markResourceUsage(jobData, indirectInfo.buildRangeBuffer);

    // Get the dedicated builder for this AS
    auto& builder = AccelerationStructureBuilder::getBuilderFromView(buildInfo.dstView);
    if (!buildInfo.dstView.viewsJobLocalAccelerationStructure()) {
        // Borrow ownership of the builder of the used persistent AS into a separate storage
        auto& asImpl = AccelerationStructureImpl::getAccelerationStructureImpl(buildInfo.dstView);
        jobData->resources.usedASBuilders.push_back(asImpl.getBuilder());
    }

    // Allocate scratch buffer for the build
    uint64_t scratchBufferSize = builder.getScratchBufferSize(buildInfo.mode);

    auto scratchBufferSetup = BufferSetup(
        scratchBufferSize, BufferUsage::StorageBuffer | BufferUsage::DeviceAddress, 0, 256);
    BufferView scratchBuffer = jobData->resources.localBuffers.acquireNewBuffer(
        scratchBufferSetup, DebugTarget::makeSilent());

    // Immediately mark the scratch buffer as used
    markResourceUsage(jobData, scratchBuffer);

    // Copy the data as stored resources
    auto accessedViewsData = jobData->record.cmdBuffer.allocate<StoredAccelerationStructureView>(
        buildInfo.instanceGeometry.accessedViews);
    auto triangleGeometriesData = jobData->record.cmdBuffer.allocate<StoredTriangleGeometryBuildInfo>(
        buildInfo.triangleGeometries);
    auto aabbGeometriesData = jobData->record.cmdBuffer.allocate<StoredAABBGeometryBuildInfo>(buildInfo.aabbGeometries);
    auto maxPrimitiveCountsData = jobData->record.cmdBuffer.allocate<uint32_t>(indirectInfo.maxPrimitiveCounts);

    return AccelerationStructureBuildData(
        &builder,
        StoredAccelerationStructureBuildInfo(buildInfo, accessedViewsData, triangleGeometriesData, aabbGeometriesData),
        StoredAccelerationStructureBuildIndirectInfo(indirectInfo, maxPrimitiveCountsData),
        scratchBuffer);
}

void Job::cmdBuildAccelerationStructuresKHR(ArrayParameter<const AccelerationStructureBuildInfo> buildInfos) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBuildAccelerationStructuresKHR", nullptr);

    if constexpr (TephraValidationEnabled) {
        for (int i = 0; i < buildInfos.size(); i++) {
            auto& builder = AccelerationStructureBuilder::getBuilderFromView(buildInfos[i].dstView);
            builder.validateBuildInfo(buildInfos[i], i);
        }
    }

    auto buildsData = jobData->record.cmdBuffer.allocate<AccelerationStructureBuildData>(buildInfos.size());
    auto unusedIndirectInfo = AccelerationStructureBuildIndirectInfo({}, {});
    for (int i = 0; i < buildInfos.size(); i++) {
        buildsData[i] = prepareASBuild(jobData, buildInfos[i], unusedIndirectInfo);
    }

    recordCommand<JobRecordStorage::BuildAccelerationStructuresData>(
        jobData->record, JobCommandTypes::BuildAccelerationStructures, buildsData);

    // We also want to query the compacted size for acceleration structures that support it after building
    // Leaving it for the end of the job will help avoid needless barriers
    for (int i = 0; i < buildInfos.size(); i++) {
        if (buildInfos[i].mode == tp::AccelerationStructureBuildMode::Build &&
            buildsData[i].builder->getFlags().contains(tp::AccelerationStructureFlag::AllowCompaction)) {
            cmdWriteAccelerationStructureSize(jobData, buildInfos[i].dstView);
        }
    }
}

void Job::cmdBuildAccelerationStructuresIndirectKHR(
    ArrayParameter<const AccelerationStructureBuildInfo> buildInfos,
    ArrayParameter<const AccelerationStructureBuildIndirectInfo> indirectInfos) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBuildAccelerationStructuresIndirectKHR", nullptr);

    if constexpr (TephraValidationEnabled) {
        if (buildInfos.size() != indirectInfos.size()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The sizes of the 'buildInfos' (",
                buildInfos.size(),
                ") and 'indirectInfos' (",
                indirectInfos.size(),
                ") arrays do not match.");
        }

        for (int i = 0; i < buildInfos.size(); i++) {
            auto& builder = AccelerationStructureBuilder::getBuilderFromView(buildInfos[i].dstView);
            builder.validateBuildIndirectInfo(buildInfos[i], indirectInfos[i], i);
        }
    }

    auto buildsData = jobData->record.cmdBuffer.allocate<AccelerationStructureBuildData>(buildInfos.size());
    for (int i = 0; i < buildInfos.size(); i++) {
        buildsData[i] = prepareASBuild(jobData, buildInfos[i], indirectInfos[i]);
    }

    recordCommand<JobRecordStorage::BuildAccelerationStructuresData>(
        jobData->record, JobCommandTypes::BuildAccelerationStructuresIndirect, buildsData);

    // We also want to query the compacted size for acceleration structures that support it after building
    // Leaving it for the end of the job will help avoid needless barriers
    for (int i = 0; i < buildInfos.size(); i++) {
        if (buildInfos[i].mode == tp::AccelerationStructureBuildMode::Build &&
            buildsData[i].builder->getFlags().contains(tp::AccelerationStructureFlag::AllowCompaction)) {
            cmdWriteAccelerationStructureSize(jobData, buildInfos[i].dstView);
        }
    }
}

void Job::cmdCopyAccelerationStructureKHR(
    const AccelerationStructureView& srcView,
    const AccelerationStructureView& dstView) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdCopyAccelerationStructureKHR", nullptr);

    markResourceUsage(jobData, srcView.getBackingBufferView());
    markResourceUsage(jobData, dstView.getBackingBufferView());

    recordCommand<JobRecordStorage::CopyAccelerationStructureData>(
        jobData->record, JobCommandTypes::CopyAccelerationStructure, srcView, dstView);
}

Job::Job(Job&& other) noexcept : debugTarget(std::move(other.debugTarget)), jobData(other.jobData) {
    other.jobData = nullptr;
};

Job& Job::operator=(Job&& other) noexcept {
    debugTarget = std::move(other.debugTarget);
    jobData = other.jobData;
    other.jobData = nullptr;
    return *this;
}

Job::~Job() {
    if (jobData != nullptr) {
        TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(debugTarget.get());
        JobResourcePoolContainer::queueReleaseJob(jobData);
    }
}

}
