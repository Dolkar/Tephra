
#include "render_pass.hpp"
#include "accesses.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

void AttachmentAccess::convertToVkAccess(
    ImageAccessRange* rangePtr,
    ResourceAccess* accessPtr,
    VkImageLayout* layoutPtr) const {
    TEPHRA_ASSERT(!imageView.isNull());
    *rangePtr = imageView.getWholeRange();
    *layoutPtr = layout;

    // Deduce Vulkan access from just the layout chosen in prepareRendering:
    // TODO: Sync2
    switch (layout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        accessPtr->accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        accessPtr->stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        rangePtr->aspectMask = ImageAspect::Depth;
        break;
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        rangePtr->aspectMask = ImageAspect::Stencil;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        rangePtr->aspectMask = ImageAspect::Depth;
        break;
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        accessPtr->stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        accessPtr->accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        rangePtr->aspectMask = ImageAspect::Stencil;
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

    isInline = false;
    inlineRecordingCallback = {};
    inlineListDebugTarget = DebugTarget::makeSilent();

    // Create space for empty command buffers and pass pointers to them to each list.
    // They will be filled out once recorded
    TEPHRA_ASSERT(listsToAssign.size() > 0);
    vkDeferredCommandBuffers.clear();
    vkDeferredCommandBuffers.resize(listsToAssign.size());

    // Create render lists. We want them all to be a part of the same render pass, so we add
    // suspend and resume flags as necessary
    VkRenderingInfo vkRenderingInfo = prepareRendering(setup);
    VkRenderingFlags baseRenderingFlags = vkRenderingInfo.flags;

    for (std::size_t i = 0; i < listsToAssign.size(); i++) {
        vkRenderingInfo.flags = baseRenderingFlags;
        if (i != 0) // not first
            vkRenderingInfo.flags |= VK_RENDERING_RESUMING_BIT;
        if (i + 1 != listsToAssign.size()) // not last
            vkRenderingInfo.flags |= VK_RENDERING_SUSPENDING_BIT;

        listsToAssign[i] = RenderList(
            &deviceImpl->getCommandPoolPool()->getVkiCommands(),
            &vkDeferredCommandBuffers[i],
            vkRenderingInfo,
            listDebugTarget);
    }
}

void RenderPass::assignInline(
    const RenderPassSetup& setup,
    RenderInlineCallback recordingCallback,
    DebugTarget listDebugTarget) {
    prepareNonAttachmentAccesses(setup);

    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(listDebugTarget);
    vkInlineRenderingInfo = prepareRendering(setup);
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
    if (isInline) {
        // Begin and end rendering here
        VkCommandBufferHandle vkCommandBufferHandle = recorder.requestBuffer();
        recorder.getVkiCommands().cmdBeginRendering(vkCommandBufferHandle, &vkInlineRenderingInfo);

        // Call the inline command recorder callback
        RenderList inlineList = RenderList(
            &recorder.getVkiCommands(), vkCommandBufferHandle, std::move(inlineListDebugTarget));
        inlineRecordingCallback(inlineList);

        recorder.getVkiCommands().cmdEndRendering(vkCommandBufferHandle);
    } else {
        for (VkCommandBufferHandle vkCommandBuffer : vkDeferredCommandBuffers) {
            if (!vkCommandBuffer.isNull())
                recorder.appendBuffer(vkCommandBuffer);
        }
    }
}

void RenderPass::prepareNonAttachmentAccesses(const RenderPassSetup& setup) {
    bufferAccesses.clear();
    bufferAccesses.insert(bufferAccesses.begin(), setup.bufferAccesses.begin(), setup.bufferAccesses.end());
    imageAccesses.clear();
    imageAccesses.insert(imageAccesses.begin(), setup.imageAccesses.begin(), setup.imageAccesses.end());
}

VkRenderingInfo RenderPass::prepareRendering(const RenderPassSetup& setup) {
    VkRenderingInfo renderingInfo;
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.pNext = nullptr;
    // We add resuming / suspending flags later depending on the method
    renderingInfo.flags = 0;
    renderingInfo.renderArea = setup.renderArea;
    renderingInfo.layerCount = setup.layerCount;
    renderingInfo.viewMask = setup.viewMask;

    // Prepare attachments, but we can't resolve the images yet
    vkRenderingAttachments.clear();
    attachmentAccesses.clear();
    auto addAttachmentRef = [this](const ImageView& imageView, VkImageLayout layout) {
        this->attachmentAccesses.push_back({ imageView, layout });
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
        TEPHRA_ASSERT(hasDepth || hasStencil);

        { // Depth attachment
            VkRenderingAttachmentInfo vkDepthAttachment = vkAttachmentCommon;
            vkDepthAttachment.loadOp = vkCastConvertibleEnum(attachment.depthLoadOp);
            vkDepthAttachment.storeOp = vkCastConvertibleEnum(attachment.depthStoreOp);

            if (attachment.depthReadOnly)
                vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            else
                vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            vkDepthAttachment.imageView = addAttachmentRef(
                hasDepth ? attachment.image : ImageView(), vkDepthAttachment.imageLayout);

            vkDepthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            vkDepthAttachment.resolveImageView = addAttachmentRef(
                hasDepth ? attachment.resolveImage : ImageView(), vkDepthAttachment.resolveImageLayout);

            vkRenderingAttachments.push_back(vkDepthAttachment);
        }

        { // Stencil attachment
            VkRenderingAttachmentInfo vkStencilAttachment = vkAttachmentCommon;
            vkStencilAttachment.loadOp = vkCastConvertibleEnum(attachment.stencilLoadOp);
            vkStencilAttachment.storeOp = vkCastConvertibleEnum(attachment.stencilStoreOp);

            if (attachment.stencilReadOnly)
                vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
            else
                vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            vkStencilAttachment.imageView = addAttachmentRef(
                hasStencil ? attachment.image : ImageView(), vkStencilAttachment.imageLayout);

            vkStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            vkStencilAttachment.resolveImageView = addAttachmentRef(
                hasStencil ? attachment.resolveImage : ImageView(), vkStencilAttachment.resolveImageLayout);

            vkRenderingAttachments.push_back(vkStencilAttachment);
        }
    }

    // Color attachments
    for (const ColorAttachment& attachment : setup.colorAttachments) {
        VkRenderingAttachmentInfo& vkAttachment = vkRenderingAttachments.emplace_back();
        vkAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        vkAttachment.pNext = nullptr;
        vkAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkAttachment.imageView = addAttachmentRef(attachment.image, vkAttachment.imageLayout);

        bool hasResolve = !attachment.resolveImage.isNull();
        vkAttachment.resolveMode = hasResolve ? vkCastConvertibleEnum(attachment.resolveMode) : VK_RESOLVE_MODE_NONE;
        vkAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkAttachment.resolveImageView = addAttachmentRef(attachment.resolveImage, vkAttachment.resolveImageLayout);

        vkAttachment.clearValue = attachment.clearValue.vkValue;
        vkAttachment.loadOp = vkCastConvertibleEnum(attachment.loadOp);
        vkAttachment.storeOp = vkCastConvertibleEnum(attachment.storeOp);
    }

    // Assign pointers now that vkRenderingAttachments is final
    renderingInfo.pDepthAttachment = &vkRenderingAttachments[0];
    renderingInfo.pStencilAttachment = &vkRenderingAttachments[1];
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkRenderingAttachments.size() - 2);
    renderingInfo.pColorAttachments = &vkRenderingAttachments[2];

    return renderingInfo;
}

}
