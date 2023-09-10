#pragma once

#include <tephra/tools/enum_tools.hpp>
#include <tephra/vulkan/header.hpp>
#include <cstdint>
#include <cstddef>

namespace tp {

// --- Conversion functionality ---

template <typename TEnum>
struct VkConvertedEnumType {};

template <typename TEnum>
using VkConvertedEnumType_t = typename VkConvertedEnumType<TEnum>::type;

template <typename TEnum>
using IsVkEnumConvertibleBothWays = std::is_same<VkConvertedEnumType_t<VkConvertedEnumType_t<TEnum>>, TEnum>;

template <typename TEnum>
constexpr VkConvertedEnumType_t<TEnum> vkCastConvertibleEnum(TEnum value) noexcept {
    return static_cast<VkConvertedEnumType_t<TEnum>>(value);
}

template <typename TEnum, typename = std::enable_if_t<IsVkEnumConvertibleBothWays<TEnum>::value>>
constexpr VkConvertedEnumType_t<TEnum>* vkCastConvertibleEnumPtr(TEnum* ptr) noexcept {
    return reinterpret_cast<VkConvertedEnumType_t<TEnum>*>(ptr);
}

template <typename TEnum, typename = std::enable_if_t<IsVkEnumConvertibleBothWays<TEnum>::value>>
constexpr const VkConvertedEnumType_t<TEnum>* vkCastConvertibleEnumPtr(const TEnum* ptr) noexcept {
    return reinterpret_cast<const VkConvertedEnumType_t<TEnum>*>(ptr);
}

template <typename TEnum>
constexpr VkFlags vkCastConvertibleEnumMask(EnumBitMask<TEnum> mask) noexcept {
    return vkCastConvertibleEnum(static_cast<TEnum>(mask));
}

template <typename TEnumTp, typename TEnumVk>
constexpr EnumBitMask<TEnumTp> vkCastConvertibleEnumMask(VkFlags mask) noexcept {
    TEnumTp tpEnum = vkCastConvertibleEnum(static_cast<TEnumVk>(mask));
    return EnumBitMask<TEnumTp>(tpEnum);
}

/// Adds two way conversion functionality between Tephra enum and Vulkan enum.
/// It guarantees that Tephra enum does not include any values that do not map to values in
/// the corresponding Vulkan enum. It may omit some Vulkan values from extensions or due to
/// redundancy, but they are still valid values for the type.
#define TEPHRA_VULKAN_COMPATIBLE_ENUM(tephraEnumType, vulkanEnumType) \
    template <> \
    struct VkConvertedEnumType<tephraEnumType> { \
        using type = vulkanEnumType; \
    }; \
    template <> \
    struct VkConvertedEnumType<vulkanEnumType> { \
        using type = tephraEnumType; \
    }

/// Adds one way conversion functionality from Vulkan enum to Tephra enum.
/// It only guarantees that all of the Vulkan values are valid values in the Tephra enum,
/// even if they are omitted.
#define TEPHRA_VULKAN_CONVERTIBLE_TO_TP_ENUM(tephraEnumType, vulkanEnumType) \
    template <> \
    struct VkConvertedEnumType<vulkanEnumType> { \
        using type = tephraEnumType; \
    }

// --- Vulkan enum wrappers ---

/// The general type of a physical device
/// @see @vksymbol{VkPhysicalDeviceType}
enum class DeviceType {
    /// The device does not match any other available types.
    Other = VK_PHYSICAL_DEVICE_TYPE_OTHER,
    /// The device is typically one embedded in or tightly coupled with the host.
    IntegratedGPU = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
    /// The device is typically a separate processor connected to the host via an interlink.
    DiscreteGPU = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
    /// The device is typically a virtual node in a virtualization environment.
    VirtualGPU = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
    /// The device is typically running on the same processors as the host.
    CPU = VK_PHYSICAL_DEVICE_TYPE_CPU,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(DeviceType, VkPhysicalDeviceType);

/// Specifies how a certain resource is to be accessed from within a shader.
///
/// In terms of the type of access, descriptors can be categorized by their tp::DescriptorType into:
///
/// Sampled descriptors, providing read-only access with format conversions:
///     - tp::DescriptorType::CombinedImageSampler
///     - tp::DescriptorType::SampledImage
///     - tp::DescriptorType::InputAttachment
///     - tp::DescriptorType::TexelBuffer
///
/// Storage descriptors, providing read, write and atomic access:
///     - tp::DescriptorType::StorageImage
///     - tp::DescriptorType::StorageBuffer
///     - tp::DescriptorType::StorageBufferDynamic
///     - tp::DescriptorType::StorageTexelBuffer
///
/// Uniform buffer descriptors, providing read-only access to uniform buffers:
///     - tp::DescriptorType::UniformBuffer
///     - tp::DescriptorType::UniformBufferDynamic
///
/// Sampler descriptors:
///     - tp::DescriptorType::Sampler
///
/// @see @vksymbol{VkDescriptorType}
enum class DescriptorType : uint32_t {
    /// A descriptor for a tp::Sampler object.
    Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
    /// A descriptor for a combination of tp::ImageView and a tp::Sampler objects as a read-only sampled image using
    /// the provided sampler.
    CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    /// A descriptor for a tp::ImageView object as a read-only sampled image.
    SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    /// A descriptor for a tp::ImageView object as a read/write storage image.
    StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    /// A descriptor for a tp::BufferView object as a formatted read-only buffer.
    TexelBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    /// A descriptor for a tp::BufferView object as a formatted read/write buffer.
    StorageTexelBuffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
    /// A descriptor for a tp::BufferView object as a read-only uniform (constant) buffer. This type of buffer
    /// descriptor is particularly suited for constants accessed uniformly by shader invocations.
    UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    /// A descriptor for a tp::BufferView object as a read/write buffer.
    StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    /// A descriptor for a tp::BufferView object as a read-only uniform (constant) buffer with a dynamic offset.
    UniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    /// A descriptor for a tp::BufferView object as a read/write buffer with a dynamic offset.
    StorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    /// A descriptor for a tp::ImageView object that is bound as an input attachment in a subpass of a render pass.
    InputAttachment = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(DescriptorType, VkDescriptorType);

/// Specifies the type of primitive topology.
/// @see @vksymbol{VkPrimitiveTopology}
enum class PrimitiveTopology : uint32_t {
    /// Specifies a series of separate point primitives.
    PointList = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    /// Specifies a series of separate line primitives.
    LineList = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    /// Specifies a series of connected line primitives with consecutive lines sharing a vertex.
    LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    /// Specifies a series of separate triangle primitives.
    TriangleList = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    /// Specifies a series of connected triangle primitives with consecutive triangles sharing an edge.
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    /// Specifies a series of connected triangle primitives with all triangles sharing a common vertex.
    TriangleFan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
    /// Specifies a series of separate line primitives with adjacency.
    LineListWithAdjacency = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
    /// Specifies a series of connected line primitives with adjacency, with consecutive primitives sharing three
    /// vertices.
    LineStripWithAdjacency = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
    /// Specifies a series of separate triangle primitives with adjacency.
    TriangleListWithAdjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
    /// Specifies connected triangle primitives with adjacency, with consecutive triangles sharing an edge.
    TriangleStripWithAdjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
    /// Specifies separate patch primitives.
    PatchList = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(PrimitiveTopology, VkPrimitiveTopology);

/// Specifies the stage of shader execution.
/// @see @vksymbol{VkShaderStageFlagBits}
enum class ShaderStage : uint32_t {
    /// The vertex shader stage
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    /// The tessellation control shader stage.
    TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    /// The tessellation evaluation shader stage.
    TessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    /// The geometry shader stage.
    Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    /// The fragment shader stage.
    Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    /// The compute shader stage.
    Compute = VK_SHADER_STAGE_COMPUTE_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ShaderStage, VkShaderStageFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ShaderStageMask, ShaderStage);

/// Additional pipeline creation options.
/// @see @vksymbol{VkPipelineCreateFlagBits}
enum class PipelineFlag : uint32_t {
    /// Asks the implementation to disable optimizations of the pipeline.
    DisableOptimizations = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(PipelineFlag, VkPipelineCreateFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(PipelineFlagMask, PipelineFlag);

/// The rate at which input attributes are pulled from buffers.
/// @see @vksymbol{VkVertexInputRate}
enum class VertexInputRate : uint32_t {
    /// The input attribute values will be consumed per-vertex.
    Vertex = VK_VERTEX_INPUT_RATE_VERTEX,
    /// The input attribute values will be consumed per-instance.
    Instance = VK_VERTEX_INPUT_RATE_INSTANCE
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(VertexInputRate, VkVertexInputRate);

/// Flags controlling which triangles get discarded.
/// @see @vksymbol{VkCullModeFlagBits}
enum class CullModeFlag : uint32_t {
    /// Triangles that are considered to be front facing won't be rasterized.
    FrontFace = VK_CULL_MODE_FRONT_BIT,
    /// Triangles that are considered to be back facing won't be rasterized.
    BackFace = VK_CULL_MODE_BACK_BIT,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(CullModeFlag, VkCullModeFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(CullModeFlagMask, CullModeFlag);

/// Comparison operators for depth, stencil and sampler operations.
/// @see @vksymbol{VkCompareOp}
enum class CompareOp : uint32_t {
    Never = VK_COMPARE_OP_NEVER,
    Less = VK_COMPARE_OP_LESS,
    Equal = VK_COMPARE_OP_EQUAL,
    LessOrEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater = VK_COMPARE_OP_GREATER,
    NotEqual = VK_COMPARE_OP_NOT_EQUAL,
    GreaterOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always = VK_COMPARE_OP_ALWAYS
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(CompareOp, VkCompareOp);

/// Stencil function specifying what happens to the stored stencil value.
/// @see @vksymbol{VkStencilOp}
enum class StencilOp : uint32_t {
    Keep = VK_STENCIL_OP_KEEP,
    Zero = VK_STENCIL_OP_ZERO,
    Replace = VK_STENCIL_OP_REPLACE,
    IncrementAndClamp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    DecrementAndClamp = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    Invert = VK_STENCIL_OP_INVERT,
    IncrementAndWrap = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    DecrementAndWrap = VK_STENCIL_OP_DECREMENT_AND_WRAP
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(StencilOp, VkStencilOp);

/// Logical comparison operators.
/// @see @vksymbol{VkLogicOp}
enum class LogicOp : uint32_t {
    Clear = VK_LOGIC_OP_CLEAR,
    And = VK_LOGIC_OP_AND,
    AndReverse = VK_LOGIC_OP_AND_REVERSE,
    Copy = VK_LOGIC_OP_COPY,
    AndInverted = VK_LOGIC_OP_AND_INVERTED,
    NoOp = VK_LOGIC_OP_NO_OP,
    Xor = VK_LOGIC_OP_XOR,
    Or = VK_LOGIC_OP_OR,
    Nor = VK_LOGIC_OP_NOR,
    Equivalent = VK_LOGIC_OP_EQUIVALENT,
    Invert = VK_LOGIC_OP_INVERT,
    OrReverse = VK_LOGIC_OP_OR_REVERSE,
    CopyInverted = VK_LOGIC_OP_COPY_INVERTED,
    OrInverted = VK_LOGIC_OP_OR_INVERTED,
    Nand = VK_LOGIC_OP_NAND,
    Set = VK_LOGIC_OP_SET
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(LogicOp, VkLogicOp);

/// Framebuffer blending operators.
/// @see @vksymbol{VkBlendOp}
enum class BlendOp : uint32_t {
    Add = VK_BLEND_OP_ADD,
    Subtract = VK_BLEND_OP_SUBTRACT,
    ReverseSubtract = VK_BLEND_OP_REVERSE_SUBTRACT,
    Min = VK_BLEND_OP_MIN,
    Max = VK_BLEND_OP_MAX
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(BlendOp, VkBlendOp);

/// Framebuffer blending factors.
/// @see @vksymbol{VkBlendFactor}
enum class BlendFactor : uint32_t {
    Zero = VK_BLEND_FACTOR_ZERO,
    One = VK_BLEND_FACTOR_ONE,
    SrcColor = VK_BLEND_FACTOR_SRC_COLOR,
    OneMinusSrcColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    DstColor = VK_BLEND_FACTOR_DST_COLOR,
    OneMinusDstColor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    SrcAlpha = VK_BLEND_FACTOR_SRC_ALPHA,
    OneMinusSrcAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    DstAlpha = VK_BLEND_FACTOR_DST_ALPHA,
    OneMinusDstAlpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    ConstantColor = VK_BLEND_FACTOR_CONSTANT_COLOR,
    OneMinusConstantColor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    ConstantAlpha = VK_BLEND_FACTOR_CONSTANT_ALPHA,
    OneMinusConstantAlpha = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    SrcAlphaSaturate = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    Src1Color = VK_BLEND_FACTOR_SRC1_COLOR,
    OneMinusSrc1Color = VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
    Src1Alpha = VK_BLEND_FACTOR_SRC1_ALPHA,
    OneMinusSrc1Alpha = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(BlendFactor, VkBlendFactor);

/// Sample counts that can be used for image storage operations.
/// @see @vksymbol{VkSampleCountFlagBits}
enum class MultisampleLevel : uint32_t {
    x1 = VK_SAMPLE_COUNT_1_BIT,
    x2 = VK_SAMPLE_COUNT_2_BIT,
    x4 = VK_SAMPLE_COUNT_4_BIT,
    x8 = VK_SAMPLE_COUNT_8_BIT,
    x16 = VK_SAMPLE_COUNT_16_BIT,
    x32 = VK_SAMPLE_COUNT_32_BIT,
    x64 = VK_SAMPLE_COUNT_64_BIT,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(MultisampleLevel, VkSampleCountFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(MultisampleLevelMask, MultisampleLevel);

/// Specifies the values placed in a component of the output color vector.
/// @see @vksymbol{VkComponentSwizzle}
enum class ComponentSwizzle : uint32_t {
    Identity = VK_COMPONENT_SWIZZLE_IDENTITY,
    Zero = VK_COMPONENT_SWIZZLE_ZERO,
    One = VK_COMPONENT_SWIZZLE_ONE,
    R = VK_COMPONENT_SWIZZLE_R,
    G = VK_COMPONENT_SWIZZLE_G,
    B = VK_COMPONENT_SWIZZLE_B,
    A = VK_COMPONENT_SWIZZLE_A
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ComponentSwizzle, VkComponentSwizzle);

/// Identifies a component of a color image.
/// @see @vksymbol{VkColorComponentFlagBits}
enum class ColorComponent : uint32_t {
    Red = VK_COLOR_COMPONENT_R_BIT,
    Green = VK_COLOR_COMPONENT_G_BIT,
    Blue = VK_COLOR_COMPONENT_B_BIT,
    Alpha = VK_COLOR_COMPONENT_A_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ColorComponent, VkColorComponentFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ColorComponentMask, ColorComponent);

/// Determines the method how a multisampled image should be resolved
/// @see @vksymbol{VkResolveModeFlagBits}
enum class ResolveMode : uint32_t {
    SampleZero = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
    Average = VK_RESOLVE_MODE_AVERAGE_BIT,
    Min = VK_RESOLVE_MODE_MIN_BIT,
    Max = VK_RESOLVE_MODE_MAX_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ResolveMode, VkResolveModeFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ResolveModeMask, ResolveMode);

/// Specifies the parts of the tp::GraphicsPipelineSetup state that are to be taken from the dynamic state commands
/// recorded into a tp::RenderList, rather than from the pipeline setup, which will be ignored.
/// @remarks
///     `VK_DYNAMIC_STATE_VIEWPORT` and `VK_DYNAMIC_STATE_SCISSOR` are always enabled and therefore not present
///     in this enum. The remaining missing values are supported, but don't have corresponding methods to set their
///     state defined in tp::RenderList - you will need to call the Vulkan functions directly.
/// @see @vksymbol{VkDynamicState}
enum class DynamicState : uint32_t {
    /// Replaces tp::GraphicsPipelineSetup::setLineWidth with tp::RenderList::cmdSetLineWidth.
    LineWidth = VK_DYNAMIC_STATE_LINE_WIDTH,
    /// Replaces tp::GraphicsPipelineSetup::setDepthBias with tp::RenderList::cmdSetDepthBias.
    DepthBias = VK_DYNAMIC_STATE_DEPTH_BIAS,
    /// Replaces tp::GraphicsPipelineSetup::setBlendConstants with tp::RenderList::cmdSetBlendConstants.
    BlendConstants = VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    /// Replaces tp::GraphicsPipelineSetup::setDepthBoundsTest with tp::RenderList::cmdSetDepthBounds.
    DepthBounds = VK_DYNAMIC_STATE_DEPTH_BOUNDS
    // TODO: Add more state
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(DynamicState, VkDynamicState);

/// The allowed usage of a tp::Image.
/// @see @vksymbol{VkImageUsageFlagBits}
enum class ImageUsage : uint32_t {
    /// Allows the image to be used as the source image of copy, resolve and blit commands.
    TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    /// Allows the image to be used as the destination image of copy, resolve and blit commands and
    /// tp::Job::cmdClearImage
    TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    /// Allows the image to be used in a tp::DescriptorType::SampledImage or tp::DescriptorType::CombinedImageSampler
    /// descriptor.
    SampledImage = VK_IMAGE_USAGE_SAMPLED_BIT,
    /// Allows the image to be used in a tp::DescriptorType::StorageImage descriptor.
    StorageImage = VK_IMAGE_USAGE_STORAGE_BIT,
    /// Allows the image to be used as a color attachment in tp::Job::cmdExecuteRenderPass.
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    /// Allows the image to be used as a depth or stencil attachment in tp::Job::cmdExecuteRenderPass.
    DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    /// Allows the image to be used in a tp::DescriptorType::InputAttachment descriptor.
    InputAttachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ImageUsage, VkImageUsageFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ImageUsageMask, ImageUsage);

/// Additional image creation options.
/// @see @vksymbol{VkImageCreateFlagBits}
enum class ImageFlag : uint32_t {
    /// When used on images with block compressed formats, allows constructing views out of them with an uncompressed
    /// format where each texel in the image view corresponds to a compressed texel block of the image.
    BlockTexelViewCompatible = VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ImageFlag, VkImageCreateFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ImageFlagMask, ImageFlag);

/// The dimensionality of an image view.
/// @see @vksymbol{VkImageViewType}
enum class ImageViewType : uint32_t {
    /// One-dimensional image view.
    View1D = VK_IMAGE_VIEW_TYPE_1D,
    /// Two-dimensional image view.
    View2D = VK_IMAGE_VIEW_TYPE_2D,
    /// Three-dimensional image view.
    View3D = VK_IMAGE_VIEW_TYPE_3D,
    /// Two-dimensional image view of six layers representing the sides of a cubemap.
    /// The layers are interpreted as follows:
    /// 0: Positive X
    /// 1: Negative X
    /// 2: Positive Y
    /// 3: Negative Y
    /// 4: Positive Z
    /// 5: Negative Z
    ViewCube = VK_IMAGE_VIEW_TYPE_CUBE,
    /// One-dimensional image view with multiple layers.
    View1DArray = VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    /// Two-dimensional image view with multiple layers.
    View2DArray = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    /// Two-dimensional image view of multiples of six layers representing the sides of cubemaps as in
    /// tp::ImageViewType::ViewCube.
    ViewCubeArray = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ImageViewType, VkImageViewType);

/// The aspect storing a type of data of an image view.
/// @see @vksymbol{VkImageAspectFlagBits}
enum class ImageAspect : uint32_t {
    Color = VK_IMAGE_ASPECT_COLOR_BIT,
    Depth = VK_IMAGE_ASPECT_DEPTH_BIT,
    Stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
    Metadata = VK_IMAGE_ASPECT_METADATA_BIT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(ImageAspect, VkImageAspectFlagBits);
TEPHRA_MAKE_ENUM_BIT_MASK(ImageAspectMask, ImageAspect);

/// The load operation applied to the contents of an attachment at the start of a render pass.
/// @see @vksymbol{VkAttachmentLoadOp}
enum class AttachmentLoadOp : uint32_t {
    /// Specifies that the attachment will load the contents of the assigned image view.
    Load = VK_ATTACHMENT_LOAD_OP_LOAD,
    /// Specifies that the attachment will be cleared to a specified value.
    Clear = VK_ATTACHMENT_LOAD_OP_CLEAR,
    /// Specifies that the contents of the attachment may be undefined.
    DontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(AttachmentLoadOp, VkAttachmentLoadOp);

/// The store operation applied to the contents of an attachment.
/// @see @vksymbol{VkAttachmentStoreOp}
enum class AttachmentStoreOp : uint32_t {
    /// Specifies that the contents of the attachment will be stored into the assigned image view.
    Store = VK_ATTACHMENT_STORE_OP_STORE,
    /// Specifies that the contents of the image view may be left undefined.
    DontCare = VK_ATTACHMENT_STORE_OP_DONT_CARE
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(AttachmentStoreOp, VkAttachmentStoreOp);

/// The type of values of an index buffer.
/// @see @vksymbol{VkIndexType}
enum class IndexType : uint32_t {
    UInt16 = VK_INDEX_TYPE_UINT16,
    UInt32 = VK_INDEX_TYPE_UINT32,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(IndexType, VkIndexType);

/// Transforms applied to a surface upon presentation.
/// @see @vksymbol{VkSurfaceTransformFlagBitsKHR}
enum class SurfaceTransform : uint32_t {
    /// Chooses the currently used transform as reported by the platform.
    UseCurrentTransform = 0,
    Identity = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    Rotate90 = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
    Rotate180 = VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
    Rotate270 = VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
    HorizontalMirror = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
    HorizontalMirrorRotate90 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
    HorizontalMirrorRotate180 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
    HorizontalMirrorRotate270 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR,
    Inherit = VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR
};
TEPHRA_VULKAN_CONVERTIBLE_TO_TP_ENUM(SurfaceTransform, VkSurfaceTransformFlagBitsKHR);
TEPHRA_MAKE_ENUM_BIT_MASK(SurfaceTransformMask, SurfaceTransform);

/// The alpha composition used for the surface upon presentation.
/// @see @vksymbol{VkCompositeAlphaFlagBitsKHR}
enum class CompositeAlpha : uint32_t {
    Opaque = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    PreMultiplied = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
    PostMultiplied = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    Inherit = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(CompositeAlpha, VkCompositeAlphaFlagBitsKHR);
TEPHRA_MAKE_ENUM_BIT_MASK(CompositeAlphaMask, CompositeAlpha);

/// The filtering mode.
/// @see @vksymbol{VkFilter}
enum class Filter : uint32_t {
    Nearest = VK_FILTER_NEAREST,
    Linear = VK_FILTER_LINEAR,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(Filter, VkFilter);

/// The behavior of sampling with texture coordinates outside an image.
/// @see @vksymbol{VkSamplerAddressMode}
enum class SamplerAddressMode : uint32_t {
    Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    MirrorClampToEdge = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(SamplerAddressMode, VkSamplerAddressMode);

/// The border color applied when using a border tp::SamplerAddressMode.
/// @see @vksymbol{VkBorderColor}
enum class BorderColor : uint32_t {
    FloatTransparentBlack = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    IntTransparentBlack = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
    FloatOpaqueBlack = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    IntOpaqueBlack = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    FloatOpaqueWhite = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    IntOpaqueWhite = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(BorderColor, VkBorderColor);

/// Additional swapchain creation options.
/// @see @vksymbol{VkSwapchainCreateFlagBitsKHR}
enum class SwapchainFlag : uint32_t {};
TEPHRA_VULKAN_COMPATIBLE_ENUM(SwapchainFlag, VkSwapchainCreateFlagBitsKHR);
TEPHRA_MAKE_ENUM_BIT_MASK(SwapchainFlagMask, SwapchainFlag);

/// The possible presentation modes for a surface.
/// @see @vksymbol{VkPresentModeKHR}
enum class PresentMode : uint32_t {
    /// Presented images appear on the screen immediately, without waiting for the next vertical blanking period.
    /// This mode may cause visible tearing.
    Immediate = VK_PRESENT_MODE_IMMEDIATE_KHR,
    /// Presented images queue up for being displayed on the screen. During each vertical blanking period, the most
    /// recent presented image will be displayed.
    Mailbox = VK_PRESENT_MODE_MAILBOX_KHR,
    /// Presented images queue up for being displayed on the screen. During each vertical blanking period, the least
    /// recent presented image will be displayed. This mode may cause tp::Swapchain::acquireNextImage to wait for an
    /// image to become available, effectively tying the rate of presentation to the screen's vertical blanking period.
    /// This is the only mode that is always supported.
    FIFO = VK_PRESENT_MODE_FIFO_KHR,
    /// Similar to tp::PresentMode::FIFO, except if the application has not presented an image in time for the next
    /// vertical blanking period, the next time an image gets presented, it will be displayed on the screen
    /// immediately. This should help smooth out the framerate, but it may also cause visible tearing in those
    /// situations.
    RelaxedFIFO = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(PresentMode, VkPresentModeKHR);

/// The formats that data can be stored in inside buffers and images.
/// @see @vksymbol{VkFormat}
enum class Format : uint32_t {
    Undefined = VK_FORMAT_UNDEFINED,

    // 8-bit color formats
    COL8_R4G4_UNORM_PACK = VK_FORMAT_R4G4_UNORM_PACK8,
    COL8_R8_UNORM = VK_FORMAT_R8_UNORM,
    COL8_R8_SNORM = VK_FORMAT_R8_SNORM,
    COL8_R8_USCALED = VK_FORMAT_R8_USCALED,
    COL8_R8_SSCALED = VK_FORMAT_R8_SSCALED,
    COL8_R8_UINT = VK_FORMAT_R8_UINT,
    COL8_R8_SINT = VK_FORMAT_R8_SINT,
    COL8_R8_SRGB = VK_FORMAT_R8_SRGB,

    // 16-bit color formats
    COL16_R4G4B4A4_UNORM_PACK = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    COL16_B4G4R4A4_UNORM_PACK = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
    COL16_R5G6B5_UNORM_PACK = VK_FORMAT_R5G6B5_UNORM_PACK16,
    COL16_B5G6R5_UNORM_PACK = VK_FORMAT_B5G6R5_UNORM_PACK16,
    COL16_R5G5B5A1_UNORM_PACK = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
    COL16_B5G5R5A1_UNORM_PACK = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
    COL16_A1R5G5B5_UNORM_PACK = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
    COL16_R8G8_UNORM = VK_FORMAT_R8G8_UNORM,
    COL16_R8G8_SNORM = VK_FORMAT_R8G8_SNORM,
    COL16_R8G8_USCALED = VK_FORMAT_R8G8_USCALED,
    COL16_R8G8_SSCALED = VK_FORMAT_R8G8_SSCALED,
    COL16_R8G8_UINT = VK_FORMAT_R8G8_UINT,
    COL16_R8G8_SINT = VK_FORMAT_R8G8_SINT,
    COL16_R8G8_SRGB = VK_FORMAT_R8G8_SRGB,
    COL16_R16_UNORM = VK_FORMAT_R16_UNORM,
    COL16_R16_SNORM = VK_FORMAT_R16_SNORM,
    COL16_R16_USCALED = VK_FORMAT_R16_USCALED,
    COL16_R16_SSCALED = VK_FORMAT_R16_SSCALED,
    COL16_R16_UINT = VK_FORMAT_R16_UINT,
    COL16_R16_SINT = VK_FORMAT_R16_SINT,
    COL16_R16_SFLOAT = VK_FORMAT_R16_SFLOAT,

    // 24-bit color formats
    COL24_R8G8B8_UNORM = VK_FORMAT_R8G8B8_UNORM,
    COL24_R8G8B8_SNORM = VK_FORMAT_R8G8B8_SNORM,
    COL24_R8G8B8_USCALED = VK_FORMAT_R8G8B8_USCALED,
    COL24_R8G8B8_SSCALED = VK_FORMAT_R8G8B8_SSCALED,
    COL24_R8G8B8_UINT = VK_FORMAT_R8G8B8_UINT,
    COL24_R8G8B8_SINT = VK_FORMAT_R8G8B8_SINT,
    COL24_R8G8B8_SRGB = VK_FORMAT_R8G8B8_SRGB,
    COL24_B8G8R8_UNORM = VK_FORMAT_B8G8R8_UNORM,
    COL24_B8G8R8_SNORM = VK_FORMAT_B8G8R8_SNORM,
    COL24_B8G8R8_USCALED = VK_FORMAT_B8G8R8_USCALED,
    COL24_B8G8R8_SSCALED = VK_FORMAT_B8G8R8_SSCALED,
    COL24_B8G8R8_UINT = VK_FORMAT_B8G8R8_UINT,
    COL24_B8G8R8_SINT = VK_FORMAT_B8G8R8_SINT,
    COL24_B8G8R8_SRGB = VK_FORMAT_B8G8R8_SRGB,

    // 32-bit color formats
    COL32_R8G8B8A8_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
    COL32_R8G8B8A8_SNORM = VK_FORMAT_R8G8B8A8_SNORM,
    COL32_R8G8B8A8_USCALED = VK_FORMAT_R8G8B8A8_USCALED,
    COL32_R8G8B8A8_SSCALED = VK_FORMAT_R8G8B8A8_SSCALED,
    COL32_R8G8B8A8_UINT = VK_FORMAT_R8G8B8A8_UINT,
    COL32_R8G8B8A8_SINT = VK_FORMAT_R8G8B8A8_SINT,
    COL32_R8G8B8A8_SRGB = VK_FORMAT_R8G8B8A8_SRGB,
    COL32_B8G8R8A8_UNORM = VK_FORMAT_B8G8R8A8_UNORM,
    COL32_B8G8R8A8_SNORM = VK_FORMAT_B8G8R8A8_SNORM,
    COL32_B8G8R8A8_USCALED = VK_FORMAT_B8G8R8A8_USCALED,
    COL32_B8G8R8A8_SSCALED = VK_FORMAT_B8G8R8A8_SSCALED,
    COL32_B8G8R8A8_UINT = VK_FORMAT_B8G8R8A8_UINT,
    COL32_B8G8R8A8_SINT = VK_FORMAT_B8G8R8A8_SINT,
    COL32_B8G8R8A8_SRGB = VK_FORMAT_B8G8R8A8_SRGB,
    COL32_A8B8G8R8_UNORM_PACK = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
    COL32_A8B8G8R8_SNORM_PACK = VK_FORMAT_A8B8G8R8_SNORM_PACK32,
    COL32_A8B8G8R8_USCALED_PACK = VK_FORMAT_A8B8G8R8_USCALED_PACK32,
    COL32_A8B8G8R8_SSCALED_PACK = VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
    COL32_A8B8G8R8_UINT_PACK = VK_FORMAT_A8B8G8R8_UINT_PACK32,
    COL32_A8B8G8R8_SINT_PACK = VK_FORMAT_A8B8G8R8_SINT_PACK32,
    COL32_A8B8G8R8_SRGB_PACK = VK_FORMAT_A8B8G8R8_SRGB_PACK32,
    COL32_A2R10G10B10_UNORM_PACK = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    COL32_A2R10G10B10_SNORM_PACK = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
    COL32_A2R10G10B10_USCALED_PACK = VK_FORMAT_A2R10G10B10_USCALED_PACK32,
    COL32_A2R10G10B10_SSCALED_PACK = VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
    COL32_A2R10G10B10_UINT_PACK = VK_FORMAT_A2R10G10B10_UINT_PACK32,
    COL32_A2R10G10B10_SINT_PACK = VK_FORMAT_A2R10G10B10_SINT_PACK32,
    COL32_A2B10G10R10_UNORM_PACK = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    COL32_A2B10G10R10_SNORM_PACK = VK_FORMAT_A2B10G10R10_SNORM_PACK32,
    COL32_A2B10G10R10_USCALED_PACK = VK_FORMAT_A2B10G10R10_USCALED_PACK32,
    COL32_A2B10G10R10_SSCALED_PACK = VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
    COL32_A2B10G10R10_UINT_PACK = VK_FORMAT_A2B10G10R10_UINT_PACK32,
    COL32_A2B10G10R10_SINT_PACK = VK_FORMAT_A2B10G10R10_SINT_PACK32,
    COL32_R16G16_UNORM = VK_FORMAT_R16G16_UNORM,
    COL32_R16G16_SNORM = VK_FORMAT_R16G16_SNORM,
    COL32_R16G16_USCALED = VK_FORMAT_R16G16_USCALED,
    COL32_R16G16_SSCALED = VK_FORMAT_R16G16_SSCALED,
    COL32_R16G16_UINT = VK_FORMAT_R16G16_UINT,
    COL32_R16G16_SINT = VK_FORMAT_R16G16_SINT,
    COL32_R16G16_SFLOAT = VK_FORMAT_R16G16_SFLOAT,
    COL32_R32_UINT = VK_FORMAT_R32_UINT,
    COL32_R32_SINT = VK_FORMAT_R32_SINT,
    COL32_R32_SFLOAT = VK_FORMAT_R32_SFLOAT,
    COL32_B10G11R11_UFLOAT_PACK = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
    COL32_E5B9G9R9_UFLOAT_PACK = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

    // 48-bit color formats
    COL48_R16G16B16_UNORM = VK_FORMAT_R16G16B16_UNORM,
    COL48_R16G16B16_SNORM = VK_FORMAT_R16G16B16_SNORM,
    COL48_R16G16B16_USCALED = VK_FORMAT_R16G16B16_USCALED,
    COL48_R16G16B16_SSCALED = VK_FORMAT_R16G16B16_SSCALED,
    COL48_R16G16B16_UINT = VK_FORMAT_R16G16B16_UINT,
    COL48_R16G16B16_SINT = VK_FORMAT_R16G16B16_SINT,
    COL48_R16G16B16_SFLOAT = VK_FORMAT_R16G16B16_SFLOAT,

    // 64-bit color formats
    COL64_R16G16B16A16_UNORM = VK_FORMAT_R16G16B16A16_UNORM,
    COL64_R16G16B16A16_SNORM = VK_FORMAT_R16G16B16A16_SNORM,
    COL64_R16G16B16A16_USCALED = VK_FORMAT_R16G16B16A16_USCALED,
    COL64_R16G16B16A16_SSCALED = VK_FORMAT_R16G16B16A16_SSCALED,
    COL64_R16G16B16A16_UINT = VK_FORMAT_R16G16B16A16_UINT,
    COL64_R16G16B16A16_SINT = VK_FORMAT_R16G16B16A16_SINT,
    COL64_R16G16B16A16_SFLOAT = VK_FORMAT_R16G16B16A16_SFLOAT,
    COL64_R32G32_UINT = VK_FORMAT_R32G32_UINT,
    COL64_R32G32_SINT = VK_FORMAT_R32G32_SINT,
    COL64_R32G32_SFLOAT = VK_FORMAT_R32G32_SFLOAT,
    COL64_R64_UINT = VK_FORMAT_R64_UINT,
    COL64_R64_SINT = VK_FORMAT_R64_SINT,
    COL64_R64_SFLOAT = VK_FORMAT_R64_SFLOAT,

    // 96-bit color formats
    COL96_R32G32B32_UINT = VK_FORMAT_R32G32B32_UINT,
    COL96_R32G32B32_SINT = VK_FORMAT_R32G32B32_SINT,
    COL96_R32G32B32_SFLOAT = VK_FORMAT_R32G32B32_SFLOAT,

    // 128-bit color formats
    COL128_R32G32B32A32_UINT = VK_FORMAT_R32G32B32A32_UINT,
    COL128_R32G32B32A32_SINT = VK_FORMAT_R32G32B32A32_SINT,
    COL128_R32G32B32A32_SFLOAT = VK_FORMAT_R32G32B32A32_SFLOAT,
    COL128_R64G64_UINT = VK_FORMAT_R64G64_UINT,
    COL128_R64G64_SINT = VK_FORMAT_R64G64_SINT,
    COL128_R64G64_SFLOAT = VK_FORMAT_R64G64_SFLOAT,

    // 192-bit color formats
    COL192_R64G64B64_UINT = VK_FORMAT_R64G64B64_UINT,
    COL192_R64G64B64_SINT = VK_FORMAT_R64G64B64_SINT,
    COL192_R64G64B64_SFLOAT = VK_FORMAT_R64G64B64_SFLOAT,

    // 256-bit color formats
    COL256_R64G64B64A64_UINT = VK_FORMAT_R64G64B64A64_UINT,
    COL256_R64G64B64A64_SINT = VK_FORMAT_R64G64B64A64_SINT,
    COL256_R64G64B64A64_SFLOAT = VK_FORMAT_R64G64B64A64_SFLOAT,

    // Compressed color formats
    COMP_BC1_RGB_UNORM_BLOCK = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
    COMP_BC1_RGB_SRGB_BLOCK = VK_FORMAT_BC1_RGB_SRGB_BLOCK,
    COMP_BC1_RGBA_UNORM_BLOCK = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
    COMP_BC1_RGBA_SRGB_BLOCK = VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
    COMP_BC2_UNORM_BLOCK = VK_FORMAT_BC2_UNORM_BLOCK,
    COMP_BC2_SRGB_BLOCK = VK_FORMAT_BC2_SRGB_BLOCK,
    COMP_BC3_UNORM_BLOCK = VK_FORMAT_BC3_UNORM_BLOCK,
    COMP_BC3_SRGB_BLOCK = VK_FORMAT_BC3_SRGB_BLOCK,
    COMP_BC4_UNORM_BLOCK = VK_FORMAT_BC4_UNORM_BLOCK,
    COMP_BC4_SNORM_BLOCK = VK_FORMAT_BC4_SNORM_BLOCK,
    COMP_BC5_UNORM_BLOCK = VK_FORMAT_BC5_UNORM_BLOCK,
    COMP_BC5_SNORM_BLOCK = VK_FORMAT_BC5_SNORM_BLOCK,
    COMP_BC6H_UFLOAT_BLOCK = VK_FORMAT_BC6H_UFLOAT_BLOCK,
    COMP_BC6H_SFLOAT_BLOCK = VK_FORMAT_BC6H_SFLOAT_BLOCK,
    COMP_BC7_UNORM_BLOCK = VK_FORMAT_BC7_UNORM_BLOCK,
    COMP_BC7_SRGB_BLOCK = VK_FORMAT_BC7_SRGB_BLOCK,
    COMP_ETC2_R8G8B8_UNORM_BLOCK = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
    COMP_ETC2_R8G8B8_SRGB_BLOCK = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
    COMP_ETC2_R8G8B8A1_UNORM_BLOCK = VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
    COMP_ETC2_R8G8B8A1_SRGB_BLOCK = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
    COMP_ETC2_EAC_R8G8B8A8_UNORM_BLOCK = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
    COMP_ETC2_EAC_R8G8B8A8_SRGB_BLOCK = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
    COMP_EAC_R11_UNORM_BLOCK = VK_FORMAT_EAC_R11_UNORM_BLOCK,
    COMP_EAC_R11_SNORM_BLOCK = VK_FORMAT_EAC_R11_SNORM_BLOCK,
    COMP_EAC_R11G11_UNORM_BLOCK = VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
    COMP_EAC_R11G11_SNORM_BLOCK = VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
    COMP_ASTC_4x4_UNORM_BLOCK = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
    COMP_ASTC_4x4_SRGB_BLOCK = VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
    COMP_ASTC_5x4_UNORM_BLOCK = VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
    COMP_ASTC_5x4_SRGB_BLOCK = VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
    COMP_ASTC_5x5_UNORM_BLOCK = VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
    COMP_ASTC_5x5_SRGB_BLOCK = VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
    COMP_ASTC_6x5_UNORM_BLOCK = VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
    COMP_ASTC_6x5_SRGB_BLOCK = VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
    COMP_ASTC_6x6_UNORM_BLOCK = VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
    COMP_ASTC_6x6_SRGB_BLOCK = VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
    COMP_ASTC_8x5_UNORM_BLOCK = VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
    COMP_ASTC_8x5_SRGB_BLOCK = VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
    COMP_ASTC_8x6_UNORM_BLOCK = VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
    COMP_ASTC_8x6_SRGB_BLOCK = VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
    COMP_ASTC_8x8_UNORM_BLOCK = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
    COMP_ASTC_8x8_SRGB_BLOCK = VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
    COMP_ASTC_10x5_UNORM_BLOCK = VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
    COMP_ASTC_10x5_SRGB_BLOCK = VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
    COMP_ASTC_10x6_UNORM_BLOCK = VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
    COMP_ASTC_10x6_SRGB_BLOCK = VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
    COMP_ASTC_10x8_UNORM_BLOCK = VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
    COMP_ASTC_10x8_SRGB_BLOCK = VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
    COMP_ASTC_10x10_UNORM_BLOCK = VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
    COMP_ASTC_10x10_SRGB_BLOCK = VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
    COMP_ASTC_12x10_UNORM_BLOCK = VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
    COMP_ASTC_12x10_SRGB_BLOCK = VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
    COMP_ASTC_12x12_UNORM_BLOCK = VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
    COMP_ASTC_12x12_SRGB_BLOCK = VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

    // Depth formats
    DEPTH16_D16_UNORM = VK_FORMAT_D16_UNORM,
    DEPTH24_X8_D24_UNORM_PACK = VK_FORMAT_X8_D24_UNORM_PACK32,
    DEPTH32_D32_SFLOAT = VK_FORMAT_D32_SFLOAT,

    // Stencil formats
    STC8_S8_UINT = VK_FORMAT_S8_UINT,

    // Depth & stencil formats
    DEPTHSTC24_D16_UNORM_S8_UINT = VK_FORMAT_D16_UNORM_S8_UINT,
    DEPTHSTC32_D24_UNORM_S8_UINT = VK_FORMAT_D24_UNORM_S8_UINT,
    DEPTHSTC48_D32_SFLOAT_S8_UINT = VK_FORMAT_D32_SFLOAT_S8_UINT,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(Format, VkFormat);

/// The severity of a reported debug message.
/// @see @vksymbol{VkDebugUtilsMessageSeverityFlagBitsEXT}
enum class DebugMessageSeverity : uint32_t {
    Verbose = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
    Information = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
    Warning = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
    Error = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(DebugMessageSeverity, VkDebugUtilsMessageSeverityFlagBitsEXT);
TEPHRA_MAKE_ENUM_BIT_MASK(DebugMessageSeverityMask, DebugMessageSeverity);

/// The type of a reported debug message.
/// @see @vksymbol{VkDebugUtilsMessageTypeFlagBitsEXT}
enum class DebugMessageType : uint32_t {
    General = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
    Validation = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
    Performance = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
};
TEPHRA_VULKAN_COMPATIBLE_ENUM(DebugMessageType, VkDebugUtilsMessageTypeFlagBitsEXT);
TEPHRA_MAKE_ENUM_BIT_MASK(DebugMessageTypeMask, DebugMessageType);

}
