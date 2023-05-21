
#include "render_pass.hpp"
#include "accesses.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

bool AttachmentAccess::isSplitAccess() const {
    return firstAccess.stageMask != lastAccess.stageMask || firstAccess.accessMask != lastAccess.accessMask ||
        firstLayout != lastLayout;
}

void countAttachmentReferences(
    ArrayView<const AttachmentBinding> attachmentBindings,
    uint32_t* inputAttachmentCount,
    uint32_t* colorAttachmentCount,
    bool* hasResolveAttachments,
    bool* hasDepthStencilAttachment) {
    *inputAttachmentCount = 0;
    *colorAttachmentCount = 0;
    *hasResolveAttachments = false;
    *hasDepthStencilAttachment = false;

    for (const AttachmentBinding& binding : attachmentBindings) {
        switch (binding.bindPoint.type) {
        case AttachmentBindPointType::Input:
            *inputAttachmentCount = tp::max(*inputAttachmentCount, binding.bindPoint.number + 1);
            break;
        case AttachmentBindPointType::Color:
            *colorAttachmentCount = tp::max(*colorAttachmentCount, binding.bindPoint.number + 1);
            break;
        case AttachmentBindPointType::ResolveFromColor:
            *colorAttachmentCount = tp::max(*colorAttachmentCount, binding.bindPoint.number + 1);
            *hasResolveAttachments = true;
            break;
        case AttachmentBindPointType::DepthStencil:
            *hasDepthStencilAttachment = true;
            break;
        default:
            break;
        }
    }
}

// Fills Vulkan attachment references for the subpass description from the given bindings
void fillAttachmentReferences(
    ArrayView<const AttachmentBinding> attachmentBindings,
    ArrayView<VkAttachmentReference> attachmentReferences,
    uint32_t* attachmentReferenceOffset,
    VkSubpassDescription* subpassInfo) {
    // TODO: Handle using the same attachment in multiple bindings, can use depth stencil readonly layouts for input,
    // too

    bool hasResolveAttachments;
    bool hasDepthStencilAttachment;
    countAttachmentReferences(
        attachmentBindings,
        &subpassInfo->inputAttachmentCount,
        &subpassInfo->colorAttachmentCount,
        &hasResolveAttachments,
        &hasDepthStencilAttachment);

    VkAttachmentReference* pInputAttachments = attachmentReferences.data() + *attachmentReferenceOffset;
    (*attachmentReferenceOffset) += subpassInfo->inputAttachmentCount;
    VkAttachmentReference* pColorAttachments = attachmentReferences.data() + *attachmentReferenceOffset;
    (*attachmentReferenceOffset) += subpassInfo->colorAttachmentCount;
    VkAttachmentReference* pResolveAttachments = nullptr;
    if (hasResolveAttachments) {
        pResolveAttachments = attachmentReferences.data() + *attachmentReferenceOffset;
        (*attachmentReferenceOffset) += subpassInfo->colorAttachmentCount;
    }
    VkAttachmentReference* pDepthStencilAttachment = nullptr;
    if (hasDepthStencilAttachment) {
        pDepthStencilAttachment = attachmentReferences.data() + *attachmentReferenceOffset;
        (*attachmentReferenceOffset)++;
    }

    subpassInfo->pInputAttachments = pInputAttachments;
    subpassInfo->pColorAttachments = pColorAttachments;
    subpassInfo->pResolveAttachments = pResolveAttachments;
    subpassInfo->pDepthStencilAttachment = pDepthStencilAttachment;

    for (const AttachmentBinding& binding : attachmentBindings) {
        switch (binding.bindPoint.type) {
        case AttachmentBindPointType::Input: {
            VkAttachmentReference& attachmentReference = *(pInputAttachments + binding.bindPoint.number);
            attachmentReference.attachment = binding.attachmentIndex;
            attachmentReference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        }
        case AttachmentBindPointType::Color: {
            VkAttachmentReference& attachmentReference = *(pColorAttachments + binding.bindPoint.number);
            attachmentReference.attachment = binding.attachmentIndex;
            attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        }
        case AttachmentBindPointType::ResolveFromColor: {
            TEPHRA_ASSERT(pResolveAttachments != nullptr);
            VkAttachmentReference& attachmentReference = *(pResolveAttachments + binding.bindPoint.number);
            attachmentReference.attachment = binding.attachmentIndex;
            attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        }
        case AttachmentBindPointType::DepthStencil: {
            TEPHRA_ASSERT(pDepthStencilAttachment != nullptr);
            VkAttachmentReference& attachmentReference = *pDepthStencilAttachment;
            attachmentReference.attachment = binding.attachmentIndex;
            if (binding.bindPoint.isReadOnly)
                attachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            else
                attachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        }
        default:
            break;
        }
    }

    // Preserve attachments get filled elsewhere as that requires knowledge of other subpasses
}

RenderPassTemplate::RenderPassTemplate(
    DeviceContainer* deviceImpl,
    ArrayParameter<const AttachmentDescription> attachmentDescriptions,
    ArrayParameter<const SubpassLayout> subpassLayouts)
    : deviceImpl(deviceImpl) {
    // Count attachment references
    std::size_t attachmentReferenceCount = 0;
    for (const SubpassLayout& subpassLayout : subpassLayouts) {
        uint32_t inputAttachmentCount;
        uint32_t colorAttachmentCount;
        bool hasResolveAttachments;
        bool hasDepthStencilAttachment;
        countAttachmentReferences(
            subpassLayout.bindings,
            &inputAttachmentCount,
            &colorAttachmentCount,
            &hasResolveAttachments,
            &hasDepthStencilAttachment);

        uint32_t totalAttachmentCount = inputAttachmentCount + colorAttachmentCount +
            (hasDepthStencilAttachment ? 1 : 0) + (hasResolveAttachments ? colorAttachmentCount : 0);

        // Sanity check
        TEPHRA_ASSERT(totalAttachmentCount < 32);

        attachmentReferenceCount += totalAttachmentCount;
    }

    attachmentReferences.resize(attachmentReferenceCount);
    for (std::size_t i = 0; i < attachmentReferenceCount; i++) {
        attachmentReferences[i].attachment = VK_ATTACHMENT_UNUSED;
        attachmentReferences[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    // Fill subpass descriptions and attachment references
    // Also gather the details of the first and last use of each attachment
    // This is needed to deduce what attachments need to be preserved and for later synchronization
    // against the renderpass
    ScratchVector<std::pair<uint32_t, uint32_t>> attachmentFirstLastUses;
    attachmentFirstLastUses.resize(attachmentDescriptions.size(), { ~0, 0 });
    attachmentAccesses.resize(attachmentDescriptions.size());

    subpassInfos.reserve(subpassLayouts.size());
    uint32_t attachmentReferenceOffset = 0;
    for (uint32_t subpassIndex = 0; subpassIndex < subpassLayouts.size(); subpassIndex++) {
        const SubpassLayout& subpassLayout = subpassLayouts[subpassIndex];
        VkSubpassDescription subpassInfo;
        subpassInfo.flags = 0;
        subpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        uint32_t firstAttachment = attachmentReferenceOffset;
        fillAttachmentReferences(
            subpassLayout.bindings, view(attachmentReferences), &attachmentReferenceOffset, &subpassInfo);
        uint32_t lastAttachment = attachmentReferenceOffset;

        subpassInfos.push_back(subpassInfo);

        for (const AttachmentBinding& binding : subpassLayout.bindings) {
            AttachmentAccess& attachmentAccess = attachmentAccesses[binding.attachmentIndex];
            std::pair<uint32_t, uint32_t>& attachmentFirstLastUse = attachmentFirstLastUses[binding.attachmentIndex];
            uint32_t& firstAttachmentUse = attachmentFirstLastUse.first;
            uint32_t& lastAttachmentUse = attachmentFirstLastUse.second;

            if (subpassIndex < firstAttachmentUse) {
                firstAttachmentUse = subpassIndex;
                attachmentAccess.firstAccess = {};
            }
            if (subpassIndex == firstAttachmentUse) {
                ResourceAccess newAccess;
                convertAttachmentAccessToVkAccess(
                    binding.bindPoint.type, binding.bindPoint.isReadOnly, &newAccess.stageMask, &newAccess.accessMask);
                attachmentAccess.firstAccess = attachmentAccess.firstAccess | newAccess;
            }

            if (subpassIndex > lastAttachmentUse) {
                lastAttachmentUse = subpassIndex;
                attachmentAccess.lastAccess = {};
            }
            if (subpassIndex == lastAttachmentUse) {
                ResourceAccess newAccess;
                convertAttachmentAccessToVkAccess(
                    binding.bindPoint.type, binding.bindPoint.isReadOnly, &newAccess.stageMask, &newAccess.accessMask);
                attachmentAccess.lastAccess = attachmentAccess.lastAccess | newAccess;
            }
        }

        for (uint32_t i = firstAttachment; i < lastAttachment; i++) {
            const VkAttachmentReference& attachmentReference = attachmentReferences[i];
            AttachmentAccess& attachmentAccess = attachmentAccesses[attachmentReference.attachment];
            const std::pair<uint32_t, uint32_t>& attachmentFirstLastUse =
                attachmentFirstLastUses[attachmentReference.attachment];
            uint32_t firstAttachmentUse = attachmentFirstLastUse.first;
            uint32_t lastAttachmentUse = attachmentFirstLastUse.second;

            // First and last attachment uses were already set in the previous loop
            if (subpassIndex == firstAttachmentUse) {
                attachmentAccess.firstLayout = attachmentReference.layout;
            }
            if (subpassIndex == lastAttachmentUse) {
                attachmentAccess.lastLayout = attachmentReference.layout;
            }
        }
    }

    attachmentInfos.reserve(attachmentDescriptions.size());
    // Fill what we can for attachment descriptions
    for (uint32_t attachmentIndex = 0; attachmentIndex < attachmentDescriptions.size(); attachmentIndex++) {
        const AttachmentDescription& attachment = attachmentDescriptions[attachmentIndex];
        const AttachmentAccess& attachmentAccess = attachmentAccesses[attachmentIndex];

        VkAttachmentDescription attachmentInfo;
        attachmentInfo.flags = 0; // Should never alias?
        attachmentInfo.format = vkCastConvertibleEnum(attachment.format);
        attachmentInfo.samples = vkCastConvertibleEnum(attachment.sampleCount);
        // Specialized in individual derivatives, covered by Renderpass compatibility
        attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentInfo.initialLayout = attachmentAccess.firstLayout;
        VkImageLayout finalLayout = attachmentAccess.lastLayout;
        if (finalLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
            // finalLayout cannot be undefined, but attachment is unused
            finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        attachmentInfo.finalLayout = finalLayout;
        attachmentInfos.push_back(attachmentInfo);
    }

    // Allocate preserve attachments for the worst case
    preserveAttachmentReferences.resize(subpassLayouts.size() * attachmentDescriptions.size());

    // Deduce preserve attachments
    uint32_t preserveAttachmentOffset = 0;
    for (uint32_t subpassIndex = 0; subpassIndex < subpassLayouts.size(); subpassIndex++) {
        const SubpassLayout& subpassLayout = subpassLayouts[subpassIndex];
        VkSubpassDescription& subpassDescription = subpassInfos[subpassIndex];

        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = preserveAttachmentReferences.data() + preserveAttachmentOffset;

        for (uint32_t attachmentIndex = 0; attachmentIndex < attachmentDescriptions.size(); attachmentIndex++) {
            auto [attachmentFirstUse, attachmentLastUse] = attachmentFirstLastUses[attachmentIndex];
            bool shouldPreserve = subpassIndex >= attachmentFirstUse && subpassIndex <= attachmentLastUse;

            bool usesAttachment = false;
            for (const AttachmentBinding& binding : subpassLayout.bindings) {
                if (binding.attachmentIndex == attachmentIndex) {
                    usesAttachment = true;
                    break;
                }
            }

            if (shouldPreserve && !usesAttachment) { // Need to add a preserve attachment reference
                preserveAttachmentReferences[preserveAttachmentOffset++] = attachmentIndex;
                subpassDescription.preserveAttachmentCount++;
            }
        }
    }

    // Handle subpass dependenciess
    std::size_t subpassDependencyCount = 0;
    for (const SubpassLayout& subpassLayout : subpassLayouts) {
        subpassDependencyCount += subpassLayout.dependencies.size();
    }
    dependencyInfos.reserve(subpassDependencyCount);

    for (uint32_t subpassIndex = 0; subpassIndex < subpassLayouts.size(); subpassIndex++) {
        const SubpassLayout& subpassLayout = subpassLayouts[subpassIndex];
        for (const SubpassDependency& dependency : subpassLayout.dependencies) {
            VkSubpassDependency dependencyInfo;
            dependencyInfo.srcSubpass = dependency.sourceSubpassIndex;
            dependencyInfo.dstSubpass = subpassIndex;

            bool isAtomic;
            convertRenderAccessToVkAccess(
                dependency.additionalSourceAccessMask,
                &dependencyInfo.srcStageMask,
                &dependencyInfo.srcAccessMask,
                &isAtomic);
            convertRenderAccessToVkAccess(
                dependency.additionalDestinationAccessMask,
                &dependencyInfo.dstStageMask,
                &dependencyInfo.dstAccessMask,
                &isAtomic);

            dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            // Deduce dependencies between render pass attachments
            for (const AttachmentBinding& srcBinding : subpassLayouts[dependency.sourceSubpassIndex].bindings) {
                for (const AttachmentBinding& dstBinding : subpassLayout.bindings) {
                    if (srcBinding.attachmentIndex == dstBinding.attachmentIndex &&
                        (!srcBinding.bindPoint.isReadOnly || !dstBinding.bindPoint.isReadOnly)) {
                        // Update source dependency
                        VkPipelineStageFlags stageMask;
                        VkAccessFlags accessMask;
                        convertAttachmentAccessToVkAccess(
                            srcBinding.bindPoint.type, srcBinding.bindPoint.isReadOnly, &stageMask, &accessMask);
                        dependencyInfo.srcStageMask |= stageMask;
                        dependencyInfo.srcAccessMask |= accessMask;

                        // Update destination dependency
                        convertAttachmentAccessToVkAccess(
                            dstBinding.bindPoint.type, dstBinding.bindPoint.isReadOnly, &stageMask, &accessMask);
                        dependencyInfo.dstStageMask |= stageMask;
                        dependencyInfo.dstAccessMask |= accessMask;
                    }
                }
            }

            dependencyInfos.push_back(dependencyInfo);
        }
    }

    // Finally, find a good multisample level default. Using the attachments from the layout will work 99% of the time.
    subpassDefaultMultisampleLevels.reserve(subpassLayouts.size());
    for (uint32_t subpassIndex = 0; subpassIndex < subpassLayouts.size(); subpassIndex++) {
        const SubpassLayout& subpassLayout = subpassLayouts[subpassIndex];
        uint32_t multisampleLevel = static_cast<uint32_t>(MultisampleLevel::x1);

        for (const AttachmentBinding& binding : subpassLayout.bindings) {
            uint32_t bindingLevel = static_cast<uint32_t>(attachmentDescriptions[binding.attachmentIndex].sampleCount);
            multisampleLevel = tp::max(multisampleLevel, bindingLevel);
        }

        subpassDefaultMultisampleLevels.push_back(static_cast<MultisampleLevel>(multisampleLevel));
    }
}

void RenderPass::assignSetup(const RenderPassSetup& setup, const char* debugName) {
    // Cleanup and reuse existing resources
    bufferAccesses.clear();
    bufferAccesses.insert(bufferAccesses.begin(), setup.bufferAccesses.begin(), setup.bufferAccesses.end());
    imageAccesses.clear();
    imageAccesses.insert(imageAccesses.begin(), setup.imageAccesses.begin(), setup.imageAccesses.end());

    const RenderPassTemplate* renderPassTemplate = setup.layout->renderPassTemplate.get();

    TEPHRA_ASSERT(setup.attachments.size() == renderPassTemplate->attachmentInfos.size());
    attachmentAccesses.clear();
    attachmentAccesses.reserve(setup.attachments.size());
    attachmentClearValues.clear();
    attachmentClearValues.reserve(setup.attachments.size());

    for (uint32_t attachmentIndex = 0; attachmentIndex < setup.attachments.size(); attachmentIndex++) {
        const RenderPassAttachment& renderPassAttachment = setup.attachments[attachmentIndex];
        AttachmentAccess templateAccess = renderPassTemplate->attachmentAccesses[attachmentIndex];
        templateAccess.image = renderPassAttachment.image;
        attachmentAccesses.emplace_back(std::move(templateAccess));
        attachmentClearValues.push_back(renderPassAttachment.clearValue);
    }

    subpasses.resize(renderPassTemplate->subpassInfos.size());
    renderArea = setup.renderArea;
    layerCount = setup.layerCount;

    // Modify render pass attachments and accesses to fit the specific usage of this render pass
    ScratchVector<VkAttachmentDescription> attachmentInfos;
    attachmentInfos.insert(
        attachmentInfos.begin(),
        renderPassTemplate->attachmentInfos.begin(),
        renderPassTemplate->attachmentInfos.end());

    for (uint32_t attachmentIndex = 0; attachmentIndex < setup.attachments.size(); attachmentIndex++) {
        const RenderPassAttachment& renderPassAttachment = setup.attachments[attachmentIndex];
        VkAttachmentDescription& attachmentInfo = attachmentInfos[attachmentIndex];
        FormatClassProperties attachmentFormatProperties = getFormatClassProperties(
            renderPassAttachment.image.getFormat());
        bool hasDepthAspect = attachmentFormatProperties.aspectMask.contains(ImageAspect::Depth);
        bool hasStencilAspect = attachmentFormatProperties.aspectMask.contains(ImageAspect::Stencil);
        bool hasColorAspect = attachmentFormatProperties.aspectMask.contains(ImageAspect::Color);

        // Update first and last accesses according to load and store ops
        AttachmentAccess& attachmentAccess = attachmentAccesses[attachmentIndex];

        ResourceAccess loadAccess;
        if (hasColorAspect) {
            loadAccess.stageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            if (renderPassAttachment.loadOp == AttachmentLoadOp::Load)
                loadAccess.accessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            else
                loadAccess.accessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (hasDepthAspect) {
            loadAccess.stageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            if (renderPassAttachment.loadOp == AttachmentLoadOp::Load)
                loadAccess.accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            else
                loadAccess.accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (hasStencilAspect) {
            loadAccess.stageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            if (renderPassAttachment.stencilLoadOp == AttachmentLoadOp::Load)
                loadAccess.accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            else
                loadAccess.accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        ResourceAccess storeAccess;
        if (hasColorAspect) {
            storeAccess = ResourceAccess(
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        } else { // Depth or stencil aspect
            storeAccess = ResourceAccess(
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }

        // TODO: Not entirely sure about this, but we might need to synchronize against both the load/store ops
        // and the first / last accesses. Do so just to be on the safe side.
        attachmentAccess.firstAccess |= loadAccess;
        attachmentAccess.lastAccess |= storeAccess;
        // TODO: Cannot use VK_IMAGE_LAYOUT_UNDEFINED together with synchronizing through pipeline barriers
        // Undefined -> Undefined transition isn't valid
        // attachmentAccess.firstLayout = attachmentLoaded ? attachmentAccess.firstLayout : VK_IMAGE_LAYOUT_UNDEFINED;

        attachmentInfo.loadOp = vkCastConvertibleEnum(renderPassAttachment.loadOp);
        attachmentInfo.storeOp = vkCastConvertibleEnum(renderPassAttachment.storeOp);
        attachmentInfo.stencilLoadOp = vkCastConvertibleEnum(renderPassAttachment.stencilLoadOp);
        attachmentInfo.stencilStoreOp = vkCastConvertibleEnum(renderPassAttachment.stencilStoreOp);
        attachmentInfo.initialLayout = attachmentAccess.firstLayout;
        attachmentInfo.finalLayout = attachmentAccess.lastLayout;
    }

    VkRenderPassHandle vkRenderPassHandle = deviceImpl->getLogicalDevice()->createRenderPass(
        view(attachmentInfos), view(renderPassTemplate->subpassInfos), view(renderPassTemplate->dependencyInfos));

    deviceImpl->getLogicalDevice()->setObjectDebugName(vkRenderPassHandle, debugName);

    renderPassHandle = deviceImpl->vkMakeHandleLifeguard(vkRenderPassHandle);

    framebufferHandle.destroyHandle();
}

std::vector<VkCommandBufferHandle>& RenderPass::assignDeferredSubpass(uint32_t subpassIndex, std::size_t bufferCount) {
    TEPHRA_ASSERT(subpassIndex < subpasses.size());
    return subpasses[subpassIndex].assignDeferred(bufferCount);
}

void RenderPass::assignInlineSubpass(
    uint32_t subpassIndex,
    RenderInlineCallback recordingCallback,
    DebugTarget renderListDebugTarget) {
    TEPHRA_ASSERT(subpassIndex < subpasses.size());
    subpasses[subpassIndex].assignInline(std::move(recordingCallback), std::move(renderListDebugTarget));
}

void RenderPass::createFramebuffer() {
    ScratchVector<VkImageViewHandle> attachmentImages;
    attachmentImages.reserve(attachmentAccesses.size());

    for (uint32_t attachmentIndex = 0; attachmentIndex < attachmentAccesses.size(); attachmentIndex++) {
        const ImageView& attachmentImage = attachmentAccesses[attachmentIndex].image;
        attachmentImages.push_back(attachmentImage.vkGetImageViewHandle());
    }

    VkFramebufferHandle vkFramebufferHandle = deviceImpl->getLogicalDevice()->createFramebuffer(
        renderPassHandle.vkGetHandle(),
        view(attachmentImages),
        renderArea.offset.x + renderArea.extent.width,
        renderArea.offset.y + renderArea.extent.height,
        layerCount);

    framebufferHandle = deviceImpl->vkMakeHandleLifeguard(vkFramebufferHandle);
}

RenderPass::ExecutionMethod RenderPass::getExecutionMethod() const {
    TEPHRA_ASSERT(!subpasses.empty());

    if (subpasses.size() == 1 && subpasses[0].vkPreparedCommandBuffers.size() == 1)
        return ExecutionMethod::PrerecordedRenderPass;
    else
        return ExecutionMethod::General;
}

void RenderPass::recordPass(PrimaryBufferRecorder& recorder) {
    const VulkanCommandInterface* vkiCommands = &deviceImpl->getCommandPoolPool()->getVkiCommands();
    TEPHRA_ASSERT(!subpasses.empty());

    ExecutionMethod executionMethod = getExecutionMethod();
    if (executionMethod == ExecutionMethod::PrerecordedRenderPass) {
        if (!subpasses[0].vkPreparedCommandBuffers[0].isNull())
            recorder.appendBuffer(subpasses[0].vkPreparedCommandBuffers[0]);
    } else {
        TEPHRA_ASSERT(executionMethod == ExecutionMethod::General);
        recordBeginRenderPass(recorder.requestBuffer());

        for (uint32_t subpassIndex = 0; subpassIndex < subpasses.size(); subpassIndex++) {
            Subpass& subpass = subpasses[subpassIndex];
            if (subpass.isInline) {
                auto inlineList = RenderList(
                    vkiCommands, recorder.requestBuffer(), this, ~0u, std::move(subpass.inlineListDebugTarget));
                subpass.inlineRecordingCallback(inlineList);
            } else if (!subpass.vkPreparedCommandBuffers.empty()) {
                vkiCommands->cmdExecuteCommands(
                    recorder.requestBuffer(),
                    static_cast<uint32_t>(subpass.vkPreparedCommandBuffers.size()),
                    vkCastTypedHandlePtr(subpass.vkPreparedCommandBuffers.data()));
            }

            if (subpassIndex + 1 < subpasses.size()) {
                recordNextSubpass(recorder.requestBuffer(), subpassIndex + 1);
            }
        }

        vkiCommands->cmdEndRenderPass(recorder.requestBuffer());
    }
}

void RenderPass::recordBeginRenderPass(VkCommandBufferHandle vkCommandBuffer) const {
    const VulkanCommandInterface* vkiCommands = &deviceImpl->getCommandPoolPool()->getVkiCommands();
    TEPHRA_ASSERT(!subpasses.empty());

    VkRenderPassBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.renderPass = renderPassHandle.vkGetHandle();
    beginInfo.framebuffer = framebufferHandle.vkGetHandle();
    beginInfo.renderArea = renderArea;
    beginInfo.clearValueCount = static_cast<uint32_t>(attachmentClearValues.size());
    beginInfo.pClearValues = vkCastConvertibleStructPtr(attachmentClearValues.data());

    bool recordSecondary = getExecutionMethod() == ExecutionMethod::General && !subpasses[0].isInline;
    vkiCommands->cmdBeginRenderPass(
        vkCommandBuffer,
        &beginInfo,
        recordSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::recordNextSubpass(VkCommandBufferHandle vkCommandBuffer, uint32_t subpassIndex) const {
    const VulkanCommandInterface* vkiCommands = &deviceImpl->getCommandPoolPool()->getVkiCommands();
    TEPHRA_ASSERT(subpassIndex < subpasses.size());
    TEPHRA_ASSERT(subpassIndex > 0);
    TEPHRA_ASSERT(getExecutionMethod() == ExecutionMethod::General);

    bool recordSecondary = !subpasses[0].isInline;
    vkiCommands->cmdNextSubpass(
        vkCommandBuffer, recordSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
}

std::vector<VkCommandBufferHandle>& RenderPass::Subpass::assignDeferred(std::size_t bufferCount) {
    isInline = false;
    inlineRecordingCallback = {};
    inlineListDebugTarget = DebugTarget::makeSilent();
    // Create space for bufferCount empty command buffers. They will be assigned once recorded
    TEPHRA_ASSERT(bufferCount > 0);
    vkPreparedCommandBuffers.clear();
    vkPreparedCommandBuffers.resize(bufferCount);

    return vkPreparedCommandBuffers;
}

void RenderPass::Subpass::assignInline(RenderInlineCallback recordingCallback, DebugTarget renderListDebugTarget) {
    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(renderListDebugTarget);
    vkPreparedCommandBuffers.clear();
}

}
