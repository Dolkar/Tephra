#pragma once

#include <tephra/vulkan/enums.hpp>
#include <tephra/vulkan/handles.hpp>
#include <tephra/macros.hpp>
#include <cstdint>
#include <cstddef>

namespace tp {

/// The usage of a resource using a particular format.
/// @see @vksymbol{VkFormatFeatureFlagBits}
enum class FormatUsage : uint32_t {
    /// Corresponds to usage through tp::DescriptorType::SampledImage or tp::DescriptorType::CombinedImageSampler.
    SampledImage = 1 << 0,
    /// Corresponds to usage through tp::DescriptorType::StorageImage.
    StorageImage = 1 << 1,
    /// Corresponds to usage through tp::ColorAttachment.
    ColorAttachment = 1 << 2,
    /// Corresponds to usage through tp::DepthStencilAttachment.
    DepthStencilAttachment = 1 << 3,
    /// Corresponds to usage through tp::DescriptorType::TexelBuffer.
    TexelBuffer = 1 << 4,
    /// Corresponds to usage through tp::DescriptorType::StorageTexelBuffer.
    StorageTexelBuffer = 1 << 5,
    /// Corresponds to usage in tp::VertexInputAttribute.
    VertexBuffer = 1 << 6
};
TEPHRA_MAKE_ENUM_BIT_MASK(FormatUsageMask, FormatUsage);

/// Describes additional features of a particular format.
/// @see @vksymbol{VkFormatFeatureFlagBits}
enum class FormatFeature : uint32_t {
    /// Describes if a format supports atomic operations of storage images or texel buffers.
    AtomicOperations = 1 << 0,
    /// Describes if a format supports linear filtering through tp::Filter::Linear.
    LinearFiltering = 1 << 1,
};
TEPHRA_MAKE_ENUM_BIT_MASK(FormatFeatureMask, FormatFeature);

/// Represents the set of capabilities supported by a device for a particular format.
struct FormatCapabilities {
    /// The set of usages that are supported for a format.
    FormatUsageMask usageMask;
    /// The set of features that are supported for a format.
    FormatFeatureMask featureMask;

    /// @param usageMask
    ///     The usage mask of the capabilities.
    /// @param featureMask
    ///     The feature mask of the capabilities.
    constexpr FormatCapabilities(
        FormatUsageMask usageMask = FormatUsageMask::None(),
        FormatFeatureMask featureMask = FormatFeatureMask::None())
        : usageMask(usageMask), featureMask(featureMask) {}

    /// Returns `true` if this describes a subset of the capabilities of `other`.
    constexpr bool isSubsetOf(const FormatCapabilities& other) const {
        return other.usageMask.containsAll(usageMask) && other.featureMask.containsAll(featureMask);
    }

    /// Returns `true` if this describes a superset of the capabilities of `other`.
    constexpr bool isSupersetOf(const FormatCapabilities& other) const {
        return other.isSubsetOf(*this);
    }
};

constexpr bool operator==(const FormatCapabilities& lhs, const FormatCapabilities& rhs) {
    return lhs.usageMask == rhs.usageMask && lhs.featureMask == rhs.featureMask;
}

constexpr bool operator!=(const FormatCapabilities& lhs, const FormatCapabilities& rhs) {
    return lhs.usageMask != rhs.usageMask || lhs.featureMask != rhs.featureMask;
}

}
