#include "compute_pass.hpp"
#include "command_recording.hpp"
#include "../device/device_container.hpp"

namespace tp {

std::vector<VkCommandBufferHandle>& ComputePass::assignDeferred(const ComputePassSetup& setup, std::size_t bufferCount) {
    reassign(setup);

    isInline = false;
    inlineRecordingCallback = {};
    inlineListDebugTarget = DebugTarget::makeSilent();
    // Create space for bufferCount empty command buffers. They will be assigned once recorded
    TEPHRA_ASSERT(bufferCount > 0);
    vkPreparedCommandBuffers.clear();
    vkPreparedCommandBuffers.resize(bufferCount);

    return vkPreparedCommandBuffers;
}

void ComputePass::assignInline(
    const ComputePassSetup& setup,
    ComputeInlineCallback recordingCallback,
    DebugTarget computeListDebugTarget) {
    reassign(setup);
    isInline = true;
    inlineRecordingCallback = std::move(recordingCallback);
    inlineListDebugTarget = std::move(computeListDebugTarget);
    vkPreparedCommandBuffers.clear();
}

void ComputePass::recordPass(PrimaryBufferRecorder& recorder) {
    if (isInline) {
        // Call the inline command recorder callback
        ComputeList inlineList = ComputeList(
            &recorder.getVkiCommands(), recorder.requestBuffer(), std::move(inlineListDebugTarget));
        inlineRecordingCallback(inlineList);
    } else {
        for (VkCommandBufferHandle vkCommandBuffer : vkPreparedCommandBuffers) {
            if (!vkCommandBuffer.isNull())
                recorder.appendBuffer(vkCommandBuffer);
        }
    }
}

void ComputePass::reassign(const ComputePassSetup& setup) {
    bufferAccesses.clear();
    bufferAccesses.insert(bufferAccesses.begin(), setup.bufferAccesses.begin(), setup.bufferAccesses.end());
    imageAccesses.clear();
    imageAccesses.insert(imageAccesses.begin(), setup.imageAccesses.begin(), setup.imageAccesses.end());
}

}
