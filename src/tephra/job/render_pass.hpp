#pragma once

#include "accesses.hpp"
#include "../common_impl.hpp"
#include <tephra/render.hpp>
#include <deque>

namespace tp {

struct AttachmentAccess {
    // Will be null in templates
    ImageView image;
    ResourceAccess firstAccess;
    VkImageLayout firstLayout;
    ResourceAccess lastAccess;
    VkImageLayout lastLayout;

    // Returns true if last access or layout are different from the first, requiring special handling to synchronize
    bool isSplitAccess() const;
};

struct RenderPassTemplate {
    RenderPassTemplate(
        DeviceContainer* deviceImpl,
        ArrayParameter<const AttachmentDescription> attachmentDescriptions,
        ArrayParameter<const SubpassLayout> subpassLayouts);

    DeviceContainer* deviceImpl;

    std::vector<VkAttachmentDescription> attachmentInfos;
    std::vector<VkSubpassDescription> subpassInfos;
    std::vector<VkSubpassDependency> dependencyInfos;

    std::vector<VkAttachmentReference> attachmentReferences;
    std::vector<uint32_t> preserveAttachmentReferences;

    std::vector<AttachmentAccess> attachmentAccesses;
    std::vector<MultisampleLevel> subpassDefaultMultisampleLevels;
};

class PrimaryBufferRecorder;

class RenderPass {
public:
    enum class ExecutionMethod {
        // The general method with no applicable optimizations. Will record into an existing primary command buffer,
        // issuing either inline callbacks or executing pre-recorded secondary command buffers.
        General,
        // If there is a single subpass with a single pre-recorded command buffer, we can directly execute it as
        // a primary command buffer that itself begins and ends the render pass.
        PrerecordedRenderPass
        // TODO: VK_KHR_dynamic_rendering
    };

    explicit RenderPass(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {}

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

    void assignSetup(const RenderPassSetup& setup, const char* debugName);

    std::vector<VkCommandBufferHandle>& assignDeferredSubpass(uint32_t subpassIndex, std::size_t bufferCount);

    void assignInlineSubpass(
        uint32_t subpassIndex,
        RenderInlineCallback recordingCallback,
        DebugTarget renderListDebugTarget);

    // Resolve the attachment images and create the framebuffer.
    void createFramebuffer();

    // Returns the current execution method based on the assigned subpasses
    ExecutionMethod getExecutionMethod() const;

    // Records the render pass for execution
    void recordPass(PrimaryBufferRecorder& recorder);

    void recordBeginRenderPass(VkCommandBufferHandle vkCommandBuffer) const;

    void recordNextSubpass(VkCommandBufferHandle vkCommandBuffer, uint32_t subpassIndex) const;

    VkRenderPassHandle vkGetRenderPassHandle() const {
        return renderPassHandle.vkGetHandle();
    }

    static const RenderPassTemplate* getRenderPassTemplate(const RenderPassLayout& layout) {
        return layout.renderPassTemplate.get();
    }

    TEPHRA_MAKE_NONCOPYABLE(RenderPass);
    TEPHRA_MAKE_NONMOVABLE(RenderPass);
    ~RenderPass() = default;

private:
    friend class RenderList;

    struct Subpass {
        bool isInline = false;
        RenderInlineCallback inlineRecordingCallback;
        DebugTarget inlineListDebugTarget = DebugTarget::makeSilent();
        std::vector<VkCommandBufferHandle> vkPreparedCommandBuffers;

        std::vector<VkCommandBufferHandle>& assignDeferred(std::size_t bufferCount);

        void assignInline(RenderInlineCallback recordingCallback, DebugTarget renderListDebugTarget);
    };

    DeviceContainer* deviceImpl = nullptr;

    Lifeguard<VkRenderPassHandle> renderPassHandle;
    Lifeguard<VkFramebufferHandle> framebufferHandle;

    std::vector<BufferRenderAccess> bufferAccesses;
    std::vector<ImageRenderAccess> imageAccesses;
    std::vector<AttachmentAccess> attachmentAccesses;

    std::vector<ClearValue> attachmentClearValues;
    Rect2D renderArea;
    uint32_t layerCount = 0;

    std::vector<Subpass> subpasses;
};

}
