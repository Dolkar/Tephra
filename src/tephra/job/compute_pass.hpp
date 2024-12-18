#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <tephra/compute.hpp>
#include <deque>

namespace tp {

class PrimaryBufferRecorder;

struct StoredBufferComputeAccess {
    StoredBufferView buffer;
    ComputeAccessMask accessMask;

    StoredBufferComputeAccess(const BufferComputeAccess& access)
        : buffer(access.buffer), accessMask(access.accessMask) {}
};

struct StoredImageComputeAccess {
    StoredImageView image;
    ImageSubresourceRange range;
    ComputeAccessMask accessMask;

    StoredImageComputeAccess(const ImageComputeAccess& access)
        : image(access.image), range(access.range), accessMask(access.accessMask) {}
};

class ComputePass {
public:
    explicit ComputePass(DeviceContainer* deviceImpl)
        : deviceImpl(deviceImpl), inlineListDebugTarget(DebugTarget::makeSilent()) {}

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    ArrayView<StoredBufferComputeAccess> getBufferAccesses() {
        return view(bufferAccesses);
    }

    ArrayView<StoredImageComputeAccess> getImageAccesses() {
        return view(imageAccesses);
    }

    void assignDeferred(
        const ComputePassSetup& setup,
        const JobData* jobData,
        const DebugTarget& listDebugTarget,
        ArrayView<ComputeList>& listsToAssign);

    void assignInline(
        const ComputePassSetup& setup,
        ComputeInlineCallback recordingCallback,
        DebugTarget listDebugTarget);

    void recordPass(const JobData* jobData, PrimaryBufferRecorder& recorder);

    TEPHRA_MAKE_NONCOPYABLE(ComputePass);
    TEPHRA_MAKE_NONMOVABLE(ComputePass);
    ~ComputePass() = default;

private:
    DeviceContainer* deviceImpl = nullptr;

    std::vector<StoredBufferComputeAccess> bufferAccesses;
    std::vector<StoredImageComputeAccess> imageAccesses;

    bool isInline = false;
    ComputeInlineCallback inlineRecordingCallback;
    DebugTarget inlineListDebugTarget;
    std::vector<VkCommandBufferHandle> vkDeferredCommandBuffers;

    void prepareAccesses(const ComputePassSetup& setup);
};

}
