#include "compute_pass.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

void ComputePass::assignDeferred(
    const ComputePassSetup& setup,
    const DebugTarget& listDebugTarget,
    ArrayView<ComputeList>& listsToAssign) {
    prepareAccesses(setup);

    isInline = false;
    inlineRecordingCallback = {};
    inlineListDebugTarget = DebugTarget::makeSilent();

    // Create space for empty command buffers and pass pointers to them to each list.
    // They will be filled out once recorded
    TEPHRA_ASSERT(listsToAssign.size() > 0);
    vkDeferredCommandBuffers.clear();
    vkDeferredCommandBuffers.resize(listsToAssign.size());

    for (std::size_t i = 0; i < listsToAssign.size(); i++) {
        listsToAssign[i] = ComputeList(
            &deviceImpl->getCommandPoolPool()->getVkiCommands(), &vkDeferredCommandBuffers[i], listDebugTarget);
    }
}

void ComputePass::assignInline(
    const ComputePassSetup& setup,
    ComputeInlineCallback recordingCallback,
    DebugTarget listDebugTarget) {
    prepareAccesses(setup);

    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(listDebugTarget);
    vkDeferredCommandBuffers.clear();
}

void ComputePass::recordPass(PrimaryBufferRecorder& recorder) {
    if (isInline) {
        // Call the inline command recorder callback
        ComputeList inlineList = ComputeList(
            &recorder.getVkiCommands(),
            recorder.requestBuffer(),
            &recorder.getQueryRecorder(),
            std::move(inlineListDebugTarget));
        inlineRecordingCallback(inlineList);
    } else {
        for (VkCommandBufferHandle vkCommandBuffer : vkDeferredCommandBuffers) {
            if (!vkCommandBuffer.isNull())
                recorder.appendBuffer(vkCommandBuffer);
        }
    }
}

void ComputePass::prepareAccesses(const ComputePassSetup& setup) {
    bufferAccesses.clear();
    bufferAccesses.insert(bufferAccesses.begin(), setup.bufferAccesses.begin(), setup.bufferAccesses.end());
    imageAccesses.clear();
    imageAccesses.insert(imageAccesses.begin(), setup.imageAccesses.begin(), setup.imageAccesses.end());
}

}
