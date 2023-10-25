#pragma once

#include "../common_impl.hpp"
#include <tephra/compute.hpp>
#include <deque>

namespace tp {

class PrimaryBufferRecorder;

class ComputePass {
public:
    explicit ComputePass(DeviceContainer* deviceImpl)
        : deviceImpl(deviceImpl), inlineListDebugTarget(DebugTarget::makeSilent()) {}

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    ArrayView<const BufferComputeAccess> getBufferAccesses() const {
        return view(bufferAccesses);
    }

    ArrayView<const ImageComputeAccess> getImageAccesses() const {
        return view(imageAccesses);
    }

    void assignDeferred(
        const ComputePassSetup& setup,
        const DebugTarget& listDebugTarget,
        ArrayView<ComputeList>& listsToAssign);

    void assignInline(
        const ComputePassSetup& setup,
        ComputeInlineCallback recordingCallback,
        DebugTarget listDebugTarget);

    void recordPass(PrimaryBufferRecorder& recorder);

    TEPHRA_MAKE_NONCOPYABLE(ComputePass);
    TEPHRA_MAKE_NONMOVABLE(ComputePass);
    ~ComputePass() = default;

private:
    DeviceContainer* deviceImpl = nullptr;

    std::vector<BufferComputeAccess> bufferAccesses;
    std::vector<ImageComputeAccess> imageAccesses;

    bool isInline = false;
    ComputeInlineCallback inlineRecordingCallback;
    DebugTarget inlineListDebugTarget;
    std::vector<VkCommandBufferHandle> vkDeferredCommandBuffers;

    void prepareAccesses(const ComputePassSetup& setup);
};

}
