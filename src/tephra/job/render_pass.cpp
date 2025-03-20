
#include "render_pass.hpp"
#include "accesses.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

void AttachmentAccess::convertToVkAccess(
    ImageAccessRange* rangePtr,
    ResourceAccess* accessPtr,
    VkImageLayout* layoutPtr) {
    TEPHRA_ASSERT(!imageView.isNull());
    *rangePtr = imageView.getWholeRange();
    rangePtr->aspectMask = aspect;
    *layoutPtr = layout;

    switch (layout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        accessPtr->stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        accessPtr->accessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        TEPHRA_ASSERT(aspect == ImageAspect::Depth);
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        TEPHRA_ASSERT(aspect == ImageAspect::Stencil);
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        TEPHRA_ASSERT(aspect == ImageAspect::Depth || aspect == ImageAspect::Stencil);
        // This should match ReadAccess::DepthStencilAttachment
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    default:
        TEPHRA_ASSERTD(false, "Unexpected layout");
    }
}

void RenderPass::assignDeferred(
    const RenderPassSetup& setup,
    const DebugTarget& listDebugTarget,
    ArrayView<RenderList>& listsToAssign) {
    prepareNonAttachmentAccesses(setup);
    prepareRendering(setup, true);

    isInline = false;
    inlineRecordingCallback = {};
    inlineListDebugTarget = DebugTarget::makeSilent();

    // Create space for empty command buffers and pass pointers to them to each list.
    // They will be filled out once recorded
    TEPHRA_ASSERT(listsToAssign.size() > 0);
    vkDeferredCommandBuffers.clear();
    vkDeferredCommandBuffers.resize(listsToAssign.size());

    // Create render lists using secondary command buffer with rendering inheritance
    prepareInheritance(setup);

    int multiviewViewCount = tp::max(countBitsSet(vkRenderingInfo.viewMask), 1u);

    for (std::size_t i = 0; i < listsToAssign.size(); i++) {
        listsToAssign[i] = RenderList(
            &deviceImpl->getCommandPoolPool()->getVkiCommands(),
            &vkDeferredCommandBuffers[i],
            &vkInheritanceInfo,
            multiviewViewCount,
            listDebugTarget);
    }
}

void RenderPass::assignInline(
    const RenderPassSetup& setup,
    RenderInlineCallback recordingCallback,
    DebugTarget listDebugTarget) {
    prepareNonAttachmentAccesses(setup);
    prepareRendering(setup, false);

    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(listDebugTarget);
    vkDeferredCommandBuffers.clear();
}

void RenderPass::resolveAttachmentViews() {
    TEPHRA_ASSERT(vkRenderingAttachments.size() * 2 == attachmentAccesses.size());

    // Map attachment accesses to rendering attachments and resolve the images now that it's safe to do so
    for (std::size_t i = 0; i < vkRenderingAttachments.size(); i++) {
        VkRenderingAttachmentInfo& vkAttachment = vkRenderingAttachments[i];

        vkAttachment.imageView = attachmentAccesses[i * 2].imageView.vkGetImageViewHandle();
        TEPHRA_ASSERT(vkAttachment.imageLayout == attachmentAccesses[i * 2].layout);

        vkAttachment.resolveImageView = attachmentAccesses[i * 2 + 1].imageView.vkGetImageViewHandle();
        TEPHRA_ASSERT(vkAttachment.resolveImageLayout == attachmentAccesses[i * 2 + 1].layout);
    }
}

void RenderPass::recordPass(PrimaryBufferRecorder& recorder) {
    // Begin and end rendering here
    VkCommandBufferHandle vkPrimaryCommandBufferHandle = recorder.requestBuffer();
    recorder.getVkiCommands().cmdBeginRendering(vkPrimaryCommandBufferHandle, &vkRenderingInfo);

    if (isInline) {
        // Call the inline command recorder callback
        int multiviewViewCount = tp::max(countBitsSet(vkRenderingInfo.viewMask), 1u);
        RenderList inlineList = RenderList(
            &recorder.getVkiCommands(),
            vkPrimaryCommandBufferHandle,
            &recorder.getQueryRecorder(),
            multiviewViewCount,
            std::move(inlineListDebugTarget));
        inlineRecordingCallback(inlineList);

    } else {
        // Execute deferred command buffers that ended up being recorded
        ScratchVector<VkCommandBuffer> vkFilledCommandBuffers;

        for (VkCommandBufferHandle vkCommandBuffer : vkDeferredCommandBuffers) {
            if (!vkCommandBuffer.isNull())
                vkFilledCommandBuffers.push_back(vkCommandBuffer);
        }

        if (!vkFilledCommandBuffers.empty()) {
            recorder.getVkiCommands().cmdExecuteCommands(
                recorder.requestBuffer(),
                static_cast<uint32_t>(vkFilledCommandBuffers.size()),
                vkFilledCommandBuffers.data());
        }
    }

    recorder.getVkiCommands().cmdEndRendering(vkPrimaryCommandBufferHandle);
}

void RenderPass::prepareNonAttachmentAccesses(const RenderPassSetup& setup) {
    bufferAccesses.clear();
    bufferAccesses.insert(bufferAccesses.begin(), setup.bufferAccesses.begin(), setup.bufferAccesses.end());
    imageAccesses.clear();
    imageAccesses.insert(imageAccesses.begin(), setup.imageAccesses.begin(), setup.imageAccesses.end());
}

void RenderPass::prepareRendering(const RenderPassSetup& setup, bool useSecondaryCmdBuffers) {
    if (setup.vkRenderingInfoExtMap != nullptr)
        vkRenderingInfoExtMap = *setup.vkRenderingInfoExtMap;
    else
        vkRenderingInfoExtMap.clear();

    vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    vkRenderingInfo.pNext = vkRenderingInfoExtMap.empty() ? nullptr : &vkRenderingInfoExtMap.front();
    vkRenderingInfo.flags = useSecondaryCmdBuffers ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT : 0;
    vkRenderingInfo.renderArea = setup.renderArea;
    vkRenderingInfo.layerCount = setup.layerCount;
    vkRenderingInfo.viewMask = setup.viewMask;

    // Prepare attachments, but we can't resolve the images yet
    vkRenderingAttachments.clear();
    attachmentAccesses.clear();
    auto addAttachmentRef = [this](const ImageView& imageView, VkImageLayout layout, ImageAspect aspect) {
        this->attachmentAccesses.push_back({ imageView, layout, aspect });
        return VK_NULL_HANDLE;
    };

    // Depth and stencil attachments
    {
        const DepthStencilAttachment& attachment = setup.depthStencilAttachment;

        // Prepare common fields
        VkRenderingAttachmentInfo vkAttachmentCommon;
        vkAttachmentCommon.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        vkAttachmentCommon.pNext = nullptr;

        vkAttachmentCommon.clearValue = attachment.clearValue.vkValue;
        bool hasResolve = !attachment.resolveImage.isNull();
        vkAttachmentCommon.resolveMode = hasResolve ? vkCastConvertibleEnum(attachment.resolveMode) :
                                                      VK_RESOLVE_MODE_NONE;

        bool hasImage = !attachment.image.isNull();
        bool hasDepth = hasImage && attachment.image.getWholeRange().aspectMask.contains(ImageAspect::Depth);
        bool hasStencil = hasImage && attachment.image.getWholeRange().aspectMask.contains(ImageAspect::Stencil);

        { // Depth attachment
            VkRenderingAttachmentInfo vkDepthAttachment = vkAttachmentCommon;
            vkDepthAttachment.loadOp = vkCastConvertibleEnum(attachment.depthLoadOp);
            vkDepthAttachment.storeOp = vkCastConvertibleEnum(attachment.depthStoreOp);

            if (attachment.depthReadOnly)
                vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            else
                vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            vkDepthAttachment.imageView = addAttachmentRef(
                hasDepth ? attachment.image : ImageView(), vkDepthAttachment.imageLayout, ImageAspect::Depth);

            vkDepthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            vkDepthAttachment.resolveImageView = addAttachmentRef(
                hasDepth ? attachment.resolveImage : ImageView(),
                vkDepthAttachment.resolveImageLayout,
                ImageAspect::Depth);

            vkRenderingAttachments.push_back(vkDepthAttachment);
        }

        { // Stencil attachment
            VkRenderingAttachmentInfo vkStencilAttachment = vkAttachmentCommon;
            vkStencilAttachment.loadOp = vkCastConvertibleEnum(attachment.stencilLoadOp);
            vkStencilAttachment.storeOp = vkCastConvertibleEnum(attachment.stencilStoreOp);

            if (attachment.stencilReadOnly)
                vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            else
                vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            vkStencilAttachment.imageView = addAttachmentRef(
                hasStencil ? attachment.image : ImageView(), vkStencilAttachment.imageLayout, ImageAspect::Stencil);

            vkStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            vkStencilAttachment.resolveImageView = addAttachmentRef(
                hasStencil ? attachment.resolveImage : ImageView(),
                vkStencilAttachment.resolveImageLayout,
                ImageAspect::Stencil);

            vkRenderingAttachments.push_back(vkStencilAttachment);
        }
    }

    // Color attachments
    for (const ColorAttachment& attachment : setup.colorAttachments) {
        VkRenderingAttachmentInfo& vkAttachment = vkRenderingAttachments.emplace_back();
        vkAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        vkAttachment.pNext = nullptr;
        vkAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkAttachment.imageView = addAttachmentRef(attachment.image, vkAttachment.imageLayout, ImageAspect::Color);

        bool hasResolve = !attachment.resolveImage.isNull();
        vkAttachment.resolveMode = hasResolve ? vkCastConvertibleEnum(attachment.resolveMode) : VK_RESOLVE_MODE_NONE;
        vkAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkAttachment.resolveImageView = addAttachmentRef(
            attachment.resolveImage, vkAttachment.resolveImageLayout, ImageAspect::Color);

        vkAttachment.clearValue = attachment.clearValue.vkValue;
        vkAttachment.loadOp = vkCastConvertibleEnum(attachment.loadOp);
        vkAttachment.storeOp = vkCastConvertibleEnum(attachment.storeOp);
    }

    // Assign pointers now that vkRenderingAttachments is final
    vkRenderingInfo.pDepthAttachment = &vkRenderingAttachments[0];
    vkRenderingInfo.pStencilAttachment = &vkRenderingAttachments[1];
    vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkRenderingAttachments.size() - 2);
    vkRenderingInfo.pColorAttachments = vkRenderingAttachments.data() + 2;
}

void RenderPass::prepareInheritance(const RenderPassSetup& setup) {
    TEPHRA_ASSERT(vkRenderingInfo.sType == VK_STRUCTURE_TYPE_RENDERING_INFO_KHR);

    vkInheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
    vkInheritanceRenderingInfo.pNext = nullptr;
    // Flags need to be identical, except for this bit
    vkInheritanceRenderingInfo.flags = vkRenderingInfo.flags & (~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
    vkInheritanceRenderingInfo.viewMask = vkRenderingInfo.viewMask;
    vkInheritanceRenderingInfo.rasterizationSamples = vkCastConvertibleEnum(MultisampleLevel::x1);

    if (vkRenderingInfoExtMap.contains<VkMultiviewPerViewAttributesInfoNVX>()) {
        // This structure needs to be present for command buffer inheritance, too
        vkMultiviewInfoExt = vkRenderingInfoExtMap.get<VkMultiviewPerViewAttributesInfoNVX>();
        vkMultiviewInfoExt.pNext = nullptr;
        vkInheritanceRenderingInfo.pNext = &vkMultiviewInfoExt;
    }

    // Depth and stencil attachments
    {
        const DepthStencilAttachment& attachment = setup.depthStencilAttachment;

        bool hasImage = !attachment.image.isNull();
        bool hasDepth = hasImage && attachment.image.getWholeRange().aspectMask.contains(ImageAspect::Depth);
        bool hasStencil = hasImage && attachment.image.getWholeRange().aspectMask.contains(ImageAspect::Stencil);

        if (hasDepth) {
            vkInheritanceRenderingInfo.depthAttachmentFormat = vkCastConvertibleEnum(attachment.image.getFormat());
        } else {
            vkInheritanceRenderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        }

        if (hasStencil) {
            vkInheritanceRenderingInfo.stencilAttachmentFormat = vkCastConvertibleEnum(attachment.image.getFormat());
        } else {
            vkInheritanceRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
        }

        if (hasImage) {
            vkInheritanceRenderingInfo.rasterizationSamples = vkCastConvertibleEnum(attachment.image.getSampleLevel());
        }
    }

    // Color attachments
    vkColorAttachmentFormats.clear();
    vkColorAttachmentFormats.reserve(setup.colorAttachments.size());
    for (const ColorAttachment& attachment : setup.colorAttachments) {
        if (!attachment.image.isNull()) {
            vkColorAttachmentFormats.push_back(vkCastConvertibleEnum(attachment.image.getFormat()));
            vkInheritanceRenderingInfo.rasterizationSamples = vkCastConvertibleEnum(attachment.image.getSampleLevel());
        } else {
            vkColorAttachmentFormats.push_back(VK_FORMAT_UNDEFINED);
        }
    }
    vkInheritanceRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkColorAttachmentFormats.size());
    vkInheritanceRenderingInfo.pColorAttachmentFormats = vkColorAttachmentFormats.data();

    // Also need base inheritance info to redirect to vkInheritanceRenderingInfo
    vkInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    vkInheritanceInfo.pNext = &vkInheritanceRenderingInfo;
    vkInheritanceInfo.renderPass = VK_NULL_HANDLE;
    vkInheritanceInfo.subpass = 0;
    vkInheritanceInfo.framebuffer = nullptr;
    // We don't need query inheritance
    vkInheritanceInfo.occlusionQueryEnable = false;
    vkInheritanceInfo.queryFlags = 0;
    vkInheritanceInfo.pipelineStatistics = 0;
}

}
