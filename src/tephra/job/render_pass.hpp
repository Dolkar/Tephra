#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <tephra/render.hpp>
#include <deque>

namespace tp {

class PrimaryBufferRecorder;

class RenderPass {
public:
    explicit RenderPass(DeviceContainer* deviceImpl)
        : deviceImpl(deviceImpl), inlineListDebugTarget(DebugTarget::makeSilent()) {}

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    ArrayView<const BufferRenderAccess> getBufferAccesses() const {
        return view(bufferAccesses);
    }

    ArrayView<const ImageRenderAccess> getImageAccesses() const {
        return view(imageAccesses);
    }

    void assignDeferred(
        const RenderPassSetup& setup,
        const DebugTarget& listDebugTarget,
        ArrayView<RenderList>& listsToAssign);

    void assignInline(const RenderPassSetup& setup, RenderInlineCallback recordingCallback, DebugTarget listDebugTarget);

    void recordPass(PrimaryBufferRecorder& recorder);

    TEPHRA_MAKE_NONCOPYABLE(RenderPass);
    TEPHRA_MAKE_NONMOVABLE(RenderPass);
    ~RenderPass() = default;

private:
    DeviceContainer* deviceImpl = nullptr;

    std::vector<BufferRenderAccess> bufferAccesses;
    std::vector<ImageRenderAccess> imageAccesses;

    bool isInline = false;
    RenderInlineCallback inlineRecordingCallback;
    DebugTarget inlineListDebugTarget;
    VkRenderingInfo vkInlineRenderingInfo;
    std::vector<VkCommandBufferHandle> vkDeferredCommandBuffers;
    std::vector<VkRenderingAttachmentInfo> vkRenderingAttachments;

    void prepareAccesses(const RenderPassSetup& setup);

    VkRenderingInfo prepareRenderingInfo(const RenderPassSetup& setup);
};

}
