#pragma once

#include <tephra/format.hpp>
#include <tephra/common.hpp>
#include <type_traits>

namespace tp {

/// The mode used for rasterization of primitives.
/// @see @vksymbol{VkPolygonMode}
enum class RasterizationMode {
    /// Turns rasterization off completely.
    Discard = 0,
    /// Rasterize the full polygons.
    Fill = 1,
    /// Rasterize the edges of polygons.
    Line = 2,
    /// Rasterize the vertices of polygons as points.
    Point = 3,
};

/// Describes the value of a particular specialization constant of any scalar boolean, integer or floating point type.
/// @see tp::ShaderStageSetup
/// @see @vksymbol{VkSpecializationMapEntry}
struct SpecializationConstant {
    uint32_t constantID;
    uint32_t constantSizeBytes;
    std::byte data[sizeof(uint64_t)];

    /// @param constantID
    ///     The ID of the constant as referenced in the shader.
    /// @param value
    ///     The assigned value of the matching type.
    template <typename T>
    SpecializationConstant(uint32_t constantID, T value) : constantID(constantID), constantSizeBytes(sizeof(T)) {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        static_assert(std::is_arithmetic<T>::value);
        memcpy(data, &value, constantSizeBytes);
    }
};

/// Identifies a specific vertex input attribute such as position, normal, etc.
/// @see tp::VertexInputBinding
/// @see @vksymbol{VkVertexInputAttributeDescription}
struct VertexInputAttribute {
    uint32_t location;
    Format format;
    uint32_t offset;

    /// @param location
    ///     The shader vertex input location number.
    /// @param format
    ///     The format of the attribute data.
    /// @param offset
    ///     The offset in bytes relative to the start of an element in the vertex input binding.
    VertexInputAttribute(uint32_t location, Format format, uint32_t offset)
        : location(location), format(format), offset(offset) {}
};

/// Describes the shader binding for vertex input buffer and the layout of its attributes.
/// @see tp::GraphicsPipelineSetup::setVertexInputBindings
/// @see @vksymbol{VkVertexInputBindingDescription}
struct VertexInputBinding {
    ArrayView<const VertexInputAttribute> attributes;
    uint32_t stride;
    VertexInputRate inputRate;

    /// @param attributes
    ///     The description of the individual vertex input attributes present in this binding.
    /// @param stride
    ///     The stride in bytes between consecutive elements in the buffer.
    /// @param inputRate
    ///     Whether the elements are addressed by the index of the vertex or instance.
    VertexInputBinding(
        ArrayView<const VertexInputAttribute> attributes,
        uint32_t stride,
        VertexInputRate inputRate = VertexInputRate::Vertex)
        : attributes(attributes), stride(stride), inputRate(inputRate) {}
};

/// Describes the blending operation with its factors for a single component of a render pass attachment.
/// @see tp::AttachmentBlendState
struct BlendState {
    BlendFactor srcBlendFactor;
    BlendFactor dstBlendFactor;
    BlendOp blendOp;

    /// Default constructor for no blending (passthrough).
    BlendState() : BlendState(BlendFactor::One, BlendFactor::Zero, BlendOp::Add) {}

    /// @param srcBlendFactor
    ///     The multiplicative factor applied to the source value.
    /// @param dstBlendFactor
    ///     The multiplicative factor applied to the destination value.
    /// @param blendOp
    ///     The blending operation used to combine the source and destination values after their factors get applied.
    BlendState(BlendFactor srcBlendFactor, BlendFactor dstBlendFactor, BlendOp blendOp = BlendOp::Add)
        : srcBlendFactor(srcBlendFactor), dstBlendFactor(dstBlendFactor), blendOp(blendOp) {}

    /// Returns a passthrough blend state for no blending.
    static BlendState NoBlend() {
        return BlendState();
    }
};

inline bool operator==(const BlendState& lhs, const BlendState& rhs) {
    return lhs.srcBlendFactor == rhs.srcBlendFactor && lhs.dstBlendFactor == rhs.dstBlendFactor &&
        lhs.blendOp == rhs.blendOp;
}

inline bool operator!=(const BlendState& lhs, const BlendState& rhs) {
    return !(lhs == rhs);
}

/// Specifies the output blending state of a render pass attachment.
/// @see @vksymbol{VkPipelineColorBlendAttachmentState}
struct AttachmentBlendState {
    BlendState colorBlend;
    BlendState alphaBlend;
    ColorComponentMask writeMask;

    /// Default constructor for no blending (passthrough).
    AttachmentBlendState() : AttachmentBlendState({}, {}) {}

    /// @param colorBlend
    ///     The blend state for the color components.
    /// @param alphaBlend
    ///     The blend state for the alpha component.
    /// @param writeMask
    ///     The optional mask of components to be written.
    AttachmentBlendState(
        BlendState colorBlend,
        BlendState alphaBlend,
        ColorComponentMask writeMask = ColorComponent::Red | ColorComponent::Green | ColorComponent::Blue |
            ColorComponent::Alpha)
        : colorBlend(std::move(colorBlend)), alphaBlend(std::move(alphaBlend)), writeMask(writeMask) {}

    /// Returns a passthrough blend state for no blending.
    static AttachmentBlendState NoBlend() {
        return AttachmentBlendState();
    }
};

inline bool operator==(const AttachmentBlendState& lhs, const AttachmentBlendState& rhs) {
    return lhs.colorBlend == rhs.colorBlend && lhs.alphaBlend == rhs.alphaBlend && lhs.writeMask == rhs.writeMask;
}

inline bool operator!=(const AttachmentBlendState& lhs, const AttachmentBlendState& rhs) {
    return !(lhs == rhs);
}

/// Represents a single shader module as loaded from SPIR-V bytecode.
/// @see tp::Device::createShaderModule
/// @see tp::ShaderStageSetup
/// @see @vksymbol{VkShaderModule}
class ShaderModule {
public:
    ShaderModule() = default;

    ShaderModule(Lifeguard<VkShaderModuleHandle> shaderModuleHandle)
        : shaderModuleHandle(std::move(shaderModuleHandle)) {}

    /// Returns `true` if the shader module is null and not valid for use.
    bool isNull() const {
        return shaderModuleHandle.isNull();
    }

    /// Returns the associated Vulkan @vksymbol{VkShaderModule} handle.
    VkShaderModuleHandle vkGetShaderModuleHandle() const {
        return shaderModuleHandle.vkGetHandle();
    }

private:
    Lifeguard<VkShaderModuleHandle> shaderModuleHandle;
};

/// Describes the layout of resources accessed by a compute or graphics pipeline.
/// @see tp::Device::createPipelineLayout
/// @see @vksymbol{VkPipelineLayout}
class PipelineLayout {
public:
    PipelineLayout() {}

    PipelineLayout(Lifeguard<VkPipelineLayoutHandle> pipelineLayoutHandle)
        : pipelineLayoutHandle(std::move(pipelineLayoutHandle)) {}

    /// Returns `true` if the pipeline layout is null and not valid for use.
    bool isNull() const {
        return pipelineLayoutHandle.isNull();
    }

    /// Returns the associated Vulkan @vksymbol{VkPipelineLayout} handle.
    VkPipelineLayoutHandle vkGetPipelineLayoutHandle() const {
        return pipelineLayoutHandle.vkGetHandle();
    }

private:
    Lifeguard<VkPipelineLayoutHandle> pipelineLayoutHandle;
};

/// Speeds up the compilation of pipelines by allowing the result of pipeline compilation to be reused
/// between pipelines and between application runs.
/// @remarks
///     Access to the tp::PipelineCache object is internally synchronized, meaning it is safe to operate on it from
///     multiple threads at the same time.
/// @see tp::Device::createPipelineCache
/// @see @vksymbol{VkPipelineCache}
class PipelineCache {
public:
    PipelineCache() {}

    PipelineCache(const Device* device, Lifeguard<VkPipelineCacheHandle> pipelineCacheHandle)
        : device(device), pipelineCacheHandle(std::move(pipelineCacheHandle)) {}

    /// Returns `true` if the pipeline cache is null and not valid for use.
    bool isNull() const {
        return pipelineCacheHandle.isNull();
    }

    /// Returns the size of the pipeline cache data in bytes.
    std::size_t getDataSize() const;

    /// Writes the cache data to the given array.
    /// @remarks
    ///     The size of the array must be at least as big as the number of bytes returned by
    ///     tp::PipelineCache::getDataSize.
    void getData(ArrayView<std::byte> data);

    /// Returns the associated Vulkan @vksymbol{VkPipelineCache} handle.
    VkPipelineCacheHandle vkGetPipelineCacheHandle() const {
        return pipelineCacheHandle.vkGetHandle();
    }

private:
    const Device* device = nullptr;
    Lifeguard<VkPipelineCacheHandle> pipelineCacheHandle;
};

/// Represents a full compiled state of a compute or graphics pipeline, composed of multiple shader
/// stages and the state of the configurable fixed-function stages.
/// @see tp::Device::compileComputePipelines
/// @see tp::Device::compileGraphicsPipelines
/// @see @vksymbol{VkPipeline}
class Pipeline {
public:
    Pipeline() {}

    Pipeline(Lifeguard<VkPipelineHandle> pipelineHandle) : pipelineHandle(std::move(pipelineHandle)) {}

    /// Returns `true` if the pipeline is null and not valid for use.
    bool isNull() const {
        return pipelineHandle.isNull();
    }

    /// Returns the associated Vulkan @vksymbol{VkPipeline} handle.
    VkPipelineHandle vkGetPipelineHandle() const {
        return pipelineHandle.vkGetHandle();
    }

private:
    Lifeguard<VkPipelineHandle> pipelineHandle;
};

/// Describes an individual shader stage of a pipeline, referencing a tp::ShaderModule and its entry point.
/// @see @vksymbol{VkPipelineShaderStageCreateInfo}
struct ShaderStageSetup {
    const ShaderModule* stageModule;
    const char* stageEntryPoint;
    ArrayView<SpecializationConstant> specializationConstants;

    ShaderStageSetup() : ShaderStageSetup(nullptr, nullptr) {}

    /// @param stageModule
    ///     The shader module to be used for this stage.
    /// @param stageEntryPoint
    ///     The entry point to be executed for this stage.
    /// @param specializationConstants
    ///     The values of specialization constants to be used for this stage.
    ShaderStageSetup(
        const ShaderModule* stageModule,
        const char* stageEntryPoint,
        ArrayView<SpecializationConstant> specializationConstants = {})
        : stageModule(stageModule),
          stageEntryPoint(stageEntryPoint),
          specializationConstants(specializationConstants) {}
};

/// Used as configuration for creating a new compute tp::Pipeline object for use inside compute passes.
/// @see tp::Device::compileComputePipelines
/// @see @vksymbol{VkComputePipelineCreateInfo}
class ComputePipelineSetup {
public:
    /// @param pipelineLayout
    ///     The pipeline layout to use.
    /// @param computeStageSetup
    ///     The setup of the compute shader stage.
    /// @param debugName
    ///     The debug name identifier for the object.
    ComputePipelineSetup(
        const PipelineLayout* pipelineLayout,
        ShaderStageSetup computeStageSetup,
        const char* debugName = nullptr);

    /// Sets the compute shader stage.
    ComputePipelineSetup& setComputeStage(ShaderStageSetup computeStageSetup);
    /// Adds the pipeline flags.
    ComputePipelineSetup& addFlags(PipelineFlagMask flags);
    /// Clears all pipeline flags.
    ComputePipelineSetup& clearFlags();
    /// Sets a debug name identifier for the object.
    ComputePipelineSetup& setDebugName(const char* debugName = nullptr);
    /// Sets the pointer to additional Vulkan structures to be passed in `pNext` of
    /// @vksymbol{VkComputePipelineCreateInfo}.
    ComputePipelineSetup& vkSetCreateInfoExtPtr(void* pNext);

private:
    friend class ComputePipelineInfoBuilder;

    const PipelineLayout* pipelineLayout;
    ShaderStageSetup computeStageSetup;
    PipelineFlagMask flags;
    std::string debugName;
    void* pNext = nullptr;
};

/// Used as configuration for creating a new graphics tp::Pipeline object for use inside render passes.
/// @see tp::Device::compileGraphicsPipelines
/// @see @vksymbol{VkGraphicsPipelineCreateInfo}
class GraphicsPipelineSetup {
public:
    /// @param pipelineLayout
    ///     The pipeline layout to use.
    /// @param vertexStageSetup
    ///     The setup of the vertex shader stage.
    /// @param fragmentStageSetup
    ///     The setup of the optional fragment shader stage.
    /// @param debugName
    ///     The debug name identifier for the object.
    GraphicsPipelineSetup(
        const PipelineLayout* pipelineLayout,
        ShaderStageSetup vertexStageSetup,
        ShaderStageSetup fragmentStageSetup = {},
        const char* debugName = nullptr);

    /// Sets the bindings for the vertex input buffers to the given array.
    GraphicsPipelineSetup& setVertexInputBindings(ArrayParameter<const VertexInputBinding> vertexInputBindings = {});
    /// Sets the vertex shader stage.
    GraphicsPipelineSetup& setVertexStage(ShaderStageSetup vertexStageSetup);
    /// Sets the fragment shader stage.
    GraphicsPipelineSetup& setFragmentStage(ShaderStageSetup fragmentStageSetup = {});
    /// Sets the geometry shader stage.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`geometryShader` feature must be enabled.
    GraphicsPipelineSetup& setGeometryStage(ShaderStageSetup geometryStageSetup = {});
    /// Sets the tessellation control stages.
    /// @param tessellationControlStageSetup
    ///     The setup of the tessellation control shader stage.
    /// @param tessellationEvaluationStageSetup
    ///     The setup of the tessellation evaluation shader stage.
    /// @param patchControlPoints
    ///     Specifies the number of control points per patch.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`tessellationShader` feature must be enabled.
    GraphicsPipelineSetup& setTessellationStages(
        ShaderStageSetup tessellationControlStageSetup = {},
        ShaderStageSetup tessellationEvaluationStageSetup = {},
        uint32_t patchControlPoints = 0);
    /// Sets the input primitive topology.
    /// @param topology
    ///     The primitive topology.
    /// @param primitiveRestartEnable
    ///     When enabled, a special vertex index value (~0) in the index buffer restarts the primitive assembly.
    GraphicsPipelineSetup& setTopology(
        PrimitiveTopology topology = PrimitiveTopology::TriangleList,
        bool primitiveRestartEnable = false);
    /// Sets the format of the depth stencil attachment that will be bound along with this pipeline.
    /// @param depthStencilAttachmentFormat
    ///     The format of the depth stencil attachment.
    /// @param depthStencilAspects
    ///     The used aspects of the depth stencil attachment.
    /// @remarks
    ///     The format and aspects must match the image view assigned to the corresponding attachment in
    ///     tp::RenderPassSetup of the active render pass when this pipeline is bound. If the attachment will be
    ///     unbound, the format must be set to tp::Format::Undefined and the aspects are ignored.
    GraphicsPipelineSetup& setDepthStencilAttachment(
        Format depthStencilAttachmentFormat = Format::Undefined,
        ImageAspectMask depthStencilAspects = ImageAspect::Depth | ImageAspect::Stencil);
    /// Sets the number and format of color attachments that will be bound along with this pipeline.
    /// @remarks
    ///     The formats must match the image views assigned to the corresponding attachments in tp::RenderPassSetup
    ///     of the active render pass when this pipeline is bound. If an attachment will be unbound,
    ///     the corresponding format must be set to tp::Format::Undefined.
    GraphicsPipelineSetup& setColorAttachments(ArrayParameter<const Format> colorAttachmentFormats = {});
    /// Sets the number of viewports.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`multiViewport` feature must be enabled for `viewportCount != 1`.
    GraphicsPipelineSetup& setViewportCount(uint32_t viewportCount = 1);
    /// Sets the view mask indicating the indices of attachment layers that will be rendered into when it is not 0.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceVulkan11Features}::`multiview` feature must be enabled for `viewMask != 0`.
    GraphicsPipelineSetup& setMultiViewMask(uint32_t viewMask = 0);
    /// Sets the rasterization mode.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`fillModeNonSolid` feature must be enabled for
    ///     `mode == RasterizationMode::Line` or `mode == RasterizationMode::Point`.
    GraphicsPipelineSetup& setRasterizationMode(RasterizationMode mode = RasterizationMode::Fill);
    /// Sets whether or not the clockwise winding order of primitives should be considered as front facing.
    GraphicsPipelineSetup& setFrontFace(bool frontFaceIsClockwise = false);
    /// Sets whether front and/or back faces should be culled.
    GraphicsPipelineSetup& setCullMode(CullModeFlagMask cullMode = CullModeFlagMask::None());
    /// Sets the depth bias functionality.
    /// @param enable
    ///     Enables depth bias if `true`.
    /// @param constantFactor
    ///     Controls the constant depth value added to each fragment.
    /// @param slopeFactor
    ///     Controls the slope dependent depth value added to each fragment.
    /// @param biasClamp
    ///     Sets the maximum depth bias of a fragment.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`depthBiasClamp` feature must be enabled for `biasClamp != 0.0f`.
    GraphicsPipelineSetup& setDepthBias(
        bool enable = false,
        float constantFactor = 0.0f,
        float slopeFactor = 0.0f,
        float biasClamp = 0.0f);
    /// Sets the multisampling functionality.
    /// @param level
    ///     The multisampling level to use, specifying the number of samples used in rasterization.
    /// @param sampleMask
    ///     The mask of samples to be applied to coverage.
    /// @param sampleShadingEnable
    ///     Enables shading of individual samples separately.
    /// @param minSampleShading
    ///     If `sampleShadingEnable` is true, specifies the minimum of samples to be shaded separately as a fraction.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`sampleRateShading` feature must be enabled for
    ///     `sampleShadingEnable == true`.
    GraphicsPipelineSetup& setMultisampling(
        MultisampleLevel level = MultisampleLevel::x1,
        uint64_t sampleMask = ~0,
        bool sampleShadingEnable = false,
        float minSampleShading = 1.0f);
    /// Sets the alpha to coverage functionality.
    /// @param enable
    ///     Enables alpha to coverage if `true`, generating coverage based on the alpha component of the fragment's
    ///     first color output.
    /// @param alphaToOneEnable
    ///     If `true`, replaces the alpha component with 1 afterwards.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`alphaToOne` feature must be enabled for
    ///     `alphaToOneEnable == true`.
    GraphicsPipelineSetup& setAlphaToCoverage(bool enable = false, bool alphaToOneEnable = false);
    /// Sets the depth testing functionality.
    /// @param enable
    ///     Enables depth operations if `true`.
    /// @param compareOp
    ///     The comparison operator to use for depth testing.
    /// @param enableWrite
    ///     If `true` and depth testing is enabled, also enables depth writes.
    GraphicsPipelineSetup& setDepthTest(
        bool enable = false,
        CompareOp compareOp = CompareOp::Always,
        bool enableWrite = false);
    /// Sets the depth bounds testing functionality.
    /// @param enable
    ///     Enables depth bounds testing if `true`, discarding samples if the existing depth value in the depth
    ///     attachment falls outside the given range.
    /// @param minDepthBounds
    ///     The minimum depth value.
    /// @param maxDepthBounds
    ///     The maximum depth value.
    /// @remarks
    ///     The values must be between 0.0 and 1.0 inclusive.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`depthBounds` feature must be enabled.
    GraphicsPipelineSetup& setDepthBoundsTest(
        bool enable = false,
        float minDepthBounds = 0.0f,
        float maxDepthBounds = 1.0f);
    /// Sets the depth clamp funcionality.
    /// @param enable
    ///     Enables depth clamp if `true`, clamping the output sample depth to the minimum and maximum depth values as
    ///     set by tp::RenderList::cmdSetViewport.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`depthClamp` feature must be enabled.
    GraphicsPipelineSetup& setDepthClamp(bool enable = false);
    /// Sets the stencil testing functionality.
    /// @param enable
    ///     Enables stencil testing if `true`.
    /// @param stencilState
    ///     The stencil state to use for both front faces and back faces.
    GraphicsPipelineSetup& setStencilTest(bool enable = false, StencilState stencilState = {});
    /// Sets the stencil testing functionality.
    /// @param enable
    ///     Enables stencil testing if `true`.
    /// @param frontFaceStencilState
    ///     The stencil state to use for front faces.
    /// @param backFaceStencilState
    ///     The stencil state to use for back faces.
    GraphicsPipelineSetup& setStencilTest(
        bool enable = false,
        StencilState frontFaceStencilState = {},
        StencilState backFaceStencilState = {});
    /// Sets the logic blend operation functinality.
    /// @param enable
    ///     Enables logic blending on integer color attachment formats if `true`.
    /// @param logicOp
    ///     The logical operator to use.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`logicOp` feature must be enabled.
    GraphicsPipelineSetup& setLogicBlendOp(bool enable = false, LogicOp logicOp = LogicOp::And);
    /// Sets the width of rasterized line segments.
    /// @param width
    ///     The width of the rasterized line segments in pixels.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`wideLines` feature must be enabled for `width != 1.0`.
    GraphicsPipelineSetup& setLineWidth(float width = 1.0f);
    /// Sets the attachment blending functionality.
    /// @param enable
    ///     Enables blending operations on color attachments if `true`.
    /// @param blendState
    ///     The blend state used for all color attachments.
    GraphicsPipelineSetup& setBlending(bool enable = false, AttachmentBlendState blendState = {});
    /// Sets the attachment blending functionality with an independent blend state for each attachment.
    /// @param enable
    ///     Enables blending operations on color attachments if `true`.
    /// @param blendStates
    ///     The blend states used for each color attachment.
    /// @remarks
    ///     The size of the array must match the number of color attachments provided in `setColorAttachments`.
    /// @remarks
    ///     The @vksymbol{VkPhysicalDeviceFeatures}::`independentBlend` feature must be enabled.
    GraphicsPipelineSetup& setIndependentBlending(
        bool enable = false,
        ArrayParameter<const AttachmentBlendState> blendStates = {});
    /// Sets the constants used for certain blend factors during blending operations.
    GraphicsPipelineSetup& setBlendConstants(float blendConstants[4]);
    /// Adds a dynamic state flag, ignoring the associated fields in tp::GraphicsPipelineSetup in favor of setting
    /// them dynamically through the methods in tp::RenderList.
    GraphicsPipelineSetup& addDynamicState(DynamicState dynamicState);
    /// Clears all dynamic state flags.
    GraphicsPipelineSetup& clearDynamicState();
    /// Adds the pipeline flags.
    GraphicsPipelineSetup& addFlags(PipelineFlagMask flags);
    /// Clears all pipeline flags.
    GraphicsPipelineSetup& clearFlags();
    /// Sets a debug name identifier for the object.
    GraphicsPipelineSetup& setDebugName(const char* debugName = nullptr);
    // TODO: Accept extending structures for contained CreateInfos, re-route them based on their sType.
    /// Sets the pointer to additional Vulkan structures to be passed in `pNext` of
    /// @vksymbol{VkGraphicsPipelineCreateInfo}.
    GraphicsPipelineSetup& vkSetCreateInfoExtPtr(void* pNext);

private:
    friend class GraphicsPipelineInfoBuilder;

    const PipelineLayout* pipelineLayout;
    std::vector<VertexInputBinding> vertexInputBindings;

    ShaderStageSetup vertexStageSetup;
    ShaderStageSetup fragmentStageSetup;
    ShaderStageSetup geometryStageSetup;
    ShaderStageSetup tessellationControlStageSetup;
    ShaderStageSetup tessellationEvaluationStageSetup;
    uint32_t patchControlPoints = 0;

    Format depthStencilAttachmentFormat = Format::Undefined;
    ImageAspectMask depthStencilAspects = ImageAspect::Depth | ImageAspect::Stencil;
    std::vector<Format> colorAttachmentFormats = {};
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool primitiveRestartEnable = false;
    uint32_t viewportCount = 1;
    uint32_t viewMask = 0;
    RasterizationMode rasterizationMode = RasterizationMode::Fill;
    bool frontFaceIsClockwise = false;
    bool depthClampEnable = false;
    CullModeFlagMask cullMode = CullModeFlagMask::None();

    bool depthBiasEnable = false;
    float depthBiasConstantFactor = 0.0f;
    float depthBiasSlopeFactor = 0.0f;
    float depthBiasClamp = 0.0f;

    float lineWidth = 1.0f;
    bool blendEnable = false;
    bool independentBlendEnable = false;
    std::vector<AttachmentBlendState> blendStates;
    float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    MultisampleLevel multisampleLevel = MultisampleLevel::x1;
    uint64_t sampleMask = ~0;
    bool sampleShadingEnable = false;
    float minSampleShading = 1.0f;

    bool alphaToCoverageEnable = false;
    bool alphaToOneEnable = false;

    bool depthTestEnable = false;
    CompareOp depthTestCompareOp = CompareOp::Always;
    bool depthWriteEnable = false;

    bool depthBoundsTestEnable = false;
    float minDepthBounds = 0.0f;
    float maxDepthBounds = 1.0f;

    bool stencilTestEnable = false;
    StencilState frontFaceStencilState;
    StencilState backFaceStencilState;

    bool logicBlendEnable = false;
    LogicOp logicBlendOp = LogicOp::And;

    std::vector<DynamicState> dynamicStates;
    PipelineFlagMask flags;
    std::string debugName;
    void* pNext = nullptr;
};

}
