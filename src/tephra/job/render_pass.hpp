#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <tephra/render.hpp>
#include <deque>

namespace tp {

class PrimaryBufferRecorder;

struct StoredBufferRenderAccess {
    StoredBufferView buffer;
    RenderAccessMask accessMask;

    StoredBufferRenderAccess(const BufferRenderAccess& access) : buffer(access.buffer), accessMask(access.accessMask) {}
};

struct StoredImageRenderAccess {
    StoredImageView image;
    ImageSubresourceRange range;
    RenderAccessMask accessMask;

    StoredImageRenderAccess(const ImageRenderAccess& access)
        : image(access.image), range(access.range), accessMask(access.accessMask) {}
};

// Represents access of a render attachment and stores unresolved image view
struct AttachmentAccess {
    StoredImageView imageView;
    VkImageLayout layout;

    void convertToVkAccess(ImageAccessRange* rangePtr, ResourceAccess* accessPtr, VkImageLayout* layoutPtr);
};

class RenderPass {
public:
    explicit RenderPass(DeviceContainer* deviceImpl)
        : deviceImpl(deviceImpl), inlineListDebugTarget(DebugTarget::makeSilent()) {}

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    ArrayView<StoredBufferRenderAccess> getBufferAccesses() {
        return view(bufferAccesses);
    }

    ArrayView<StoredImageRenderAccess> getImageAccesses() {
        return view(imageAccesses);
    }

    ArrayView<AttachmentAccess> getAttachmentAccesses() {
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

    std::vector<StoredBufferRenderAccess> bufferAccesses;
    std::vector<StoredImageRenderAccess> imageAccesses;
    // Every two entries here correspond to one entry in vkRenderingAttachments (imageView and resolveImageView)
    // Entries can be null
    std::vector<AttachmentAccess> attachmentAccesses;

    bool isInline = false;
    RenderInlineCallback inlineRecordingCallback;
    DebugTarget inlineListDebugTarget;
    VkRenderingInfo vkRenderingInfo = {};
    VkRenderingInfoExtMap vkRenderingInfoExtMap;
    // We need to pass this structure also to vkInheritanceInfo, but not as part of the whole vkRenderingInfo chain
    VkMultiviewPerViewAttributesInfoNVX vkMultiviewInfoExt = {};
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
