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
/// @see tp::SubpassDependency
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

/// Describes how a bound attachment will be interpreted.
enum class AttachmentBindPointType {
    /// A color attachment for storing rasterization outputs.
    Color,
    /// An input attachment for loading pixel-local data from previous subpasses.
    Input,
    /// A depth and/or stencil attachment used during rasterization.
    DepthStencil,
    /// An attachment that will be a target of a multisample resolve operation from a Color attachment.
    ResolveFromColor
};

/// Identifies a bind point for an attachment and determines how the graphics pipeline will access the attachment.
/// @see tp::AttachmentBinding
struct AttachmentBindPoint {
    AttachmentBindPointType type;
    uint32_t number;
    bool isReadOnly;

    /// A generic constructor. Consider using the typed factory methods instead.
    /// @param type
    ///     The attachment bind point type.
    /// @param number
    ///     The bind point number. Must be 0 for a depth / stencil attachment.
    /// @param isReadOnly
    ///     Specifies whether the attachment is read only.
    AttachmentBindPoint(AttachmentBindPointType type, uint32_t number, bool isReadOnly)
        : type(type), number(number), isReadOnly(isReadOnly) {}

    /// Returns a color attachment bind point.
    /// @param number
    ///     The bind point number.
    static AttachmentBindPoint Color(uint32_t number) {
        return AttachmentBindPoint(AttachmentBindPointType::Color, number, false);
    }

    /// Returns an input attachment bind point.
    /// @param number
    ///     The bind point number.
    static AttachmentBindPoint Input(uint32_t number) {
        return AttachmentBindPoint(AttachmentBindPointType::Input, number, true);
    }

    /// Returns a depth / stencil attachment bind point.
    /// @param isReadOnly
    ///     Specifies whether the attachment is read only.
    static AttachmentBindPoint DepthStencil(bool isReadOnly = false) {
        return AttachmentBindPoint(AttachmentBindPointType::DepthStencil, 0, isReadOnly);
    }

    /// Returns a color resolve target attachment bind point.
    /// @param number
    ///     The bind point number.
    static AttachmentBindPoint ResolveFromColor(uint32_t number) {
        return AttachmentBindPoint(AttachmentBindPointType::ResolveFromColor, number, false);
    }
};

/// Serves to bind an attachment to the given bind point for a subpass.
/// @see tp::SubpassLayout
struct AttachmentBinding {
    AttachmentBindPoint bindPoint;
    uint32_t attachmentIndex;

    /// @param bindPoint
    ///     The tp::AttachmentBindPoint to bind to.
    /// @param attachmentIndex
    ///     The index of the attachment to be bound, referencing the `attachmentDescriptions` array
    ///     as passed in tp::Device::createRenderPassLayout.
    AttachmentBinding(AttachmentBindPoint bindPoint, uint32_t attachmentIndex)
        : bindPoint(std::move(bindPoint)), attachmentIndex(attachmentIndex) {}
};

/// Specifies a dependency on another subpass from the current subpass.
/// @remarks
///     Dependencies on attachment resources may only be pixel-local. Sampling an output attachment as a texture in
///     a following subpass is not allowed. A separate render pass must be used instead.
/// @see tp::SubpassLayout
/// @see @vksymbol{VkSubpassDependency}
struct SubpassDependency {
    uint32_t sourceSubpassIndex;
    RenderAccessMask additionalSourceAccessMask;
    RenderAccessMask additionalDestinationAccessMask;

    /// Specifies a subpass dependency on a given source subpass that depends only on its attachments.
    /// @param sourceSubpassIndex
    ///     Index of the source subpass.
    explicit SubpassDependency(uint32_t sourceSubpassIndex)
        : SubpassDependency(sourceSubpassIndex, RenderAccessMask::None(), RenderAccessMask::None()) {}

    /// Specifies a subpass dependency on a given source subpass with additional non-attachment dependencies.
    /// @param sourceSubpassIndex
    ///     Index of the source subpass.
    /// @param additionalSourceAccessMask
    ///     The additional, non-attachment source accesses.
    /// @param additionalDestinationAccessMask
    ///     The additional, non-attachment destination accesses.
    SubpassDependency(
        uint32_t sourceSubpassIndex,
        RenderAccessMask additionalSourceAccessMask,
        RenderAccessMask additionalDestinationAccessMask)
        : sourceSubpassIndex(sourceSubpassIndex),
          additionalSourceAccessMask(additionalSourceAccessMask),
          additionalDestinationAccessMask(additionalDestinationAccessMask) {}
};

/// Describes a single subpass of a render pass, the attachments bindings and the subpass' dependencies.
///
/// A subpass represents an execution point for graphics commands through the use of tp::Job::cmdExecuteRenderPass.
/// These commands share the same set of attachments and the same render area.
///
/// @see tp::RenderPassLayout
struct SubpassLayout {
    ArrayView<const AttachmentBinding> bindings;
    ArrayView<const SubpassDependency> dependencies;

    /// @param bindings
    ///     The attachments bindings for the subpass.
    /// @param dependencies
    ///     The dependencies on other subpasses within the same render pass.
    explicit SubpassLayout(ArrayView<const AttachmentBinding> bindings, ArrayView<const SubpassDependency> dependencies)
        : bindings(bindings), dependencies(dependencies) {}
};

/// Describes the format and sample count of images that can be used as attachments within the render pass.
/// @see tp::RenderPassLayout
/// @see @vksymbol{VkAttachmentDescription}
struct AttachmentDescription {
    Format format;
    MultisampleLevel sampleCount;

    /// @param format
    ///     The format of the attachment.
    /// @param sampleCount
    ///     The sample count of the attachment.
    AttachmentDescription(Format format, MultisampleLevel sampleCount = MultisampleLevel::x1)
        : format(format), sampleCount(sampleCount) {}
};

class RenderPass;
struct RenderPassTemplate;

/// Describes ahead of time the layout and characteritics of a render pass.
///
/// A render pass is a collection of one or more consecutive subpasses that share the same render area, allowing
/// execution of graphics commands.
///
/// @remarks
///     It is also needed for the compilation of graphics tp::Pipeline objects, allowing the implementation to
///     specialize the pipeline for render passes using this layout.
/// @remarks
///     The subpasses and the commands in them are allowed to be executed in a tiled fashion. This means that
///     dependencies between subpasses can only be pixel-local. Sampling an output attachment as a texture in a
///     following subpass is not allowed. A separate render pass must be used instead.
/// @see tp::Device::createRenderPassLayout
/// @see tp::RenderPassSetup
/// @see @vksymbol{VkRenderPass}
class RenderPassLayout {
public:
    RenderPassLayout();
    RenderPassLayout(
        Lifeguard<VkRenderPassHandle>&& templateRenderPassHandle,
        std::unique_ptr<RenderPassTemplate> renderPassTemplate);

    /// Returns `true` if the render pass layout is null and does not view any resource.
    bool isNull() const {
        return templateRenderPassHandle.isNull();
    }

    /// Returns the @vksymbol{VkRenderPass} handle that is used by this layout as a template for creating pipelines.
    VkRenderPassHandle vkGetTemplateRenderPassHandle() const {
        return templateRenderPassHandle.vkGetHandle();
    }

    TEPHRA_MAKE_NONCOPYABLE(RenderPassLayout);
    TEPHRA_MAKE_MOVABLE(RenderPassLayout);
    virtual ~RenderPassLayout();

private:
    friend class RenderPass;

    std::unique_ptr<RenderPassTemplate> renderPassTemplate;
    Lifeguard<VkRenderPassHandle> templateRenderPassHandle;
};

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
    ///     The pipeline must have been created with the same tp::RenderPassLayout and subpass index
    ///     as the render list is being executed in.
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

    const RenderPass* renderPass = nullptr;
    uint32_t subpassIndex = 0;

    RenderList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkInlineCommandBuffer,
        const RenderPass* renderPass,
        uint32_t subpassIndex,
        DebugTarget debugTarget);

    RenderList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle* vkFutureCommandBuffer,
        const RenderPass* renderPass,
        uint32_t subpassIndex,
        DebugTarget debugTarget);
};

/// Represents an tp::ImageView as a render pass attachment, allowing it to be bound as a render target inside of its
/// subpasses. Also specifies any operations that are to be done at the beginning or end of the render pass. Clearing an
/// image through an attachment load operation tends to be more efficient than with an explicit command.
/// @see tp::RenderPassSetup
struct RenderPassAttachment {
    ImageView image;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
    AttachmentLoadOp stencilLoadOp;
    AttachmentStoreOp stencilStoreOp;
    ClearValue clearValue;

    /// @param image
    ///     The image view used as an attachment.
    /// @param loadOp
    ///     The load operation done at the start of a render pass.
    /// @param storeOp
    ///     The store operation done at the end of a render pass.
    /// @param clearValue
    ///     If load operation is tp::AttachmentLoadOp::Clear, specifies the clear value.
    RenderPassAttachment(ImageView image, AttachmentLoadOp loadOp, AttachmentStoreOp storeOp, ClearValue clearValue = {})
        : RenderPassAttachment(std::move(image), loadOp, storeOp, loadOp, storeOp, clearValue) {}

    /// @param image
    ///     The image view used as an attachment.
    /// @param loadOp
    ///     The load operation for the color / depth aspect done at the start of a render pass.
    /// @param storeOp
    ///     The store operation for the color / depth aspect done at the end of a render pass.
    /// @param stencilLoadOp
    ///     The load operation for the stencil aspect done at the start of a render pass.
    /// @param stencilStoreOp
    ///     The store operation for the stencil aspect done at the end of a render pass.
    /// @param clearValue
    ///     If load operation is tp::AttachmentLoadOp::Clear, specifies the clear value.
    RenderPassAttachment(
        ImageView image,
        AttachmentLoadOp loadOp,
        AttachmentStoreOp storeOp,
        AttachmentLoadOp stencilLoadOp,
        AttachmentStoreOp stencilStoreOp,
        ClearValue clearValue = {})
        : image(std::move(image)),
          loadOp(loadOp),
          storeOp(storeOp),
          stencilLoadOp(stencilLoadOp),
          stencilStoreOp(stencilStoreOp),
          clearValue(clearValue) {}
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
/// @see @vksymbol{VkRenderPassCreateInfo}
/// @see @vksymbol{VkFramebufferCreateInfo}
struct RenderPassSetup {
    const RenderPassLayout* layout;
    ArrayView<const RenderPassAttachment> attachments;
    ArrayView<const BufferRenderAccess> bufferAccesses;
    ArrayView<const ImageRenderAccess> imageAccesses;
    Rect2D renderArea;
    uint32_t layerCount;

    /// Constructs the tp::RenderPassSetup with a default render area that covers the minimum of all attachment sizes.
    /// @param layout
    ///     The layout of this render pass.
    /// @param attachments
    ///     The list of attachments to be bound. The number of attachments as well as their format and sample count
    ///     must match that of the tp::AttachmentDescription array provided when creating tp::RenderPassLayout.
    /// @param bufferAccesses
    ///     The buffer accesses to be made within the render pass.
    /// @param imageAccesses
    ///     The additional non-attachment image accesses to be made within the render pass.
    /// @param layerCount
    ///     The number of layers that may be rendered to.
    /// @remarks
    ///     There must be no overlap between image views in `attachments` and `imageAccesses`.
    RenderPassSetup(
        const RenderPassLayout* layout,
        ArrayView<const RenderPassAttachment> attachments,
        ArrayView<const BufferRenderAccess> bufferAccesses,
        ArrayView<const ImageRenderAccess> imageAccesses,
        uint32_t layerCount = 1);

    /// @param layout
    ///     The layout of this render pass.
    /// @param attachments
    ///     The list of attachments to be bound. The number of attachments as well as their format and sample count
    ///     must match that of the tp::AttachmentDescription array provided when creating tp::RenderPassLayout.
    /// @param bufferAccesses
    ///     The buffer accesses to be made within the render pass.
    /// @param imageAccesses
    ///     The additional non-attachment image accesses to be made within the render pass.
    /// @param renderArea
    ///     The image area that may be rendered to, applied to all layers.
    /// @param layerCount
    ///     The number of layers that may be rendered to.
    /// @remarks
    ///     There must be no overlap between image views in `attachments` and `imageAccesses`.
    RenderPassSetup(
        const RenderPassLayout* layout,
        ArrayView<const RenderPassAttachment> attachments,
        ArrayView<const BufferRenderAccess> bufferAccesses,
        ArrayView<const ImageRenderAccess> imageAccesses,
        Rect2D renderArea,
        uint32_t layerCount = 1)
        : layout(layout),
          attachments(attachments),
          bufferAccesses(bufferAccesses),
          imageAccesses(imageAccesses),
          renderArea(renderArea),
          layerCount(layerCount) {}
};

/// The type of the user-provided function callback for recording commands to a render pass inline.
/// @see tp::Job::cmdExecuteRenderPass
using RenderInlineCallback = std::function<void(RenderList&)>;

}
