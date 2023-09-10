#include "vulkan/interface.hpp"
#include "device/device_container.hpp"
#include "job/render_pass.hpp"
#include "common_impl.hpp"
#include <tephra/render.hpp>
#include <tephra/pipeline.hpp>

namespace tp {

void RenderList::beginRecording(CommandPool* commandPool) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "beginRecording", nullptr);

    TEPHRA_ASSERT(vkCommandBufferHandle.isNull());
    TEPHRA_ASSERTD(vkFutureCommandBuffer != nullptr, "beginRecording() of inline RenderList");
    TEPHRA_ASSERTD(vkRenderingInfo.sType == VK_STRUCTURE_TYPE_RENDERING_INFO, "invalid vkRenderingInfo was provided");

    vkCommandBufferHandle = commandPool->acquirePrimaryCommandBuffer(debugTarget->getObjectName());

    // Record to a fresh primary command buffer
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    throwRetcodeErrors(vkiCommands->beginCommandBuffer(vkCommandBufferHandle, &beginInfo));

    // Start rendering using the info we were given
    vkiCommands->cmdBeginRendering(vkCommandBufferHandle, &vkRenderingInfo);
}

void RenderList::endRecording() {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "endRecording", nullptr);

    vkiCommands->cmdEndRendering(vkCommandBufferHandle);
    throwRetcodeErrors(vkiCommands->endCommandBuffer(vkCommandBufferHandle));

    // The command buffer is ready to be used now
    *vkFutureCommandBuffer = vkCommandBufferHandle;
}

void RenderList::cmdBindGraphicsPipeline(const Pipeline& pipeline) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBindGraphicsPipeline", nullptr);
    vkiCommands->cmdBindPipeline(
        vkCommandBufferHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkGetPipelineHandle());
}

void RenderList::cmdBindIndexBuffer(const BufferView& buffer, IndexType indexType) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBindIndexBuffer", nullptr);
    uint64_t viewOffset;
    VkBufferHandle vkBufferHandle = buffer.vkResolveBufferHandle(&viewOffset);
    vkiCommands->cmdBindIndexBuffer(
        vkCommandBufferHandle, vkBufferHandle, viewOffset, vkCastConvertibleEnum(indexType));
}

void RenderList::cmdBindVertexBuffers(ArrayParameter<const BufferView> buffers, uint32_t firstBinding) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBindVertexBuffers", nullptr);

    ScratchVector<VkBufferHandle> underlyingBuffers(buffers.size());
    ScratchVector<uint64_t> bufferOffsets(buffers.size());

    for (uint32_t i = 0; i < buffers.size(); i++) {
        underlyingBuffers[i] = buffers[i].vkResolveBufferHandle(&bufferOffsets[i]);
    }

    vkiCommands->cmdBindVertexBuffers(
        vkCommandBufferHandle,
        firstBinding,
        static_cast<uint32_t>(buffers.size()),
        vkCastTypedHandlePtr(underlyingBuffers.data()),
        bufferOffsets.data());
}

void RenderList::cmdSetViewport(ArrayParameter<const Viewport> viewports, uint32_t firstViewport) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetViewport", nullptr);
    vkiCommands->cmdSetViewport(
        vkCommandBufferHandle,
        firstViewport,
        static_cast<uint32_t>(viewports.size()),
        static_cast<const VkViewport*>(viewports.data()));
}

void RenderList::cmdSetScissor(ArrayParameter<const Rect2D> scissors, uint32_t firstScissor) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetScissor", nullptr);
    vkiCommands->cmdSetScissor(
        vkCommandBufferHandle,
        firstScissor,
        static_cast<uint32_t>(scissors.size()),
        static_cast<const VkRect2D*>(scissors.data()));
}

void RenderList::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDraw", nullptr);
    vkiCommands->cmdDraw(vkCommandBufferHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderList::cmdDrawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDrawIndexed", nullptr);
    vkiCommands->cmdDrawIndexed(
        vkCommandBufferHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RenderList::cmdDrawIndirect(const BufferView& drawParamBuffer, uint32_t drawCount, uint32_t stride) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDrawIndirect", nullptr);
    uint64_t viewOffset;
    VkBufferHandle vkBufferHandle = drawParamBuffer.vkResolveBufferHandle(&viewOffset);
    vkiCommands->cmdDrawIndirect(vkCommandBufferHandle, vkBufferHandle, viewOffset, drawCount, stride);
}

void RenderList::cmdDrawIndexedIndirect(const BufferView& drawParamBuffer, uint32_t drawCount, uint32_t stride) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDrawIndexedIndirect", nullptr);
    uint64_t viewOffset;
    VkBufferHandle vkBufferHandle = drawParamBuffer.vkResolveBufferHandle(&viewOffset);
    vkiCommands->cmdDrawIndexedIndirect(vkCommandBufferHandle, vkBufferHandle, viewOffset, drawCount, stride);
}

void RenderList::cmdDrawIndirectCount(
    const BufferView& drawParamBuffer,
    const BufferView& countBuffer,
    uint32_t maxDrawCount,
    uint32_t stride) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDrawIndirectCount", nullptr);
    uint64_t drawParamViewOffset;
    VkBufferHandle vkDrawParamBufferHandle = drawParamBuffer.vkResolveBufferHandle(&drawParamViewOffset);
    uint64_t countViewOffset;
    VkBufferHandle vkCountBufferHandle = countBuffer.vkResolveBufferHandle(&countViewOffset);
    vkiCommands->cmdDrawIndirectCount(
        vkCommandBufferHandle,
        vkDrawParamBufferHandle,
        drawParamViewOffset,
        vkCountBufferHandle,
        countViewOffset,
        maxDrawCount,
        stride);
}

void RenderList::cmdDrawIndexedIndirectCount(
    const BufferView& drawParamBuffer,
    const BufferView& countBuffer,
    uint32_t maxDrawCount,
    uint32_t stride) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDrawIndexedIndirectCount", nullptr);
    uint64_t drawParamViewOffset;
    VkBufferHandle vkDrawParamBufferHandle = drawParamBuffer.vkResolveBufferHandle(&drawParamViewOffset);
    uint64_t countViewOffset;
    VkBufferHandle vkCountBufferHandle = countBuffer.vkResolveBufferHandle(&countViewOffset);
    vkiCommands->cmdDrawIndexedIndirectCount(
        vkCommandBufferHandle,
        vkDrawParamBufferHandle,
        drawParamViewOffset,
        vkCountBufferHandle,
        countViewOffset,
        maxDrawCount,
        stride);
}

void RenderList::cmdSetLineWidth(float width) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetLineWidth", nullptr);
    vkiCommands->cmdSetLineWidth(vkCommandBufferHandle, width);
}

void RenderList::cmdSetDepthBias(float constantFactor, float slopeFactor, float biasClamp) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetDepthBias", nullptr);
    vkiCommands->cmdSetDepthBias(vkCommandBufferHandle, constantFactor, biasClamp, slopeFactor);
}

void RenderList::cmdSetBlendConstants(float blendConstants[4]) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetBlendConstants", nullptr);
    vkiCommands->cmdSetBlendConstants(vkCommandBufferHandle, blendConstants);
}

void RenderList::cmdSetDepthBounds(float minDepthBounds, float maxDepthBounds) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdSetDepthBounds", nullptr);
    vkiCommands->cmdSetDepthBounds(vkCommandBufferHandle, minDepthBounds, maxDepthBounds);
}

RenderList::RenderList(RenderList&&) noexcept = default;

RenderList& RenderList::operator=(RenderList&&) noexcept = default;

RenderList::~RenderList() = default;

RenderList::RenderList(
    const VulkanCommandInterface* vkiCommands,
    VkCommandBufferHandle vkInlineCommandBuffer,
    DebugTarget debugTarget)
    : CommandList(vkiCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, vkInlineCommandBuffer, std::move(debugTarget)),
      vkRenderingInfo() {}

RenderList::RenderList(
    const VulkanCommandInterface* vkiCommands,
    VkCommandBufferHandle* vkFutureCommandBuffer,
    VkRenderingInfo vkRenderingInfo,
    DebugTarget debugTarget)
    : CommandList(vkiCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, vkFutureCommandBuffer, std::move(debugTarget)),
      vkRenderingInfo(vkRenderingInfo) {}

RenderPassSetup::RenderPassSetup(
    RenderPassAttachment depthStencilAttachment,
    ArrayView<const RenderPassAttachment> colorAttachments,
    ArrayView<const BufferRenderAccess> bufferAccesses,
    ArrayView<const ImageRenderAccess> imageAccesses,
    uint32_t layerCount,
    uint32_t viewMask,
    void* vkRenderingInfoExtPtr)
    : depthStencilAttachment(std::move(depthStencilAttachment)),
      colorAttachments(colorAttachments),
      bufferAccesses(bufferAccesses),
      imageAccesses(imageAccesses),
      layerCount(layerCount),
      viewMask(viewMask),
      vkRenderingInfoExtPtr(vkRenderingInfoExtPtr) {
    // Set default render area
    Extent3D minExtent;
    if (!depthStencilAttachment.image.isNull())
        minExtent = depthStencilAttachment.image.getExtent();
    else if (!colorAttachments.empty())
        minExtent = colorAttachments[0].image.getExtent();

    if (!colorAttachments.empty()) {
        for (const RenderPassAttachment& attachment : viewRange(colorAttachments, 1, colorAttachments.size() - 1)) {
            Extent3D extent = attachment.image.getExtent();
            minExtent.width = min(minExtent.width, extent.width);
            minExtent.height = min(minExtent.height, extent.height);
        }
        renderArea.extent = Extent2D(minExtent.width, minExtent.height);
        renderArea.offset = Offset2D(0, 0);
    }
}

}
