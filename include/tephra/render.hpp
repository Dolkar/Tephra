#pragma once

#include <tephra/command_list.hpp>
#include <tephra/descriptor.hpp>
#include <tephra/common.hpp>
#include <functional>

namespace tp {

/// Describes a kind of resource access from the render pipeline.
/// @see tp::DescriptorType for classification of descriptors into Storage, Sampled and Uniform.
/// @see tp::BufferRenderAccess
/// @see tp::ImageRenderAccess
enum class RenderAccess : uint64_t {
    /// Read access of indirect command data through an indirect drawing command.
    DrawIndirectRead = 1 << 0,
    /// Read access of an index buffer through an indexed drawing command.
    DrawIndexRead = 1 << 1,
    /// Read access of a vertex buffer through a drawing command.
    DrawVertexRead = 1 << 2,

    /// Vertex shader read access through storage descriptors.
    VertexShaderStorageRead = 1 << 3,
    /// Vertex shader write access through storage descriptors.
    VertexShaderStorageWrite = 1 << 4,
    /// Vertex shader atomic write access through storage descriptors.
    VertexShaderStorageAtomic = 1 << 5,
    /// Vertex shader read access through sampled descriptors.
    VertexShaderSampledRead = 1 << 6,
    /// Vertex shader read access through uniform buffer descriptors.
    VertexShaderUniformRead = 1 << 7,

    /// Tessellation control shader read access through storage descriptors.
    TessellationControlShaderStorageRead = 1 << 8,
    /// Tessellation control shader write access through storage descriptors.
    TessellationControlShaderStorageWrite = 1 << 9,
    /// Tessellation control shader atomic write access through storage descriptors.
    TessellationControlShaderStorageAtomic = 1 << 10,
    /// Tessellation control shader read access through sampled descriptors.
    TessellationControlShaderSampledRead = 1 << 11,
    /// Tessellation control shader read access through uniform buffer descriptors.
    TessellationControlShaderUniformRead = 1 << 12,

    /// Tessellation evaluation shader read access through storage descriptors.
    TessellationEvaluationShaderStorageRead = 1 << 13,
    /// Tessellation evaluation shader write access through storage descriptors.
    TessellationEvaluationShaderStorageWrite = 1 << 14,
    /// Tessellation evaluation shader atomic write access through storage descriptors.
    TessellationEvaluationShaderStorageAtomic = 1 << 15,
    /// Tessellation evaluation shader read access through sampled descriptors.
    TessellationEvaluationShaderSampledRead = 1 << 16,
    /// Tessellation evaluation shader read access through uniform buffer descriptors.
    TessellationEvaluationShaderUniformRead = 1 << 17,

    /// Geometry shader read access through storage descriptors.
    GeometryShaderStorageRead = 1 << 18,
    /// Geometry shader write access through storage descriptors.
    GeometryShaderStorageWrite = 1 << 19,
    /// Geometry shader atomic write access through storage descriptors.
    GeometryShaderStorageAtomic = 1 << 20,
    /// Geometry shader read access through sampled descriptors.
    GeometryShaderSampledRead = 1 << 21,
    /// Geometry shader read access through uniform buffer descriptors.
    GeometryShaderUniformRead = 1 << 22,

    /// Fragment shader read access through storage descriptors.
    FragmentShaderStorageRead = 1 << 23,
    /// Fragment shader write access through storage descriptors.
    FragmentShaderStorageWrite = 1 << 24,
    /// Fragment shader atomic write access through storage descriptors.
    FragmentShaderStorageAtomic = 1 << 25,
    /// Fragment shader read access through sampled descriptors.
    FragmentShaderSampledRead = 1 << 26,
    /// Fragment shader read access through uniform buffer descriptors.
    FragmentShaderUniformRead = 1 << 27
};
TEPHRA_MAKE_ENUM_BIT_MASK(RenderAccessMask, RenderAccess);

class CommandPool;
class VulkanCommandInterface;

/// Provides an interface to directly record graphics commands into a Vulkan @vksymbol{VkCommandBuffer}
/// inside a compute pass.
///
/// The behavior and expected usage differs depending on the variant of the `commandRecording` parameter
/// passed to tp::Job::cmdExecuteRenderPass.
///
/// If the list was provided through the tp::ArrayView<tp::RenderList> variant, then
/// tp::RenderList::beginRecording must be called before the first and tp::RenderList::endRecording after
/// the last recorded command.
///
/// If the list was provided as a parameter to tp::RenderInlineCallback using the function callback
/// variant, tp::RenderList::beginRecording and tp::RenderList::endRecording must not be called.
/// Any changed state (cmdBind..., cmdSet...) persists between all inline lists within the same tp::Job.
///
/// @see tp::Job::cmdExecuteRenderPass
/// @see @vksymbol{VkCommandBuffer}
class RenderList : public CommandList {
public:
    /// Constructs a null tp::RenderList.
    RenderList() : CommandList(){};

    /// Begins recording commands to the list, using the given command pool.
    /// @param commandPool
    ///     The command pool to use for memory allocations for command recording. Cannot be nullptr.
    /// @remarks
    ///     The parent tp::Job must be in an enqueued state.
    /// @remarks
    ///     The tp::RenderList must not have been received as a parameter to tp::RenderInlineCallback.
    /// @remarks
    ///     The tp::CommandPool is not thread safe. Only one thread may be recording commands
    ///     using the same pool at a time.
    /// @see @vksymbol{vkBeginCommandBuffer}
    void beginRecording(CommandPool* commandPool);

    /// Ends recording commands to the list. No other methods can be called after this point.
    /// @remarks
    ///     The parent tp::Job must be in an enqueued state.
    /// @remarks
    ///     The tp::RenderList must not have been received as a parameter to tp::RenderInlineCallback.
    /// @see @vksymbol{vkEndCommandBuffer}
    void endRecording();

    /// Binds a graphics tp::Pipeline for use in subsequent draw commands.
    /// @param pipeline
    ///     The pipeline object to bind.
    /// @remarks
    ///     If the pipeline was created with a tp::PipelineLayout whose descriptor set
    ///     layouts are compatible with the pipeline layout of sets previously bound with
    ///     tp::RenderList::cmdBindDescriptorSets, then the descriptor sets are not disturbed
    ///     and may still be accessed, up to the first incompatible set number.
    /// @see @vksymbol{vkCmdBindPipeline}
    void cmdBindGraphicsPipeline(const Pipeline& pipeline);

    /// Binds an index buffer for use in subsequent indexed draw commands.
    /// @param buffer
    ///     The index buffer to bind.
    /// @param indexType
    ///     The type of the index data inside the buffer.
    /// @see @vksymbol{vkCmdBindIndexBuffer}
    void cmdBindIndexBuffer(const BufferView& buffer, IndexType indexType);

    /// Binds the given vertex buffers for use in subsequent draw commands.
    /// @param buffers
    ///     The vertex buffers to bind.
    /// @param firstBinding
    ///     The index of the first vertex buffer binding to update.
    /// @remarks
    ///     Each buffer corresponds to a binding defined with tp::VertexInputBinding for the currently bound pipeline,
    ///     starting with `firstBinding`.
    /// @see @vksymbol{vkCmdBindVertexBuffers}
    void cmdBindVertexBuffers(ArrayParameter<const BufferView> buffers, uint32_t firstBinding = 0);

    /// Sets the viewport(s) to render to.
    /// @param viewports
    ///     The viewports to set.
    /// @param firstViewport
    ///     The index of the first viewport to update.
    /// @remarks
    ///     The number of viewports must match the count set for the currently bound pipeline.
    /// @see @vksymbol{vkCmdSetViewport}
    void cmdSetViewport(ArrayParameter<const Viewport> viewports, uint32_t firstViewport = 0);

    /// Sets the scissor test rectangles.
    /// @param scissors
    ///     The scissors to set.
    /// @param firstScissor
    ///     The index of the first scissor rectangle to update.
    /// @remarks
    ///     Each scissor rectangle corresponds to an active viewport. The number of scissors must match the count set
    ///     for the currently bound pipeline.
    /// @see @vksymbol{vkCmdSetScissor}
    void cmdSetScissor(ArrayParameter<const Rect2D> scissors, uint32_t firstScissor = 0);

    /// Records a non-indexed draw.
    /// @param vertexCount
    ///     The number of vertices to draw.
    /// @param instanceCount
    ///     The number of instances to draw.
    /// @param firstVertex
    ///     The index of the first vertex to draw.
    /// @param firstInstance
    ///     The index of the first instance to draw.
    /// @see @vksymbol{vkCmdDraw}
    void cmdDraw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

    /// Records an indexed draw.
    /// @param indexCount
    ///     The number of indexed vertices to draw.
    /// @param instanceCount
    ///     The number of instances to draw.
    /// @param firstIndex
    ///     The index of the first value in the index buffer to draw.
    /// @param vertexOffset
    ///     The value added to the vertex index before indexing into the vertex buffer.
    /// @param firstInstance
    ///     The index of the first instance to draw.
    void cmdDrawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount = 1,
        uint32_t firstIndex = 0,
        int32_t vertexOffset = 0,
        uint32_t firstInstance = 0);

    /// Records indirect draws with the parameters sourced from a buffer.
    /// @param drawParamBuffer
    ///     The buffer containing draw parameters in the form of @vksymbol{VkDrawIndirectCommand}.
    /// @param drawCount
    ///     The number of draws to execute.
    /// @param stride
    ///     The stride in bytes between successive sets of draw parameters.
    /// @see @vksymbol{vkCmdDrawIndirect}
    void cmdDrawIndirect(
        const BufferView& drawParamBuffer,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand));

    /// Records indirect indexed draws with the parameters sourced from a buffer.
    /// @param drawParamBuffer
    ///     The buffer containing draw parameters in the form of @vksymbol{VkDrawIndexedIndirectCommand}.
    /// @param drawCount
    ///     The number of draws to execute.
    /// @param stride
    ///     The stride in bytes between successive sets of draw parameters.
    /// @see @vksymbol{vkCmdDrawIndexedIndirect}
    void cmdDrawIndexedIndirect(
        const BufferView& drawParamBuffer,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand));

    /// Records indirect draws, with both the parameters and the draw count sourced from buffers.
    /// @param drawParamBuffer
    ///     The buffer containing draw parameters in the form of @vksymbol{VkDrawIndirectCommand}.
    /// @param countBuffer
    ///     The buffer containing the draw count in the form of a single unsigned 32-bit integer.
    /// @param maxDrawCount
    ///     The maximum number of draws that can be executed.
    /// @param stride
    ///     The stride in bytes between successive sets of draw parameters.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceVulkan12Features}::`drawIndirectCount` feature must be enabled.
    /// @see @vksymbol{vkCmdDrawIndirectCount}
    void cmdDrawIndirectCount(
        const BufferView& drawParamBuffer,
        const BufferView& countBuffer,
        uint32_t maxDrawCount,
        uint32_t stride);

    /// Records indirect indexed draws, with both the parameters and the draw count sourced from buffers.
    /// @param drawParamBuffer
    ///     The buffer containing draw parameters in the form of @vksymbol{VkDrawIndexedIndirectCommand}.
    /// @param countBuffer
    ///     The buffer containing the draw count in the form of a single unsigned 32-bit integer.
    /// @param maxDrawCount
    ///     The maximum number of draws that can be executed.
    /// @param stride
    ///     The stride in bytes between successive sets of draw parameters.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceVulkan12Features}::`drawIndirectCount` feature must be enabled.
    /// @see @vksymbol{vkCmdDrawIndexedIndirectCount}
    void cmdDrawIndexedIndirectCount(
        const BufferView& drawParamBuffer,
        const BufferView& countBuffer,
        uint32_t maxDrawCount,
        uint32_t stride);

    /// Sets the line width pipeline dynamic state.
    /// @param width
    ///     The width of the rasterized line segments in pixels.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`wideLines` feature must be enabled for `width != 1.0`.
    /// @remarks
    ///     Only has an effect on bound pipelines with the tp::DynamicState::LineWidth dynamic state enabled.
    /// @see @vksymbol{vkCmdSetLineWidth}
    void cmdSetLineWidth(float width = 1.0f);

    /// Sets the depth bias pipeline dynamic state.
    /// @param constantFactor
    ///     Controls the constant depth value added to each fragment.
    /// @param slopeFactor
    ///     Controls the slope dependent depth value added to each fragment.
    /// @param biasClamp
    ///     Sets the maximum depth bias of a fragment.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`depthBiasClamp` feature must be enabled for `biasClamp != 0.0f`.
    /// @remarks
    ///     Only has an effect on bound pipelines with the tp::DynamicState::DepthBias dynamic state enabled.
    /// @see @vksymbol{vkCmdSetDepthBias}
    void cmdSetDepthBias(float constantFactor = 0.0f, float slopeFactor = 0.0f, float biasClamp = 0.0f);

    /// Sets the blend constants pipeline dynamic state.
    /// @param blendConstants
    ///     The constants used for certain blend factors during blending operations.
    /// @remarks
    ///     Only has an effect on bound pipelines with the tp::DynamicState::BlendConstants dynamic state enabled.
    /// @see @vksymbol{vkCmdSetBlendConstants}
    void cmdSetBlendConstants(float blendConstants[4]);

    /// Sets the depth bounds pipeline dynamic state.
    /// @param minDepthBounds
    ///     The minimum depth value.
    /// @param maxDepthBounds
    ///     The maximum depth value.
    /// @remarks
    ///     The values must be between 0.0 and 1.0 inclusive.
    /// @remarks
    ///     Only has an effect on bound pipelines with the tp::DynamicState::DepthBounds dynamic state enabled.
    /// @see @vksymbol{vkCmdSetDepthBounds}
    void cmdSetDepthBounds(float minDepthBounds = 0.0f, float maxDepthBounds = 1.0f);

    TEPHRA_MAKE_NONCOPYABLE(RenderList);
    TEPHRA_MAKE_MOVABLE(RenderList);
    virtual ~RenderList();

private:
    friend class Job;
    friend class RenderPass;

    VkRenderingInfo vkRenderingInfo = {};

    RenderList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkInlineCommandBuffer,
        DebugTarget debugTarget);

    RenderList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle* vkFutureCommandBuffer,
        const VkRenderingInfo& vkRenderingInfo,
        DebugTarget debugTarget);
};

/// Represents an tp::ImageView as a render pass color attachment, allowing it to be bound as a render target inside of
/// a render pass. Also specifies any operations on it that are to be done at the beginning or end of the render pass.
/// @see tp::RenderPassSetup
struct ColorAttachment {
    ImageView image;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
    ClearValue clearValue;
    ImageView resolveImage;
    ResolveMode resolveMode;

    /// Describes an empty attachment
    ColorAttachment() : ColorAttachment({}, AttachmentLoadOp::DontCare, AttachmentStoreOp::DontCare) {}

    /// @param image
    ///     The image view used as an attachment.
    /// @param loadOp
    ///     The load operation done at the start of the render pass.
    /// @param storeOp
    ///     The store operation done at the end of the render pass.
    /// @param clearValue
    ///     If load operation is tp::AttachmentLoadOp::Clear, specifies the clear value.
    /// @param resolveImage
    ///     The image view that resolved multisample data will be written to at the end of the render pass.
    /// @param resolveMode
    ///     The resolve mode that will be used if `resolveImage` is defined.
    ColorAttachment(
        ImageView image,
        AttachmentLoadOp loadOp,
        AttachmentStoreOp storeOp,
        ClearValue clearValue = {},
        ImageView resolveImage = {},
        ResolveMode resolveMode = ResolveMode::Average)
        : image(std::move(image)),
          loadOp(loadOp),
          storeOp(storeOp),
          clearValue(clearValue),
          resolveImage(std::move(resolveImage)),
          resolveMode(resolveMode) {}
};

/// Represents an tp::ImageView as a render pass depth & stencil attachment, allowing it to be bound inside of a render
/// pass. Also specifies any operations that are to be done on it at the beginning or end of the render pass.
/// @see tp::RenderPassSetup
struct DepthStencilAttachment {
    ImageView image;
    bool depthReadOnly;
    AttachmentLoadOp depthLoadOp;
    AttachmentStoreOp depthStoreOp;
    bool stencilReadOnly;
    AttachmentLoadOp stencilLoadOp;
    AttachmentStoreOp stencilStoreOp;
    ClearValue clearValue;
    ImageView resolveImage;
    ResolveMode resolveMode;

    /// Describes an empty attachment
    DepthStencilAttachment()
        : DepthStencilAttachment({}, false, AttachmentLoadOp::DontCare, AttachmentStoreOp::DontCare) {}

    /// @param image
    ///     The image view used as an attachment.
    /// @param readOnly
    ///     If `true`, specifies that the attachment will not be written to inside the render pass.
    /// @param loadOp
    ///     The load operation done at the start of the render pass.
    /// @param storeOp
    ///     The store operation done at the end of the render pass.
    /// @param clearValue
    ///     If load operation is tp::AttachmentLoadOp::Clear, specifies the clear value.
    /// @param resolveImage
    ///     The image view that resolved multisample data will be written to at the end of the render pass.
    /// @param resolveMode
    ///     The resolve mode that will be used if `resolveImage` is defined.
    DepthStencilAttachment(
        ImageView image,
        bool readOnly,
        AttachmentLoadOp loadOp,
        AttachmentStoreOp storeOp,
        ClearValue clearValue = {},
        ImageView resolveImage = {},
        ResolveMode resolveMode = ResolveMode::Average)
        : DepthStencilAttachment(
              std::move(image),
              readOnly,
              loadOp,
              storeOp,
              readOnly,
              loadOp,
              storeOp,
              clearValue,
              std::move(resolveImage),
              resolveMode) {}

    /// @param image
    ///     The image view used as an attachment.
    /// @param depthReadOnly
    ///     If `true`, specifies that the depth aspect will not be written to inside the render pass.
    /// @param depthLoadOp
    ///     The load operation for the depth aspect done at the start of the render pass.
    /// @param depthStoreOp
    ///     The store operation for the depth aspect done at the end of the render pass.
    /// @param stencilReadOnly
    ///     If `true`, specifies that the stenncil aspect will not be written to inside the render pass.
    /// @param stencilLoadOp
    ///     The load operation for the stencil aspect done at the start of the render pass.
    /// @param stencilStoreOp
    ///     The store operation for the stencil aspect done at the end of the render pass.
    /// @param clearValue
    ///     If load operation is tp::AttachmentLoadOp::Clear, specifies the clear value.
    /// @param resolveImage
    ///     The image view that resolved multisample data will be written to at the end of the render pass.
    /// @param resolveMode
    ///     The resolve mode that will be used if `resolveImage` is defined.
    DepthStencilAttachment(
        ImageView image,
        bool depthReadOnly,
        AttachmentLoadOp depthLoadOp,
        AttachmentStoreOp depthStoreOp,
        bool stencilReadOnly,
        AttachmentLoadOp stencilLoadOp,
        AttachmentStoreOp stencilStoreOp,
        ClearValue clearValue = {},
        ImageView resolveImage = {},
        ResolveMode resolveMode = ResolveMode::Average)
        : image(std::move(image)),
          depthReadOnly(depthReadOnly),
          depthLoadOp(depthLoadOp),
          depthStoreOp(depthStoreOp),
          stencilReadOnly(stencilReadOnly),
          stencilLoadOp(stencilLoadOp),
          stencilStoreOp(stencilStoreOp),
          clearValue(clearValue),
          resolveImage(std::move(resolveImage)),
          resolveMode(resolveMode) {}
};

/// Represents an access to a range of tp::BufferView from a graphics pipeline.
/// @see tp::RenderPassSetup
struct BufferRenderAccess {
    BufferView buffer;
    RenderAccessMask accessMask;

    /// @param buffer
    ///     The buffer view being accessed.
    /// @param accessMask
    ///     The type of accesses that are made.
    BufferRenderAccess(BufferView buffer, RenderAccessMask accessMask)
        : buffer(std::move(buffer)), accessMask(accessMask) {}
};

/// Represents an access to a range of tp::ImageView from a graphics pipeline.
/// @see tp::RenderPassSetup
struct ImageRenderAccess {
    ImageView image;
    ImageSubresourceRange range;
    RenderAccessMask accessMask;

    /// @param image
    ///     The image view being accessed.
    /// @param accessMask
    ///     The type of accesses that are made.
    ImageRenderAccess(ImageView image, RenderAccessMask accessMask)
        : ImageRenderAccess(std::move(image), image.getWholeRange(), accessMask) {}

    /// @param image
    ///     The image view being accessed.
    /// @param range
    ///     The accessed range of the image view.
    /// @param accessMask
    ///     The type of accesses that are made.
    ImageRenderAccess(ImageView image, ImageSubresourceRange range, RenderAccessMask accessMask)
        : image(std::move(image)), range(range), accessMask(accessMask) {}
};

/// Used as configuration for executing a render pass.
/// @see tp::Job::cmdExecuteRenderPass
/// @see @vksymbol{VkRenderingInfo}
struct RenderPassSetup {
    DepthStencilAttachment depthStencilAttachment;
    ArrayView<const ColorAttachment> colorAttachments;
    ArrayView<const BufferRenderAccess> bufferAccesses;
    ArrayView<const ImageRenderAccess> imageAccesses;
    Rect2D renderArea;
    uint32_t layerCount;
    uint32_t viewMask;

    /// Constructs the tp::RenderPassSetup with a default render area that covers the minimum of all attachment sizes.
    /// @param depthStencilAttachment
    ///     The attachment for depth and / or stencil image.
    /// @param colorAttachments
    ///     The color attachments for rendered output.
    /// @param bufferAccesses
    ///     The buffer accesses that will be made within the render pass.
    /// @param imageAccesses
    ///     The additional non-attachment image accesses that will be made within the render pass.
    /// @param layerCount
    ///     The number of layers that may be rendered to when `viewMask` is 0.
    /// @param viewMask
    ///     The indices of attachment layers that will be rendered into when it is not 0.
    /// @remarks
    ///     There must be no overlap between image views in `depthStencilAttachment`, `colorAttachments` and
    ///     `imageAccesses`.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceVulkan11Features}::`multiview` feature must be enabled for `viewMask != 0`.
    RenderPassSetup(
        DepthStencilAttachment depthStencilAttachment,
        ArrayView<const ColorAttachment> colorAttachments,
        ArrayView<const BufferRenderAccess> bufferAccesses,
        ArrayView<const ImageRenderAccess> imageAccesses,
        uint32_t layerCount = 1,
        uint32_t viewMask = 0);

    /// @param depthStencilAttachment
    ///     The attachment for depth and / or stencil image.
    /// @param colorAttachments
    ///     The color attachments for rendered output.
    /// @param bufferAccesses
    ///     The buffer accesses that will be made within the render pass.
    /// @param imageAccesses
    ///     The additional non-attachment image accesses that will be made within the render pass.
    /// @param renderArea
    ///     The image area that may be rendered to, applied to all layers.
    /// @param layerCount
    ///     The number of layers that may be rendered to when `viewMask` is 0.
    /// @param viewMask
    ///     The indices of attachment layers that will be rendered into when it is not 0.
    /// @remarks
    ///     There must be no overlap between image views in `depthStencilAttachment`, `colorAttachments` and
    ///     `imageAccesses`.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceVulkan11Features}::`multiview` feature must be enabled for `viewMask != 0`.
    RenderPassSetup(
        DepthStencilAttachment depthStencilAttachment,
        ArrayView<const ColorAttachment> colorAttachments,
        ArrayView<const BufferRenderAccess> bufferAccesses,
        ArrayView<const ImageRenderAccess> imageAccesses,
        Rect2D renderArea,
        uint32_t layerCount = 1,
        uint32_t viewMask = 0)
        : depthStencilAttachment(std::move(depthStencilAttachment)),
          colorAttachments(colorAttachments),
          bufferAccesses(bufferAccesses),
          imageAccesses(imageAccesses),
          renderArea(renderArea),
          layerCount(layerCount),
          viewMask(viewMask) {}
};

/// The type of the user-provided function callback for recording commands to a render pass inline.
/// @see tp::Job::cmdExecuteRenderPass
using RenderInlineCallback = std::function<void(RenderList&)>;

}
