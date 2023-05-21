#pragma once

namespace tp {

/// Describes the texture filtering to be used by the sampler.
struct SamplerFiltering {
    Filter minFilter;
    Filter magFilter;
    Filter mipmapFilter;

    /// @param minMagFilter
    ///     The filtering to use for minification and magnification.
    /// @param mipmapFilter
    ///     The filtering to use for mipmap levels.
    SamplerFiltering(Filter minMagFilter, Filter mipmapFilter)
        : SamplerFiltering(minMagFilter, minMagFilter, mipmapFilter) {}

    /// @param minFilter
    ///     The filtering to use for minification.
    /// @param magFilter
    ///     The filtering to use for magnification.
    /// @param mipmapFilter
    ///     The filtering to use for mipmap levels.
    SamplerFiltering(Filter minFilter, Filter magFilter, Filter mipmapFilter)
        : minFilter(minFilter), magFilter(magFilter), mipmapFilter(mipmapFilter) {}
};

/// Describes the addressing mode to be used by the sampler.
struct SamplerAddressing {
    SamplerAddressMode addressModeU;
    SamplerAddressMode addressModeV;
    SamplerAddressMode addressModeW;
    BorderColor borderColor;

    /// @param addressModeUVW
    ///     The addressing mode to use for UVW coordinates.
    /// @param borderColor
    ///     The border color to use for the relevant addressing modes.
    SamplerAddressing(SamplerAddressMode addressModeUVW, BorderColor borderColor = BorderColor::FloatTransparentBlack)
        : SamplerAddressing(addressModeUVW, addressModeUVW, addressModeUVW, borderColor) {}

    /// @param addressModeU
    ///     The addressing mode to use for the U coordinate.
    /// @param addressModeV
    ///     The addressing mode to use for the V coordinate.
    /// @param addressModeW
    ///     The addressing mode to use for the W coordinate.
    /// @param borderColor
    ///     The border color to use for the relevant addressing modes.
    SamplerAddressing(
        SamplerAddressMode addressModeU,
        SamplerAddressMode addressModeV,
        SamplerAddressMode addressModeW,
        BorderColor borderColor = BorderColor::FloatTransparentBlack)
        : addressModeU(addressModeU),
          addressModeV(addressModeV),
          addressModeW(addressModeW),
          borderColor(borderColor) {}
};

/// @see tp::Device::createSampler
/// @see @vksymbol{VkSamplerCreateInfo}
struct SamplerSetup {
    SamplerFiltering filtering;
    SamplerAddressing addressing;
    float maxAnisotropy;
    float minMipLod;
    float maxMipLod;
    float mipLodBias;
    bool compareEnable;
    CompareOp compareOp;
    bool unnormalizedCoordinates;

    /// @param filtering
    ///     The filtering modes for the sampler lookup.
    /// @param addressing
    ///     The addressing modes for the sampler lookup.
    /// @param maxAnisotropy
    ///     The maximum anisotropy level to use. Anisotropic filtering will be enabled with value > 1.
    /// @param minMipLod
    ///     Clamps the mipmap LOD value to the given minimum.
    /// @param maxMipLod
    ///     Clamps the mipmap LOD value to the given maximum.
    /// @param mipLodBias
    ///     The bias to be added to the mipmap LOD calculation.
    /// @param compareEnable
    ///     If `true`, enables comparison against a reference value during lookups.
    /// @param compareOp
    ///     The comparison operator to use if `compareEnable` is `true`.
    /// @param unnormalizedCoordinates
    ///     If `true`, the range of coordinates used for lookups will span the actual dimensions of the image,
    ///     rather than from 0 to 1.
    SamplerSetup(
        SamplerFiltering filtering,
        SamplerAddressing addressing,
        float maxAnisotropy = 1.0f,
        float minMipLod = 0.0f,
        float maxMipLod = VK_LOD_CLAMP_NONE,
        float mipLodBias = 0.0f,
        bool compareEnable = false,
        CompareOp compareOp = CompareOp::Never,
        bool unnormalizedCoordinates = false)
        : filtering(filtering),
          addressing(addressing),
          maxAnisotropy(maxAnisotropy),
          minMipLod(minMipLod),
          maxMipLod(maxMipLod),
          mipLodBias(mipLodBias),
          compareEnable(compareEnable),
          compareOp(compareOp),
          unnormalizedCoordinates(unnormalizedCoordinates) {}
};

/// Sampler objects are used to apply filtering and other transformations to image data when accessed
/// from shaders.
/// @see tp::Device::createSampler
/// @see @vksymbol{VkSampler}
class Sampler {
public:
    /// Creates a null sampler.
    Sampler() {}

    Sampler(Lifeguard<VkSamplerHandle>&& samplerHandle) : samplerHandle(std::move(samplerHandle)) {}

    /// Returns `true` if the sampler is null and not valid for use.
    bool isNull() const {
        return samplerHandle.isNull();
    }

    /// Returns the Vulkan @vksymbol{VkSampler} handle.
    VkSamplerHandle vkGetSamplerHandle() const {
        return samplerHandle.vkGetHandle();
    }

private:
    Lifeguard<VkSamplerHandle> samplerHandle;
};

}
