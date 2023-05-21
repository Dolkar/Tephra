#pragma once

#include <tephra/format.hpp>
#include <tephra/common.hpp>

namespace tp {

/// Defines the distinct format compatibility classes
enum class FormatCompatibilityClass {
    Undefined = 0,
    COL8 = 1,
    COL16 = 2,
    COL24 = 3,
    COL32 = 4,
    COL48 = 5,
    COL64 = 6,
    COL96 = 7,
    COL128 = 8,
    COL192 = 9,
    COL256 = 10,
    COMP_BC1_RGB = 11,
    COMP_BC1_RGBA = 12,
    COMP_BC2 = 13,
    COMP_BC3 = 14,
    COMP_BC4 = 15,
    COMP_BC5 = 16,
    COMP_BC6H = 17,
    COMP_BC7 = 18,
    COMP_ETC2_RGB = 19,
    COMP_ETC2_RGBA = 20,
    COMP_ETC2_EAC_RGBA = 21,
    COMP_EAC_R = 22,
    COMP_EAC_RG = 23,
    COMP_ASTC_4x4 = 24,
    COMP_ASTC_5x4 = 25,
    COMP_ASTC_5x5 = 26,
    COMP_ASTC_6x5 = 27,
    COMP_ASTC_6x6 = 28,
    COMP_ASTC_8x5 = 29,
    COMP_ASTC_8x6 = 30,
    COMP_ASTC_8x8 = 31,
    COMP_ASTC_10x5 = 32,
    COMP_ASTC_10x6 = 33,
    COMP_ASTC_10x8 = 34,
    COMP_ASTC_10x10 = 35,
    COMP_ASTC_12x10 = 36,
    COMP_ASTC_12x12 = 37,
    DEPTH16 = 38,
    DEPTH24 = 39,
    DEPTH32 = 40,
    STC8 = 41,
    DEPTHSTC24 = 42,
    DEPTHSTC32 = 43,
    DEPTHSTC48 = 44
};

/// Returns the tp::FormatCompatibilityClass for the given tp::Format
constexpr FormatCompatibilityClass getFormatCompatibilityClass(Format format) {
    switch (format) {
    case Format::Undefined:
        return FormatCompatibilityClass::Undefined;
    case Format::COL8_R4G4_UNORM_PACK:
    case Format::COL8_R8_UNORM:
    case Format::COL8_R8_SNORM:
    case Format::COL8_R8_USCALED:
    case Format::COL8_R8_SSCALED:
    case Format::COL8_R8_UINT:
    case Format::COL8_R8_SINT:
    case Format::COL8_R8_SRGB:
        return FormatCompatibilityClass::COL8;
    case Format::COL16_R4G4B4A4_UNORM_PACK:
    case Format::COL16_B4G4R4A4_UNORM_PACK:
    case Format::COL16_R5G6B5_UNORM_PACK:
    case Format::COL16_B5G6R5_UNORM_PACK:
    case Format::COL16_R5G5B5A1_UNORM_PACK:
    case Format::COL16_B5G5R5A1_UNORM_PACK:
    case Format::COL16_A1R5G5B5_UNORM_PACK:
    case Format::COL16_R8G8_UNORM:
    case Format::COL16_R8G8_SNORM:
    case Format::COL16_R8G8_USCALED:
    case Format::COL16_R8G8_SSCALED:
    case Format::COL16_R8G8_UINT:
    case Format::COL16_R8G8_SINT:
    case Format::COL16_R8G8_SRGB:
    case Format::COL16_R16_UNORM:
    case Format::COL16_R16_SNORM:
    case Format::COL16_R16_USCALED:
    case Format::COL16_R16_SSCALED:
    case Format::COL16_R16_UINT:
    case Format::COL16_R16_SINT:
    case Format::COL16_R16_SFLOAT:
        return FormatCompatibilityClass::COL16;
    case Format::COL24_R8G8B8_UNORM:
    case Format::COL24_R8G8B8_SNORM:
    case Format::COL24_R8G8B8_USCALED:
    case Format::COL24_R8G8B8_SSCALED:
    case Format::COL24_R8G8B8_UINT:
    case Format::COL24_R8G8B8_SINT:
    case Format::COL24_R8G8B8_SRGB:
    case Format::COL24_B8G8R8_UNORM:
    case Format::COL24_B8G8R8_SNORM:
    case Format::COL24_B8G8R8_USCALED:
    case Format::COL24_B8G8R8_SSCALED:
    case Format::COL24_B8G8R8_UINT:
    case Format::COL24_B8G8R8_SINT:
    case Format::COL24_B8G8R8_SRGB:
        return FormatCompatibilityClass::COL24;
    case Format::COL32_R8G8B8A8_UNORM:
    case Format::COL32_R8G8B8A8_SNORM:
    case Format::COL32_R8G8B8A8_USCALED:
    case Format::COL32_R8G8B8A8_SSCALED:
    case Format::COL32_R8G8B8A8_UINT:
    case Format::COL32_R8G8B8A8_SINT:
    case Format::COL32_R8G8B8A8_SRGB:
    case Format::COL32_B8G8R8A8_UNORM:
    case Format::COL32_B8G8R8A8_SNORM:
    case Format::COL32_B8G8R8A8_USCALED:
    case Format::COL32_B8G8R8A8_SSCALED:
    case Format::COL32_B8G8R8A8_UINT:
    case Format::COL32_B8G8R8A8_SINT:
    case Format::COL32_B8G8R8A8_SRGB:
    case Format::COL32_A8B8G8R8_UNORM_PACK:
    case Format::COL32_A8B8G8R8_SNORM_PACK:
    case Format::COL32_A8B8G8R8_USCALED_PACK:
    case Format::COL32_A8B8G8R8_SSCALED_PACK:
    case Format::COL32_A8B8G8R8_UINT_PACK:
    case Format::COL32_A8B8G8R8_SINT_PACK:
    case Format::COL32_A8B8G8R8_SRGB_PACK:
    case Format::COL32_A2R10G10B10_UNORM_PACK:
    case Format::COL32_A2R10G10B10_SNORM_PACK:
    case Format::COL32_A2R10G10B10_USCALED_PACK:
    case Format::COL32_A2R10G10B10_SSCALED_PACK:
    case Format::COL32_A2R10G10B10_UINT_PACK:
    case Format::COL32_A2R10G10B10_SINT_PACK:
    case Format::COL32_A2B10G10R10_UNORM_PACK:
    case Format::COL32_A2B10G10R10_SNORM_PACK:
    case Format::COL32_A2B10G10R10_USCALED_PACK:
    case Format::COL32_A2B10G10R10_SSCALED_PACK:
    case Format::COL32_A2B10G10R10_UINT_PACK:
    case Format::COL32_A2B10G10R10_SINT_PACK:
    case Format::COL32_R16G16_UNORM:
    case Format::COL32_R16G16_SNORM:
    case Format::COL32_R16G16_USCALED:
    case Format::COL32_R16G16_SSCALED:
    case Format::COL32_R16G16_UINT:
    case Format::COL32_R16G16_SINT:
    case Format::COL32_R16G16_SFLOAT:
    case Format::COL32_R32_UINT:
    case Format::COL32_R32_SINT:
    case Format::COL32_R32_SFLOAT:
    case Format::COL32_B10G11R11_UFLOAT_PACK:
    case Format::COL32_E5B9G9R9_UFLOAT_PACK:
        return FormatCompatibilityClass::COL32;
    case Format::COL48_R16G16B16_UNORM:
    case Format::COL48_R16G16B16_SNORM:
    case Format::COL48_R16G16B16_USCALED:
    case Format::COL48_R16G16B16_SSCALED:
    case Format::COL48_R16G16B16_UINT:
    case Format::COL48_R16G16B16_SINT:
    case Format::COL48_R16G16B16_SFLOAT:
        return FormatCompatibilityClass::COL48;
    case Format::COL64_R16G16B16A16_UNORM:
    case Format::COL64_R16G16B16A16_SNORM:
    case Format::COL64_R16G16B16A16_USCALED:
    case Format::COL64_R16G16B16A16_SSCALED:
    case Format::COL64_R16G16B16A16_UINT:
    case Format::COL64_R16G16B16A16_SINT:
    case Format::COL64_R16G16B16A16_SFLOAT:
    case Format::COL64_R32G32_UINT:
    case Format::COL64_R32G32_SINT:
    case Format::COL64_R32G32_SFLOAT:
    case Format::COL64_R64_UINT:
    case Format::COL64_R64_SINT:
    case Format::COL64_R64_SFLOAT:
        return FormatCompatibilityClass::COL64;
    case Format::COL96_R32G32B32_UINT:
    case Format::COL96_R32G32B32_SINT:
    case Format::COL96_R32G32B32_SFLOAT:
        return FormatCompatibilityClass::COL96;
    case Format::COL128_R32G32B32A32_UINT:
    case Format::COL128_R32G32B32A32_SINT:
    case Format::COL128_R32G32B32A32_SFLOAT:
    case Format::COL128_R64G64_UINT:
    case Format::COL128_R64G64_SINT:
    case Format::COL128_R64G64_SFLOAT:
        return FormatCompatibilityClass::COL128;
    case Format::COL192_R64G64B64_UINT:
    case Format::COL192_R64G64B64_SINT:
    case Format::COL192_R64G64B64_SFLOAT:
        return FormatCompatibilityClass::COL192;
    case Format::COL256_R64G64B64A64_UINT:
    case Format::COL256_R64G64B64A64_SINT:
    case Format::COL256_R64G64B64A64_SFLOAT:
        return FormatCompatibilityClass::COL256;
    case Format::COMP_BC1_RGB_UNORM_BLOCK:
    case Format::COMP_BC1_RGB_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_BC1_RGB;
    case Format::COMP_BC1_RGBA_UNORM_BLOCK:
    case Format::COMP_BC1_RGBA_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_BC1_RGBA;
    case Format::COMP_BC2_UNORM_BLOCK:
    case Format::COMP_BC2_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_BC2;
    case Format::COMP_BC3_UNORM_BLOCK:
    case Format::COMP_BC3_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_BC3;
    case Format::COMP_BC4_UNORM_BLOCK:
    case Format::COMP_BC4_SNORM_BLOCK:
        return FormatCompatibilityClass::COMP_BC4;
    case Format::COMP_BC5_UNORM_BLOCK:
    case Format::COMP_BC5_SNORM_BLOCK:
        return FormatCompatibilityClass::COMP_BC5;
    case Format::COMP_BC6H_UFLOAT_BLOCK:
    case Format::COMP_BC6H_SFLOAT_BLOCK:
        return FormatCompatibilityClass::COMP_BC6H;
    case Format::COMP_BC7_UNORM_BLOCK:
    case Format::COMP_BC7_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_BC7;
    case Format::COMP_ETC2_R8G8B8_UNORM_BLOCK:
    case Format::COMP_ETC2_R8G8B8_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ETC2_RGB;
    case Format::COMP_ETC2_R8G8B8A1_UNORM_BLOCK:
    case Format::COMP_ETC2_R8G8B8A1_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ETC2_RGBA;
    case Format::COMP_ETC2_EAC_R8G8B8A8_UNORM_BLOCK:
    case Format::COMP_ETC2_EAC_R8G8B8A8_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ETC2_EAC_RGBA;
    case Format::COMP_EAC_R11_UNORM_BLOCK:
    case Format::COMP_EAC_R11_SNORM_BLOCK:
        return FormatCompatibilityClass::COMP_EAC_R;
    case Format::COMP_EAC_R11G11_UNORM_BLOCK:
    case Format::COMP_EAC_R11G11_SNORM_BLOCK:
        return FormatCompatibilityClass::COMP_EAC_RG;
    case Format::COMP_ASTC_4x4_UNORM_BLOCK:
    case Format::COMP_ASTC_4x4_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_4x4;
    case Format::COMP_ASTC_5x4_UNORM_BLOCK:
    case Format::COMP_ASTC_5x4_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_5x4;
    case Format::COMP_ASTC_5x5_UNORM_BLOCK:
    case Format::COMP_ASTC_5x5_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_5x5;
    case Format::COMP_ASTC_6x5_UNORM_BLOCK:
    case Format::COMP_ASTC_6x5_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_6x5;
    case Format::COMP_ASTC_6x6_UNORM_BLOCK:
    case Format::COMP_ASTC_6x6_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_6x6;
    case Format::COMP_ASTC_8x5_UNORM_BLOCK:
    case Format::COMP_ASTC_8x5_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_8x5;
    case Format::COMP_ASTC_8x6_UNORM_BLOCK:
    case Format::COMP_ASTC_8x6_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_8x6;
    case Format::COMP_ASTC_8x8_UNORM_BLOCK:
    case Format::COMP_ASTC_8x8_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_8x8;
    case Format::COMP_ASTC_10x5_UNORM_BLOCK:
    case Format::COMP_ASTC_10x5_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_10x5;
    case Format::COMP_ASTC_10x6_UNORM_BLOCK:
    case Format::COMP_ASTC_10x6_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_10x6;
    case Format::COMP_ASTC_10x8_UNORM_BLOCK:
    case Format::COMP_ASTC_10x8_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_10x8;
    case Format::COMP_ASTC_10x10_UNORM_BLOCK:
    case Format::COMP_ASTC_10x10_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_10x10;
    case Format::COMP_ASTC_12x10_UNORM_BLOCK:
    case Format::COMP_ASTC_12x10_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_12x10;
    case Format::COMP_ASTC_12x12_UNORM_BLOCK:
    case Format::COMP_ASTC_12x12_SRGB_BLOCK:
        return FormatCompatibilityClass::COMP_ASTC_12x12;
    case Format::DEPTH16_D16_UNORM:
        return FormatCompatibilityClass::DEPTH16;
    case Format::DEPTH24_X8_D24_UNORM_PACK:
        return FormatCompatibilityClass::DEPTH24;
    case Format::DEPTH32_D32_SFLOAT:
        return FormatCompatibilityClass::DEPTH32;
    case Format::STC8_S8_UINT:
        return FormatCompatibilityClass::STC8;
    case Format::DEPTHSTC24_D16_UNORM_S8_UINT:
        return FormatCompatibilityClass::DEPTHSTC24;
    case Format::DEPTHSTC32_D24_UNORM_S8_UINT:
        return FormatCompatibilityClass::DEPTHSTC32;
    case Format::DEPTHSTC48_D32_SFLOAT_S8_UINT:
        return FormatCompatibilityClass::DEPTHSTC48;
    default:
        return FormatCompatibilityClass::Undefined;
    }
}

/// Describes the properties and memory layout of a tp::FormatCompatibilityClass
struct FormatClassProperties {
    /// The size of each texel block in bytes.
    uint32_t texelBlockBytes;
    /// The number of texels covered by each texel block in the X direction.
    uint32_t texelBlockWidth;
    /// The number of texels covered by each texel block in the Y direction.
    uint32_t texelBlockHeight;
    /// The set of image aspects that the format contains.
    ImageAspectMask aspectMask;
};

/// Returns the tp::FormatClassProperties of a given tp::FormatCompatibilityClass.
constexpr FormatClassProperties getFormatClassProperties(FormatCompatibilityClass formatClass) {
    // clang-format off
    switch (formatClass) {
    case FormatCompatibilityClass::Undefined:               return {  0,  0,  0, ImageAspectMask::None() };
    case FormatCompatibilityClass::COL8:                    return {  1,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL16:                   return {  2,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL24:                   return {  3,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL32:                   return {  4,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL48:                   return {  6,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL64:                   return {  8,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL96:                   return { 12,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL128:                  return { 16,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL192:                  return { 24,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COL256:                  return { 32,  1,  1, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC1_RGB:            return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC1_RGBA:           return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC2:                return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC3:                return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC4:                return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC5:                return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC6H:               return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_BC7:                return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ETC2_RGB:           return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ETC2_RGBA:          return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ETC2_EAC_RGBA:      return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_EAC_R:              return {  8,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_EAC_RG:             return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_4x4:           return { 16,  4,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_5x4:           return { 16,  5,  4, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_5x5:           return { 16,  5,  5, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_6x5:           return { 16,  6,  5, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_6x6:           return { 16,  6,  6, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_8x5:           return { 16,  8,  5, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_8x6:           return { 16,  8,  6, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_8x8:           return { 16,  8,  8, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_10x5:          return { 16, 10,  5, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_10x6:          return { 16, 10,  6, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_10x8:          return { 16, 10,  8, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_10x10:         return { 16, 10, 10, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_12x10:         return { 16, 12, 10, ImageAspect::Color };
    case FormatCompatibilityClass::COMP_ASTC_12x12:         return { 16, 12, 12, ImageAspect::Color };
    case FormatCompatibilityClass::DEPTH16:                 return {  2,  1,  1, ImageAspect::Depth };
    case FormatCompatibilityClass::DEPTH24:                 return {  4,  1,  1, ImageAspect::Depth };
    case FormatCompatibilityClass::DEPTH32:                 return {  4,  1,  1, ImageAspect::Depth };
    case FormatCompatibilityClass::STC8:                    return {  1,  1,  1, ImageAspect::Stencil };
    case FormatCompatibilityClass::DEPTHSTC24:              return {  3,  1,  1, ImageAspect::Depth | ImageAspect::Stencil };
    case FormatCompatibilityClass::DEPTHSTC32:              return {  4,  1,  1, ImageAspect::Depth | ImageAspect::Stencil };
    case FormatCompatibilityClass::DEPTHSTC48:              return {  5,  1,  1, ImageAspect::Depth | ImageAspect::Stencil };
    default:                                                return {  0,  0,  0, ImageAspectMask::None() };
    }
    // clang-format on
}

/// Returns the tp::FormatClassProperties of the format compatibility class that the given tp::Format is a part of.
constexpr FormatClassProperties getFormatClassProperties(Format format) {
    return getFormatClassProperties(getFormatCompatibilityClass(format));
}

}
