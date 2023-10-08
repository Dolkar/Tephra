#pragma once

#include <tephra/vulkan/enums.hpp>
#include <tephra/vulkan/header.hpp>
#include <cstdint>
#include <cstddef>

namespace tp {

// --- Conversion functionality ---

template <typename T>
struct VkConvertedStructType {};

template <typename T>
using VkConvertedStructType_t = typename VkConvertedStructType<T>::type;

template <typename T>
constexpr const VkConvertedStructType_t<T>& vkCastConvertibleStruct(const T& value) noexcept {
    static_assert(
        sizeof(T) == sizeof(VkConvertedStructType_t<T>), "Tephra and Vulkan convertible struct sizes not matching");

    return *reinterpret_cast<const VkConvertedStructType_t<T>*>(&value);
}

template <typename T>
constexpr const VkConvertedStructType_t<T>* vkCastConvertibleStructPtr(const T* ptr) noexcept {
    static_assert(
        sizeof(T) == sizeof(VkConvertedStructType_t<T>), "Tephra and Vulkan convertible struct sizes not matching");

    return reinterpret_cast<const VkConvertedStructType_t<T>*>(ptr);
}

/// Adds only a one way conversion function from Tephra struct to Vulkan struct
#define TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(tephraStructType, vulkanStructType) \
    template <> \
    struct VkConvertedStructType<tephraStructType> { \
        using type = vulkanStructType; \
    }

#define TEPHRA_VULKAN_INHERITED_STRUCT(structName, vulkanStructName) \
    static_assert( \
        sizeof(structName) == sizeof(vulkanStructName), "Tephra and Vulkan inherited struct sizes not matching"); \
    static_assert( \
        std::is_base_of<vulkanStructName, structName>::value, \
        "Tephra and Vulkan structs declared as inherited are not actually inherited")

#define TEPHRA_MAKE_BASE_CONSTRUCTIBLE(structName, baseName) \
    structName(const baseName& base) : baseName(base) {} \
    structName& operator=(const baseName& base) { \
        *static_cast<baseName*>(this) = base; \
        return *this; \
    }

// --- Types equivalent to Vulkan types, just with a C++ interface ---

/// A two-dimensional integer extent structure.
/// @see @vksymbol{VkExtent2D}
struct Extent2D : VkExtent2D {
    Extent2D() : Extent2D(0, 0) {}

    explicit Extent2D(const uint32_t values[2]) : Extent2D(values[0], values[1]) {}

    Extent2D(uint32_t width, uint32_t height) {
        this->width = width;
        this->height = height;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Extent2D, VkExtent2D);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Extent2D, VkExtent2D);

inline bool operator==(const Extent2D& lhs, const Extent2D& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height;
}
inline bool operator!=(const Extent2D& lhs, const Extent2D& rhs) {
    return !(lhs == rhs);
}

/// A three-dimensional integer extent structure.
/// @see @vksymbol{VkExtent3D}
struct Extent3D : VkExtent3D {
    Extent3D() : Extent3D(0, 0, 0) {}

    explicit Extent3D(const uint32_t values[3]) : Extent3D(values[0], values[1], values[2]) {}

    Extent3D(uint32_t width, uint32_t height, uint32_t depth) {
        this->width = width;
        this->height = height;
        this->depth = depth;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Extent3D, VkExtent3D);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Extent3D, VkExtent3D);

inline bool operator==(const Extent3D& lhs, const Extent3D& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
}
inline bool operator!=(const Extent3D& lhs, const Extent3D& rhs) {
    return !(lhs == rhs);
}

/// A two-dimensional integer offset structure.
/// @see @vksymbol{VkOffset2D}
struct Offset2D : VkOffset2D {
    Offset2D() : Offset2D(0, 0) {}

    explicit Offset2D(const int32_t values[2]) : Offset2D(values[0], values[1]) {}

    Offset2D(Extent2D extent) : Offset2D(static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height)) {}

    Offset2D(int32_t x, int32_t y) {
        this->x = x;
        this->y = y;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Offset2D, VkOffset2D);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Offset2D, VkOffset2D);

inline bool operator==(const Offset2D& lhs, const Offset2D& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
inline bool operator!=(const Offset2D& lhs, const Offset2D& rhs) {
    return !(lhs == rhs);
}

/// A three-dimensional integer offset structure.
/// @see @vksymbol{VkOffset3D}
struct Offset3D : VkOffset3D {
    Offset3D() : Offset3D(0, 0, 0) {}

    explicit Offset3D(const int32_t values[3]) : Offset3D(values[0], values[1], values[2]) {}

    Offset3D(Extent3D extent)
        : Offset3D(
              static_cast<int32_t>(extent.width),
              static_cast<int32_t>(extent.height),
              static_cast<int32_t>(extent.depth)) {}

    Offset3D(int32_t x, int32_t y, int32_t z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Offset3D, VkOffset3D);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Offset3D, VkOffset3D);

inline bool operator==(const Offset3D& lhs, const Offset3D& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}
inline bool operator!=(const Offset3D& lhs, const Offset3D& rhs) {
    return !(lhs == rhs);
}

/// A two-dimensional integer range.
/// @see @vksymbol{VkRect2D}
struct Rect2D : VkRect2D {
    Rect2D() : Rect2D({}, {}) {}

    Rect2D(Offset2D offset, Extent2D extent) {
        this->offset = offset;
        this->extent = extent;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Rect2D, VkRect2D);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Rect2D, VkRect2D);

inline bool operator==(const Rect2D& lhs, const Rect2D& rhs) {
    return lhs.offset == rhs.offset && lhs.extent == rhs.extent;
}
inline bool operator!=(const Rect2D& lhs, const Rect2D& rhs) {
    return !(lhs == rhs);
}

/// Describes a region of a buffer copy operation.
/// @see @vksymbol{VkBufferCopy}
struct BufferCopyRegion : VkBufferCopy {
    BufferCopyRegion() : BufferCopyRegion(0, 0, 0) {}

    BufferCopyRegion(uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
        this->srcOffset = srcOffset;
        this->dstOffset = dstOffset;
        this->size = size;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(BufferCopyRegion, VkBufferCopy);
};
TEPHRA_VULKAN_INHERITED_STRUCT(BufferCopyRegion, VkBufferCopy);

/// Describes a range of push constants.
/// @see @vksymbol{VkPushConstantRange}
struct PushConstantRange : VkPushConstantRange {
    PushConstantRange(ShaderStageMask stageMask, uint32_t offset, uint32_t size) {
        this->stageFlags = vkCastConvertibleEnumMask(stageMask);
        this->offset = offset;
        this->size = size;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(PushConstantRange, VkPushConstantRange);
};
TEPHRA_VULKAN_INHERITED_STRUCT(PushConstantRange, VkPushConstantRange);

/// The viewport describing a region of render operations.
/// @see @vksymbol{VkViewport}
struct Viewport : VkViewport {
    Viewport() : Viewport(0, 0, 0, 0) {}

    explicit Viewport(const Rect2D& rect)
        : Viewport(
              static_cast<float>(rect.offset.x),
              static_cast<float>(rect.offset.y),
              static_cast<float>(rect.extent.width),
              static_cast<float>(rect.extent.height)) {}

    Viewport(const Rect2D& rect, float minDepth, float maxDepth)
        : Viewport(
              static_cast<float>(rect.offset.x),
              static_cast<float>(rect.offset.y),
              static_cast<float>(rect.extent.width),
              static_cast<float>(rect.extent.height),
              minDepth,
              maxDepth) {}

    Viewport(float x, float y, float width, float height) : Viewport(x, y, width, height, 0.0, 1.0) {}

    Viewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
        this->minDepth = minDepth;
        this->maxDepth = maxDepth;
    }

    TEPHRA_MAKE_BASE_CONSTRUCTIBLE(Viewport, VkViewport);
};
TEPHRA_VULKAN_INHERITED_STRUCT(Viewport, VkViewport);

// --- Types convertible to Vulkan types, but not directly equivalent ---

/// Specifies the values placed in each component of the output color vector.
/// @see @vksymbol{VkComponentMapping}
struct ComponentMapping {
    ComponentSwizzle r = ComponentSwizzle::Identity;
    ComponentSwizzle g = ComponentSwizzle::Identity;
    ComponentSwizzle b = ComponentSwizzle::Identity;
    ComponentSwizzle a = ComponentSwizzle::Identity;

    ComponentMapping() {}

    ComponentMapping(ComponentSwizzle r, ComponentSwizzle g, ComponentSwizzle b, ComponentSwizzle a)
        : r(r), g(g), b(b), a(a) {}
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ComponentMapping, VkComponentMapping);

inline bool operator==(const ComponentMapping& lhs, const ComponentMapping& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}
inline bool operator!=(const ComponentMapping& lhs, const ComponentMapping& rhs) {
    return !(lhs == rhs);
}

/// Describes a subresource of an image containing a single array layer and mip level.
/// @see @vksymbol{VkImageSubresource}
struct ImageSubresource {
    ImageAspectMask aspectMask;
    uint32_t mipLevel;
    uint32_t arrayLayer;

    ImageSubresource() : ImageSubresource(ImageAspectMask::None(), 0, 0) {}

    ImageSubresource(ImageAspectMask aspectMask, uint32_t mipLevel, uint32_t arrayLayer)
        : aspectMask(aspectMask), mipLevel(mipLevel), arrayLayer(arrayLayer) {}

    /// Returns a subresource of the given aspect.
    ImageSubresource pickAspect(ImageAspect aspect) const {
        return ImageSubresource(aspect, mipLevel, arrayLayer);
    }
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ImageSubresource, VkImageSubresource);

inline bool operator==(const ImageSubresource& lhs, const ImageSubresource& rhs) {
    return lhs.aspectMask == rhs.aspectMask && lhs.mipLevel == rhs.mipLevel && lhs.arrayLayer == rhs.arrayLayer;
}
inline bool operator!=(const ImageSubresource& lhs, const ImageSubresource& rhs) {
    return !(lhs == rhs);
}

/// Describes a subresource of an image containing any number of array layers and a single mip level.
/// @see @vksymbol{VkImageSubresourceLayers}
struct ImageSubresourceLayers {
    ImageAspectMask aspectMask;
    uint32_t mipLevel;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;

    ImageSubresourceLayers() : ImageSubresourceLayers(ImageAspectMask::None(), 0, 0, 0) {}

    ImageSubresourceLayers(const ImageSubresource& subresource)
        : ImageSubresourceLayers(subresource.aspectMask, subresource.mipLevel, subresource.arrayLayer, 1) {}

    ImageSubresourceLayers(
        ImageAspectMask aspectMask,
        uint32_t mipLevel,
        uint32_t baseArrayLayer,
        uint32_t arrayLayerCount)
        : aspectMask(aspectMask),
          mipLevel(mipLevel),
          baseArrayLayer(baseArrayLayer),
          arrayLayerCount(arrayLayerCount) {}

    /// Returns a subresource of the given aspect.
    ImageSubresourceLayers pickAspect(ImageAspect aspect) const {
        return ImageSubresourceLayers(aspect, mipLevel, baseArrayLayer, arrayLayerCount);
    }

    /// Returns a subresource of the given layer, relative to this subresource.
    ImageSubresource pickLayer(uint32_t arrayLayerOffset) const {
        return ImageSubresource(aspectMask, mipLevel, baseArrayLayer + arrayLayerOffset);
    }

    /// Returns a subresource of the given layer range, relative to this subresource.
    ImageSubresourceLayers pickLayers(uint32_t arrayLayerOffset, uint32_t arrayLayerCount) const {
        return ImageSubresourceLayers(aspectMask, mipLevel, baseArrayLayer + arrayLayerOffset, arrayLayerCount);
    }
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ImageSubresourceLayers, VkImageSubresourceLayers);

inline bool operator==(const ImageSubresourceLayers& lhs, const ImageSubresourceLayers& rhs) {
    return lhs.aspectMask == rhs.aspectMask && lhs.mipLevel == rhs.mipLevel &&
        lhs.baseArrayLayer == rhs.baseArrayLayer && lhs.arrayLayerCount == rhs.arrayLayerCount;
}
inline bool operator!=(const ImageSubresourceLayers& lhs, const ImageSubresourceLayers& rhs) {
    return !(lhs == rhs);
}

/// Describes a subresource of an image containing any number of array layers and mip levels.
/// @see @vksymbol{VkImageSubresourceRange}
struct ImageSubresourceRange {
    ImageAspectMask aspectMask;
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;

    ImageSubresourceRange() : ImageSubresourceRange(ImageAspectMask::None(), 0, 0, 0, 0) {}

    ImageSubresourceRange(const ImageSubresource& subresource)
        : ImageSubresourceRange(subresource.aspectMask, subresource.mipLevel, 1, subresource.arrayLayer, 1) {}

    ImageSubresourceRange(const ImageSubresourceLayers& subresourceLayers)
        : ImageSubresourceRange(
              subresourceLayers.aspectMask,
              subresourceLayers.mipLevel,
              1,
              subresourceLayers.baseArrayLayer,
              subresourceLayers.arrayLayerCount) {}

    ImageSubresourceRange(
        ImageAspectMask aspectMask,
        uint32_t baseMipLevel,
        uint32_t mipLevelCount,
        uint32_t baseArrayLayer,
        uint32_t arrayLayerCount)
        : aspectMask(aspectMask),
          baseMipLevel(baseMipLevel),
          mipLevelCount(mipLevelCount),
          baseArrayLayer(baseArrayLayer),
          arrayLayerCount(arrayLayerCount) {}

    /// Returns a subresource of the given aspect.
    ImageSubresourceRange pickAspect(ImageAspect aspect) const {
        return ImageSubresourceRange(aspect, baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount);
    }

    /// Returns a subresource of the given layer, relative to this subresource.
    ImageSubresourceRange pickLayer(uint32_t arrayLayerOffset) const {
        return pickLayers(arrayLayerOffset, 1);
    }

    /// Returns a subresource of the given layer range, relative to this subresource.
    ImageSubresourceRange pickLayers(uint32_t arrayLayerOffset, uint32_t arrayLayerCount) const {
        return ImageSubresourceRange(
            aspectMask, baseMipLevel, mipLevelCount, baseArrayLayer + arrayLayerOffset, arrayLayerCount);
    }

    /// Returns a subresource of the given mip level, relative to this subresource.
    ImageSubresourceLayers pickMipLevel(uint32_t mipLevelOffset) const {
        return ImageSubresourceLayers(aspectMask, baseMipLevel + mipLevelOffset, baseArrayLayer, arrayLayerCount);
    }

    /// Returns a subresource of the given mip level range, relative to this subresource.
    ImageSubresourceRange pickMipLevels(uint32_t mipLevelOffset, uint32_t mipLevelCount) const {
        return ImageSubresourceRange(
            aspectMask, baseMipLevel + mipLevelOffset, mipLevelCount, baseArrayLayer, arrayLayerCount);
    }
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ImageSubresourceRange, VkImageSubresourceRange);

inline bool operator==(const ImageSubresourceRange& lhs, const ImageSubresourceRange& rhs) {
    return lhs.aspectMask == rhs.aspectMask && lhs.baseMipLevel == rhs.baseMipLevel &&
        lhs.mipLevelCount == rhs.mipLevelCount && lhs.baseArrayLayer == rhs.baseArrayLayer &&
        lhs.arrayLayerCount == rhs.arrayLayerCount;
}
inline bool operator!=(const ImageSubresourceRange& lhs, const ImageSubresourceRange& rhs) {
    return !(lhs == rhs);
}

/// Describes a region of an image copy operation.
/// @see @vksymbol{VkImageCopy}
struct ImageCopyRegion {
    ImageSubresourceLayers srcSubresource;
    Offset3D srcOffset;
    ImageSubresourceLayers dstSubresource;
    Offset3D dstOffset;
    Extent3D extent;

    ImageCopyRegion() {}

    ImageCopyRegion(
        ImageSubresourceLayers srcSubresource,
        Offset3D srcOffset,
        ImageSubresourceLayers dstSubresource,
        Offset3D dstOffset,
        Extent3D extent)
        : srcSubresource(srcSubresource),
          srcOffset(srcOffset),
          dstSubresource(dstSubresource),
          dstOffset(dstOffset),
          extent(extent) {}
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ImageCopyRegion, VkImageCopy);

/// Describes a region of copy operation between an image and a buffer.
/// @see @vksymbol{VkBufferImageCopy}
struct BufferImageCopyRegion {
    uint64_t bufferOffset;
    uint32_t bufferRowLength;
    uint32_t bufferImageHeight;
    ImageSubresourceLayers imageSubresource;
    Offset3D imageOffset;
    Extent3D imageExtent;

    BufferImageCopyRegion(
        uint64_t bufferOffset,
        ImageSubresourceLayers imageSubresource,
        Offset3D imageOffset,
        Extent3D imageExtent,
        uint32_t bufferRowLength = 0,
        uint32_t bufferImageHeight = 0)
        : bufferOffset(bufferOffset),
          bufferRowLength(bufferRowLength),
          bufferImageHeight(bufferImageHeight),
          imageSubresource(imageSubresource),
          imageOffset(imageOffset),
          imageExtent(imageExtent) {}
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(BufferImageCopyRegion, VkBufferImageCopy);

/// Describes a region of an image blit operation.
/// @see @vksymbol{VkImageBlit}
struct ImageBlitRegion {
    ImageSubresourceLayers srcSubresource;
    Offset3D srcOffsetBegin;
    Offset3D srcOffsetEnd;
    ImageSubresourceLayers dstSubresource;
    Offset3D dstOffsetBegin;
    Offset3D dstOffsetEnd;

    ImageBlitRegion(
        ImageSubresourceLayers srcSubresource,
        Offset3D srcOffsetBegin,
        Offset3D srcOffsetEnd,
        ImageSubresourceLayers dstSubresource,
        Offset3D dstOffsetBegin,
        Offset3D dstOffsetEnd)
        : srcSubresource(srcSubresource),
          srcOffsetBegin(srcOffsetBegin),
          srcOffsetEnd(srcOffsetEnd),
          dstSubresource(dstSubresource),
          dstOffsetBegin(dstOffsetBegin),
          dstOffsetEnd(dstOffsetEnd) {}
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ImageBlitRegion, VkImageBlit);

/// Specifies a constant clear value.
/// @see @vksymbol{VkClearValue}
struct ClearValue {
    VkClearValue vkValue;

    /// Creates a color value to be used for formats of types other than `UINT` and `SINT`.
    static ClearValue ColorFloat(const float values[4]) {
        return ColorFloat(values[0], values[1], values[2], values[3]);
    }

    /// Creates a color value to be used for formats of types other than `UINT` and `SINT`.
    static ClearValue ColorFloat(float red, float green, float blue, float alpha) {
        ClearValue v;
        v.vkValue.color.float32[0] = red;
        v.vkValue.color.float32[1] = green;
        v.vkValue.color.float32[2] = blue;
        v.vkValue.color.float32[3] = alpha;
        return v;
    }

    /// Creates a color value to be used for formats of the `SINT` type.
    static ClearValue ColorSInt(const int32_t values[4]) {
        return ColorSInt(values[0], values[1], values[2], values[3]);
    }

    /// Creates a color value to be used for formats of the `SINT` type.
    static ClearValue ColorSInt(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
        ClearValue v;
        v.vkValue.color.int32[0] = red;
        v.vkValue.color.int32[1] = green;
        v.vkValue.color.int32[2] = blue;
        v.vkValue.color.int32[3] = alpha;
        return v;
    }

    /// Creates a color value to be used for formats of the `UINT` type.
    static ClearValue ColorUInt(const uint32_t values[4]) {
        return ColorUInt(values[0], values[1], values[2], values[3]);
    }

    /// Creates a color value to be used for formats of the `UINT` type.
    static ClearValue ColorUInt(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha) {
        ClearValue v;
        v.vkValue.color.uint32[0] = red;
        v.vkValue.color.uint32[1] = green;
        v.vkValue.color.uint32[2] = blue;
        v.vkValue.color.uint32[3] = alpha;
        return v;
    }

    /// Creates a depth stencil value to be used for depth and/or stencil formats.
    static ClearValue DepthStencil(float depth, uint32_t stencil) {
        ClearValue v;
        v.vkValue.depthStencil.depth = depth;
        v.vkValue.depthStencil.stencil = stencil;
        return v;
    }
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(ClearValue, VkClearValue);

/// Specifies the full state of a stencil operation.
/// @see @vksymbol{VkStencilOpState}
struct StencilState {
    StencilOp failOp;
    StencilOp passOp;
    StencilOp depthFailOp;
    CompareOp depthCompareOp;
    uint32_t stencilCompareMask;
    uint32_t stencilWriteMask;
    uint32_t stencilReference;

    StencilState() : StencilState(StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, CompareOp::Always, 0, 0, 0) {}

    StencilState(
        StencilOp failOp,
        StencilOp passOp,
        StencilOp depthFailOp,
        CompareOp depthCompareOp,
        uint32_t stencilCompareMask,
        uint32_t stencilWriteMask,
        uint32_t stencilReference)
        : failOp(failOp),
          passOp(passOp),
          depthFailOp(depthFailOp),
          depthCompareOp(depthCompareOp),
          stencilCompareMask(stencilCompareMask),
          stencilWriteMask(stencilWriteMask),
          stencilReference(stencilReference) {}
};
TEPHRA_VULKAN_CONVERTIBLE_TO_VK_STRUCT(StencilState, VkStencilOpState);

using DeviceAddress = VkDeviceAddress;

}
