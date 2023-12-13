#include "command_recording.hpp"
#include "compute_pass.hpp"
#include "render_pass.hpp"
#include "../acceleration_structure_impl.hpp"
#include "../device/command_pool.hpp"

namespace tp {

PrimaryBufferRecorder::PrimaryBufferRecorder(
    CommandPool* commandPool,
    const VulkanCommandInterface* vkiCommands,
    const char* debugName,
    ScratchVector<VkCommandBufferHandle>* vkCommandBuffers)
    : commandPool(commandPool),
      vkiCommands(vkiCommands),
      debugName(debugName),
      vkCommandBuffers(vkCommandBuffers),
      vkCurrentBuffer() {}

VkCommandBufferHandle PrimaryBufferRecorder::requestBuffer() {
    if (vkCurrentBuffer.isNull()) {
        // Setup of a primary one time use command buffer
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        vkCurrentBuffer = commandPool->acquirePrimaryCommandBuffer(debugName);
        vkCommandBuffers->push_back(vkCurrentBuffer);
        throwRetcodeErrors(vkiCommands->beginCommandBuffer(vkCurrentBuffer, &beginInfo));
    }

    return vkCurrentBuffer;
}

VkDebugUtilsLabelEXT makeDebugLabel(const JobRecordStorage::DebugLabelData& data) {
    VkDebugUtilsLabelEXT label;
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pNext = nullptr;
    label.pLabelName = data.name.c_str();
    memcpy(label.color, data.color, sizeof(float) * 4);
    return label;
}

void PrimaryBufferRecorder::appendBuffer(VkCommandBufferHandle vkNewBuffer) {
    // End recording of the current command buffer. We can't use it anymore if we want to respect the order
    // of recorded commands.
    endRecording();
    vkCommandBuffers->push_back(vkNewBuffer);
}

void PrimaryBufferRecorder::endRecording() {
    if (!vkCurrentBuffer.isNull()) {
        throwRetcodeErrors(vkiCommands->endCommandBuffer(vkCurrentBuffer));
        vkCurrentBuffer = {};
    }
}

void addBufferAccess(
    ScratchVector<NewBufferAccess>& bufferAccesses,
    const BufferView& bufferView,
    ResourceAccess access) {
    auto [vkBufferHandle, range] = resolveBufferAccess(bufferView);
    bufferAccesses.emplace_back(vkBufferHandle, std::move(range), std::move(access));
}

void addBufferAccess(
    ScratchVector<NewBufferAccess>& bufferAccesses,
    const BufferView& bufferView,
    BufferAccessRange range,
    ResourceAccess access) {
    VkBufferHandle vkBufferHandle = resolveBufferAccess(bufferView, &range);
    bufferAccesses.emplace_back(vkBufferHandle, std::move(range), std::move(access));
}

void addImageAccess(
    ScratchVector<NewImageAccess>& imageAccesses,
    const ImageView& imageView,
    ImageAccessRange range,
    ResourceAccess access,
    VkImageLayout layout) {
    VkImageHandle vkImageHandle = resolveImageAccess(imageView, &range);
    imageAccesses.emplace_back(vkImageHandle, std::move(range), std::move(access), layout);
}

uint64_t getImageCopySizeBytes(const BufferImageCopyRegion& copyInfo, const FormatClassProperties& formatProperties) {
    uint32_t rowLength = copyInfo.bufferRowLength;
    if (rowLength == 0)
        rowLength = copyInfo.imageExtent.width;
    rowLength = roundUpToMultiple(rowLength, formatProperties.texelBlockWidth) / formatProperties.texelBlockWidth;

    uint32_t imageHeight = copyInfo.bufferImageHeight;
    if (imageHeight == 0)
        imageHeight = copyInfo.imageExtent.height;
    imageHeight = roundUpToMultiple(imageHeight, formatProperties.texelBlockHeight) / formatProperties.texelBlockHeight;

    // Can be either 3D image or 2D array, handle both
    uint32_t slices = tp::max(copyInfo.imageExtent.depth, copyInfo.imageSubresource.arrayLayerCount);

    uint64_t rowSize = static_cast<uint64_t>(rowLength) * formatProperties.texelBlockBytes;
    uint64_t imageSize = rowSize * imageHeight;

    return imageSize * slices;
}

void identifyCommandResourceAccesses(
    const JobRecordStorage::CommandMetadata* command,
    ScratchVector<NewBufferAccess>& bufferAccesses,
    ScratchVector<NewImageAccess>& imageAccesses) {
    bufferAccesses.clear();
    imageAccesses.clear();

    switch (command->commandType) {
    case JobCommandTypes::FillBuffer: {
        auto* data = getCommandData<JobRecordStorage::FillBufferData>(command);
        addBufferAccess(
            bufferAccesses, data->dstBuffer, { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT });
        break;
    }
    case JobCommandTypes::UpdateBuffer: {
        auto* data = getCommandData<JobRecordStorage::UpdateBufferData>(command);
        addBufferAccess(
            bufferAccesses, data->dstBuffer, { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT });
        break;
    }
    case JobCommandTypes::CopyBuffer: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferData>(command);
        for (const auto& copyRegion : data->copyRegions) {
            addBufferAccess(
                bufferAccesses,
                data->srcBuffer,
                { copyRegion.srcOffset, copyRegion.size },
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT });
            addBufferAccess(
                bufferAccesses,
                data->dstBuffer,
                { copyRegion.dstOffset, copyRegion.size },
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT });
        }
        break;
    }
    case JobCommandTypes::CopyBufferToImage: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferImageData>(command);
        FormatClassProperties formatProperties = getFormatClassProperties(data->image.getFormat());

        for (const auto& copyRegion : data->copyRegions) {
            addBufferAccess(
                bufferAccesses,
                data->buffer,
                { copyRegion.bufferOffset, getImageCopySizeBytes(copyRegion, formatProperties) },
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT });
            addImageAccess(
                imageAccesses,
                data->image,
                ImageSubresourceRange(copyRegion.imageSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        break;
    }
    case JobCommandTypes::CopyImageToBuffer: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferImageData>(command);
        FormatClassProperties formatProperties = getFormatClassProperties(data->image.getFormat());

        for (const auto& copyRegion : data->copyRegions) {
            addImageAccess(
                imageAccesses,
                data->image,
                ImageSubresourceRange(copyRegion.imageSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            addBufferAccess(
                bufferAccesses,
                data->buffer,
                { copyRegion.bufferOffset, getImageCopySizeBytes(copyRegion, formatProperties) },
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT });
        }
        break;
    }
    case JobCommandTypes::CopyImage:
    case JobCommandTypes::ResolveImage: {
        auto* data = getCommandData<JobRecordStorage::CopyImageData>(command);
        for (const auto& copyRegion : data->copyRegions) {
            addImageAccess(
                imageAccesses,
                data->srcImage,
                ImageSubresourceRange(copyRegion.srcSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            addImageAccess(
                imageAccesses,
                data->dstImage,
                ImageSubresourceRange(copyRegion.dstSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        break;
    }
    case JobCommandTypes::BlitImage: {
        auto* data = getCommandData<JobRecordStorage::BlitImageData>(command);
        for (const auto& blitRegion : data->blitRegions) {
            addImageAccess(
                imageAccesses,
                data->srcImage,
                ImageSubresourceRange(blitRegion.srcSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            addImageAccess(
                imageAccesses,
                data->dstImage,
                ImageSubresourceRange(blitRegion.dstSubresource),
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        break;
    }
    case JobCommandTypes::ClearImage: {
        auto* data = getCommandData<JobRecordStorage::ClearImageData>(command);
        for (const auto& range : data->ranges) {
            addImageAccess(
                imageAccesses,
                data->dstImage,
                range,
                { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT },
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        break;
    }
    case JobCommandTypes::ExecuteComputePass: {
        auto* data = getCommandData<JobRecordStorage::ExecuteComputePassData>(command);
        for (const BufferComputeAccess& entry : data->pass->getBufferAccesses()) {
            VkPipelineStageFlags stageMask;
            VkAccessFlags accessMask;
            bool isAtomic;
            convertComputeAccessToVkAccess(entry.accessMask, &stageMask, &accessMask, &isAtomic);
            addBufferAccess(bufferAccesses, entry.buffer, { stageMask, accessMask });
        }
        for (const ImageComputeAccess& entry : data->pass->getImageAccesses()) {
            VkPipelineStageFlags stageMask;
            VkAccessFlags accessMask;
            bool isAtomic;
            convertComputeAccessToVkAccess(entry.accessMask, &stageMask, &accessMask, &isAtomic);
            VkImageLayout layout = vkGetImageLayoutFromComputeAccess(entry.accessMask);
            addImageAccess(imageAccesses, entry.image, entry.range, { stageMask, accessMask }, layout);
        }
        break;
    }
    case JobCommandTypes::ExecuteRenderPass: {
        auto* data = getCommandData<JobRecordStorage::ExecuteRenderPassData>(command);
        for (const BufferRenderAccess& entry : data->pass->getBufferAccesses()) {
            VkPipelineStageFlags stageMask;
            VkAccessFlags accessMask;
            bool isAtomic;
            convertRenderAccessToVkAccess(entry.accessMask, &stageMask, &accessMask, &isAtomic);
            addBufferAccess(bufferAccesses, entry.buffer, { stageMask, accessMask });
        }
        for (const ImageRenderAccess& entry : data->pass->getImageAccesses()) {
            VkPipelineStageFlags stageMask;
            VkAccessFlags accessMask;
            bool isAtomic;
            convertRenderAccessToVkAccess(entry.accessMask, &stageMask, &accessMask, &isAtomic);
            VkImageLayout layout = vkGetImageLayoutFromRenderAccess(entry.accessMask);
            addImageAccess(imageAccesses, entry.image, entry.range, { stageMask, accessMask }, layout);
        }
        for (const AttachmentAccess& entry : data->pass->getAttachmentAccesses()) {
            ImageAccessRange range;
            ResourceAccess access;
            VkImageLayout layout;
            if (!entry.imageView.isNull()) {
                entry.convertToVkAccess(&range, &access, &layout);
                addImageAccess(imageAccesses, entry.imageView, range, access, layout);
            }
        }
        break;
    }
    case JobCommandTypes::BuildAccelerationStructures: {
        auto* data = getCommandData<JobRecordStorage::BuildAccelerationStructuresData>(command);
        const auto asBuildStage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        const auto asBuildInput = ResourceAccess(asBuildStage, VK_ACCESS_SHADER_READ_BIT);

        for (const AccelerationStructureBuildInfo& buildInfo : data->buildInfos) {
            const BufferView& dstBuffer = buildInfo.dstView.getBackingBufferView();

            VkAccessFlags dstAccess = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            // Destination structure could also be read from in case of an in-place update
            if (buildInfo.mode == AccelerationStructureBuildMode::Update && buildInfo.srcView.isNull())
                dstAccess |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            addBufferAccess(bufferAccesses, dstBuffer, { asBuildStage, dstAccess });

            if (!buildInfo.srcView.isNull()) {
                const BufferView& srcBuffer = buildInfo.srcView.getBackingBufferView();
                addBufferAccess(
                    bufferAccesses, srcBuffer, { asBuildStage, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR });
            }

            if (!buildInfo.instanceGeometry.instanceBuffer.isNull())
                addBufferAccess(bufferAccesses, buildInfo.instanceGeometry.instanceBuffer, asBuildInput);

            for (const TriangleGeometryBuildInfo& triangles : buildInfo.triangleGeometries) {
                addBufferAccess(bufferAccesses, triangles.vertexBuffer, asBuildInput);
                if (!triangles.indexBuffer.isNull())
                    addBufferAccess(bufferAccesses, triangles.indexBuffer, asBuildInput);
                if (!triangles.transformBuffer.isNull())
                    addBufferAccess(bufferAccesses, triangles.transformBuffer, asBuildInput);
            }

            for (const AABBGeometryBuildInfo& aabbs : buildInfo.aabbGeometries) {
                addBufferAccess(bufferAccesses, aabbs.aabbBuffer, asBuildInput);
            }
        }

        const auto scratchAccess = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        for (const BufferView& scratchBuffer : data->scratchBuffers) {
            addBufferAccess(bufferAccesses, scratchBuffer, { asBuildStage, scratchAccess });
        }
        break;
    }
    case JobCommandTypes::BeginDebugLabel:
    case JobCommandTypes::InsertDebugLabel:
    case JobCommandTypes::EndDebugLabel:
        break; // Commands without resource accesses
    default: {
        TEPHRA_ASSERTD(false, "Unimplemented command.");
    }
    }
}

void recordCommand(PrimaryBufferRecorder& recorder, JobRecordStorage::CommandMetadata* command) {
    const VulkanCommandInterface& vkiCommands = recorder.getVkiCommands();

    switch (command->commandType) {
    case JobCommandTypes::FillBuffer: {
        auto* data = getCommandData<JobRecordStorage::FillBufferData>(command);
        uint64_t dstOffset;
        VkBufferHandle vkDstBufferHandle = data->dstBuffer.vkResolveBufferHandle(&dstOffset);
        vkiCommands.cmdFillBuffer(
            recorder.requestBuffer(), vkDstBufferHandle, dstOffset, data->dstBuffer.getSize(), data->value);
        break;
    }
    case JobCommandTypes::UpdateBuffer: {
        auto* data = getCommandData<JobRecordStorage::UpdateBufferData>(command);
        uint64_t dstOffset;
        VkBufferHandle vkDstBufferHandle = data->dstBuffer.vkResolveBufferHandle(&dstOffset);
        vkiCommands.cmdUpdateBuffer(
            recorder.requestBuffer(), vkDstBufferHandle, dstOffset, data->dstBuffer.getSize(), data->data);
        break;
    }
    case JobCommandTypes::CopyBuffer: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferData>(command);
        uint64_t srcOffset;
        VkBufferHandle vkSrcBufferHandle = data->srcBuffer.vkResolveBufferHandle(&srcOffset);
        uint64_t dstOffset;
        VkBufferHandle vkDstBufferHandle = data->dstBuffer.vkResolveBufferHandle(&dstOffset);

        for (auto& copyRegion : data->copyRegions) {
            copyRegion.srcOffset += srcOffset;
            copyRegion.dstOffset += dstOffset;
        }

        vkiCommands.cmdCopyBuffer(
            recorder.requestBuffer(),
            vkSrcBufferHandle,
            vkDstBufferHandle,
            static_cast<uint32_t>(data->copyRegions.size()),
            data->copyRegions.data());
        break;
    }
    case JobCommandTypes::CopyBufferToImage: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferImageData>(command);
        uint64_t srcOffset;
        VkBufferHandle vkSrcBufferHandle = data->buffer.vkResolveBufferHandle(&srcOffset);
        uint32_t dstBaseMipLevel;
        uint32_t dstBaseArrayLayer;
        VkImageHandle vkDstImageHandle = data->image.vkResolveImageHandle(&dstBaseMipLevel, &dstBaseArrayLayer);

        for (auto& copyRegion : data->copyRegions) {
            copyRegion.bufferOffset += srcOffset;
            copyRegion.imageSubresource.mipLevel += dstBaseMipLevel;
            copyRegion.imageSubresource.baseArrayLayer += dstBaseArrayLayer;
        }

        vkiCommands.cmdCopyBufferToImage(
            recorder.requestBuffer(),
            vkSrcBufferHandle,
            vkDstImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(data->copyRegions.size()),
            vkCastConvertibleStructPtr(data->copyRegions.data()));
        break;
    }
    case JobCommandTypes::CopyImageToBuffer: {
        auto* data = getCommandData<JobRecordStorage::CopyBufferImageData>(command);
        uint32_t srcBaseMipLevel;
        uint32_t srcBaseArrayLayer;
        VkImageHandle vkSrcImageHandle = data->image.vkResolveImageHandle(&srcBaseMipLevel, &srcBaseArrayLayer);
        uint64_t dstOffset;
        VkBufferHandle vkDstBufferHandle = data->buffer.vkResolveBufferHandle(&dstOffset);

        for (auto& copyRegion : data->copyRegions) {
            copyRegion.imageSubresource.mipLevel += srcBaseMipLevel;
            copyRegion.imageSubresource.baseArrayLayer += srcBaseArrayLayer;
            copyRegion.bufferOffset += dstOffset;
        }

        vkiCommands.cmdCopyImageToBuffer(
            recorder.requestBuffer(),
            vkSrcImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vkDstBufferHandle,
            static_cast<uint32_t>(data->copyRegions.size()),
            vkCastConvertibleStructPtr(data->copyRegions.data()));
        break;
    }
    case JobCommandTypes::CopyImage: {
        auto* data = getCommandData<JobRecordStorage::CopyImageData>(command);
        uint32_t srcBaseMipLevel;
        uint32_t srcBaseArrayLayer;
        VkImageHandle vkSrcImageHandle = data->srcImage.vkResolveImageHandle(&srcBaseMipLevel, &srcBaseArrayLayer);
        uint32_t dstBaseMipLevel;
        uint32_t dstBaseArrayLayer;
        VkImageHandle vkDstImageHandle = data->dstImage.vkResolveImageHandle(&dstBaseMipLevel, &dstBaseArrayLayer);

        for (auto& copyRegion : data->copyRegions) {
            copyRegion.srcSubresource.mipLevel += srcBaseMipLevel;
            copyRegion.srcSubresource.baseArrayLayer += srcBaseArrayLayer;
            copyRegion.dstSubresource.mipLevel += dstBaseMipLevel;
            copyRegion.dstSubresource.baseArrayLayer += dstBaseArrayLayer;
        }

        vkiCommands.cmdCopyImage(
            recorder.requestBuffer(),
            vkSrcImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vkDstImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(data->copyRegions.size()),
            vkCastConvertibleStructPtr(data->copyRegions.data()));
        break;
    }
    case JobCommandTypes::BlitImage: {
        auto* data = getCommandData<JobRecordStorage::BlitImageData>(command);
        uint32_t srcBaseMipLevel;
        uint32_t srcBaseArrayLayer;
        VkImageHandle vkSrcImageHandle = data->srcImage.vkResolveImageHandle(&srcBaseMipLevel, &srcBaseArrayLayer);
        uint32_t dstBaseMipLevel;
        uint32_t dstBaseArrayLayer;
        VkImageHandle vkDstImageHandle = data->dstImage.vkResolveImageHandle(&dstBaseMipLevel, &dstBaseArrayLayer);

        for (auto& blitRegion : data->blitRegions) {
            blitRegion.srcSubresource.mipLevel += srcBaseMipLevel;
            blitRegion.srcSubresource.baseArrayLayer += srcBaseArrayLayer;
            blitRegion.dstSubresource.mipLevel += dstBaseMipLevel;
            blitRegion.dstSubresource.baseArrayLayer += dstBaseArrayLayer;
        }

        vkiCommands.cmdBlitImage(
            recorder.requestBuffer(),
            vkSrcImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vkDstImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(data->blitRegions.size()),
            vkCastConvertibleStructPtr(data->blitRegions.data()),
            vkCastConvertibleEnum(data->filter));
        break;
    }
    case JobCommandTypes::ClearImage: {
        auto* data = getCommandData<JobRecordStorage::ClearImageData>(command);
        uint32_t dstBaseMipLevel;
        uint32_t dstBaseArrayLayer;
        VkImageHandle vkDstImageHandle = data->dstImage.vkResolveImageHandle(&dstBaseMipLevel, &dstBaseArrayLayer);

        for (auto& range : data->ranges) {
            range.baseMipLevel += dstBaseMipLevel;
            range.baseArrayLayer += dstBaseArrayLayer;
        }

        // Vulkan has separate commands for clearing color and depth/stencil aspects
        if (data->dstImage.getWholeRange().aspectMask.contains(ImageAspect::Color)) {
            vkiCommands.cmdClearColorImage(
                recorder.requestBuffer(),
                vkDstImageHandle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &data->value.vkValue.color,
                static_cast<uint32_t>(data->ranges.size()),
                vkCastConvertibleStructPtr(data->ranges.data()));
        } else {
            vkiCommands.cmdClearDepthStencilImage(
                recorder.requestBuffer(),
                vkDstImageHandle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &data->value.vkValue.depthStencil,
                static_cast<uint32_t>(data->ranges.size()),
                vkCastConvertibleStructPtr(data->ranges.data()));
        }
        break;
    }
    case JobCommandTypes::ResolveImage: {
        auto* data = getCommandData<JobRecordStorage::CopyImageData>(command);
        uint32_t srcBaseMipLevel;
        uint32_t srcBaseArrayLayer;
        VkImageHandle vkSrcImageHandle = data->srcImage.vkResolveImageHandle(&srcBaseMipLevel, &srcBaseArrayLayer);
        uint32_t dstBaseMipLevel;
        uint32_t dstBaseArrayLayer;
        VkImageHandle vkDstImageHandle = data->dstImage.vkResolveImageHandle(&dstBaseMipLevel, &dstBaseArrayLayer);

        for (auto& copyRegion : data->copyRegions) {
            copyRegion.srcSubresource.mipLevel += srcBaseMipLevel;
            copyRegion.srcSubresource.baseArrayLayer += srcBaseArrayLayer;
            copyRegion.dstSubresource.mipLevel += dstBaseMipLevel;
            copyRegion.dstSubresource.baseArrayLayer += dstBaseArrayLayer;
        }

        // VkImageCopy and VkImageResolve are identical structures
        const VkImageCopy* copyRegionsData = vkCastConvertibleStructPtr(data->copyRegions.data());
        const VkImageResolve* resolveRegionsData = reinterpret_cast<const VkImageResolve*>(copyRegionsData);

        vkiCommands.cmdResolveImage(
            recorder.requestBuffer(),
            vkSrcImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vkDstImageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(data->copyRegions.size()),
            resolveRegionsData);
        break;
    }
    case JobCommandTypes::ExecuteComputePass: {
        auto* data = getCommandData<JobRecordStorage::ExecuteComputePassData>(command);
        data->pass->recordPass(recorder);
        break;
    }
    case JobCommandTypes::ExecuteRenderPass: {
        auto* data = getCommandData<JobRecordStorage::ExecuteRenderPassData>(command);
        data->pass->recordPass(recorder);
        break;
    }
    case JobCommandTypes::BeginDebugLabel: {
        auto* data = getCommandData<JobRecordStorage::DebugLabelData>(command);
        TEPHRA_ASSERT(vkiCommands.cmdBeginDebugUtilsLabelEXT != nullptr);
        VkDebugUtilsLabelEXT label = makeDebugLabel(*data);
        vkiCommands.cmdBeginDebugUtilsLabelEXT(recorder.requestBuffer(), &label);
        break;
    }
    case JobCommandTypes::InsertDebugLabel: {
        auto* data = getCommandData<JobRecordStorage::DebugLabelData>(command);
        TEPHRA_ASSERT(vkiCommands.cmdInsertDebugUtilsLabelEXT != nullptr);
        VkDebugUtilsLabelEXT label = makeDebugLabel(*data);
        vkiCommands.cmdInsertDebugUtilsLabelEXT(recorder.requestBuffer(), &label);
        break;
    }
    case JobCommandTypes::EndDebugLabel: {
        TEPHRA_ASSERT(vkiCommands.cmdEndDebugUtilsLabelEXT != nullptr);
        vkiCommands.cmdEndDebugUtilsLabelEXT(recorder.requestBuffer());
        break;
    }
    case JobCommandTypes::BuildAccelerationStructures: {
        TEPHRA_ASSERT(vkiCommands.cmdBuildAccelerationStructuresKHR != nullptr);
        auto* data = getCommandData<JobRecordStorage::BuildAccelerationStructuresData>(command);

        // Prepare and aggregate vulkan structures for all builds
        ScratchVector<VkAccelerationStructureBuildGeometryInfoKHR> vkBuildInfos;
        ScratchVector<const VkAccelerationStructureBuildRangeInfoKHR*> vkRangeInfosPtrs;
        vkBuildInfos.reserve(data->buildInfos.size());
        vkRangeInfosPtrs.reserve(data->buildInfos.size());

        for (std::size_t i = 0; i < data->buildInfos.size(); i++) {
            AccelerationStructureImpl& asImpl = AccelerationStructureImpl::getAccelerationStructureImpl(
                data->buildInfos[i].dstView);

            auto [vkBuildInfo, vkRangeInfos] = asImpl.getBuilder().prepareBuild(
                data->buildInfos[i], data->scratchBuffers[i]);

            vkBuildInfos.push_back(vkBuildInfo);
            vkRangeInfosPtrs.push_back(vkRangeInfos);
        }

        vkiCommands.cmdBuildAccelerationStructuresKHR(
            recorder.requestBuffer(),
            static_cast<uint32_t>(vkBuildInfos.size()),
            vkBuildInfos.data(),
            vkRangeInfosPtrs.data());
        break;
    }
    case JobCommandTypes::ExportBuffer:
    case JobCommandTypes::ExportImage:
    case JobCommandTypes::DiscardImageContents:
    case JobCommandTypes::ImportExternalBuffer:
    case JobCommandTypes::ImportExternalImage: {
        break; // No-op commands
    }
    default: {
        TEPHRA_ASSERTD(false, "Unimplemented command.");
    }
    }
}

}
