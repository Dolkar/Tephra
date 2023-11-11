#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <tephra/render.hpp>
#include <deque>

namespace tp {

class PrimaryBufferRecorder;

// Represents access of a render attachment and stores unresolved image view
struct AttachmentAccess {
    ImageView imageView;
    VkImageLayout layout;

    void convertToVkAccess(ImageAccessRange* rangePtr, ResourceAccess* accessPtr, VkImageLayout* layoutPtr) const;
};

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

    ArrayView<const AttachmentAccess> getAttachmentAccesses() const {
        return view(attachmentAccesses);
    }

    void assignDeferred(
        const RenderPassSetup& setup,
        const DebugTarget& listDebugTarget,
        ArrayView<RenderList>& listsToAssign);

    void assignInline(const RenderPassSetup& setup, RenderInlineCallback recordingCallback, DebugTarget listDebugTarget);

    // Resolves attachments to finish the rendering info for command recording.
    // We can only resolve attachments when starting the record, after enqueue. Until then, we store them in
    // attachmentAccesses.
    void resolveAttachmentViews();

    void recordPass(PrimaryBufferRecorder& recorder);

    TEPHRA_MAKE_NONCOPYABLE(RenderPass);
    TEPHRA_MAKE_NONMOVABLE(RenderPass);
    ~RenderPass() = default;

private:
    DeviceContainer* deviceImpl = nullptr;

    std::vector<BufferRenderAccess> bufferAccesses;
    std::vector<ImageRenderAccess> imageAccesses;
    // Every two entries here correspond to one entry in vkRenderingAttachments (imageView and resolveImageView)
    // Entries can be null
    std::vector<AttachmentAccess> attachmentAccesses;

    bool isInline = false;
    RenderInlineCallback inlineRecordingCallback;
    DebugTarget inlineListDebugTarget;
    VkRenderingInfo vkRenderingInfo = {};
    VkCommandBufferInheritanceRenderingInfo vkInheritanceRenderingInfo = {};
    VkCommandBufferInheritanceInfo vkInheritanceInfo = {};

    std::vector<VkCommandBufferHandle> vkDeferredCommandBuffers;
    std::vector<VkRenderingAttachmentInfo> vkRenderingAttachments;
    std::vector<VkFormat> vkColorAttachmentFormats;

    void prepareNonAttachmentAccesses(const RenderPassSetup& setup);

    // Fills out vkRenderingAttachments and attachmentsToResolve, then prepares VkRenderingInfo that points
    // to entries in vkRenderingAttachments.
    void prepareRendering(const RenderPassSetup& setup, bool useSecondaryCmdBuffers);
    // Prepares inheritance for secondary command buffer recording using the prepared rendering info
    void prepareInheritance(const RenderPassSetup& setup);
};

}
