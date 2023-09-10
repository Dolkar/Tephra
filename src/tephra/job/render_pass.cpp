
#include "render_pass.hpp"
#include "accesses.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

void RenderPass::assignDeferred(
    const RenderPassSetup& setup,
    const DebugTarget& listDebugTarget,
    ArrayView<RenderList>& listsToAssign) {
    prepareAccesses(setup);

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
    VkRenderingInfo vkRenderingInfo = prepareRenderingInfo(setup);
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
    prepareAccesses(setup);

    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(listDebugTarget);
    vkInlineRenderingInfo = prepareRenderingInfo(setup);
    vkDeferredCommandBuffers.clear();
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

void RenderPass::prepareAccesses(const RenderPassSetup& setup) {}

VkRenderingInfo RenderPass::prepareRenderingInfo(const RenderPassSetup& setup) {}

}
