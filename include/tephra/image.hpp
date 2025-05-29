#pragma once

#include <tephra/format.hpp>
#include <tephra/memory.hpp>
#include <tephra/common.hpp>
#include <variant>

namespace tp {

/// Specifies the dimensionality of an image, as well as what view types can be created for it.
enum class ImageType {
    /// One-dimensional image. Supports tp::ImageViewType::View1D and tp::ImageViewType::View1DArray views.
    Image1D = 0,
    /// Two-dimensional image. Supports tp::ImageViewType::View2D and tp::ImageViewType::View2DArray views.
    Image2D = 1,
    /// Two-dimensional image with cubemap view compatibility. Supports tp::ImageViewType::View2D,
    /// tp::ImageViewType::View2DArray, tp::ImageViewType::ViewCube and tp::ImageViewType::ViewCubeArray views.
    Image2DCubeCompatible = 2,
    /// Three-dimensional image. Supports tp::ImageViewType::View3D views.
    Image3D = 3,
    /// Three-dimensional image with two-dimensional view compatibility of its slices. Supports
    /// tp::ImageViewType::View3D, tp::ImageViewType::View2D and tp::ImageViewType::View2DArray views.
    Image3D2DArrayCompatible = 4,
};

/// Used as configuration for creating a new tp::ImageView object.
/// @see tp::Image::createView
/// @see tp::ImageView::createView
struct ImageViewSetup {
    ImageViewType viewType;
    ImageSubresourceRange subresourceRange;
    Format format;
    ComponentMapping componentMapping;

    /// @param viewType
    ///     The type of the image view.
    /// @param subresourceRange
    ///     The subresource range of the image to be viewed.
    /// @param format
    ///     The format that the viewed image data will be interpreted as. If it is tp::Format::Undefined,
    ///     the format used will be the same as that of the parent tp::Image or tp::ImageView
    ///     that the view is created from.
    /// @param componentMapping
    ///     An optional component remapping of the view when used in a tp::DescriptorType::SampledImage or
    ///     tp::DescriptorType::CombinedImageSampler descriptor.
    /// @remarks
    ///     The format must be the same as the format of the viewed tp::Image or one of the image's
    ///     compatible formats.
    ImageViewSetup(
        ImageViewType viewType,
        ImageSubresourceRange subresourceRange,
        Format format = Format::Undefined,
        ComponentMapping componentMapping = {});
};

class Image;
class ImageImpl;
class JobLocalImageImpl;

/// Represents a non-owning view of a subresource range of tp::Image.
///
/// Optionally, it can be made to interpret the data in a different format than that of the
/// viewed tp::Image or with a different tp::ComponentMapping.
///
/// @see tp::Image::createView
/// @see tp::ImageView::createView
/// @see tp::Job::allocateLocalImage
/// @see @vksymbol{VkImageView}
class ImageView {
public:
    /// Constructs a null tp::ImageView
    ImageView();

    /// Returns `true` if the image view is null and does not view any resource.
    bool isNull() const {
        return std::holds_alternative<std::monostate>(image);
    }

    /// Returns the type of the image view.
    ImageViewType getViewType() const {
        return setup.viewType;
    }

    /// Returns the image subresource range encompassing the entire range of the image view.
    /// @remarks
    ///     The returned range is relative to the view, therefore tp::ImageSubresourceRange::baseMipLevel
    ///     and tp::ImageSubresourceRange::baseArrayLevel are always 0. To resolve the
    ///     actual offsets to the underlying image, see tp::ImageView::vkResolveImageHandle.
    ImageSubresourceRange getWholeRange() const;

    /// Returns the format of the image view.
    Format getFormat() const {
        return setup.format;
    }

    /// Returns the extent of a specific mip level of the image view.
    Extent3D getExtent(uint32_t mipLevel = 0) const;

    /// Returns the multisampling level of the image view.
    MultisampleLevel getSampleLevel() const;

    /// Returns `true` if the instance views a job-local image.
    /// Returns `false` if it views a persistent one.
    bool viewsJobLocalImage() const {
        return std::holds_alternative<JobLocalImageImpl*>(image);
    }

    /// Creates another view of the viewed image relative to this view.
    /// @param subviewSetup
    ///     The setup structure describing the view.
    /// @remarks
    ///     The subresource range of the new view must be fully contained inside this view.
    /// @remarks
    ///     This is effectively a shorthand for tp::Image::createView. This means that it isn't thread safe with
    ///     respect to the underlying image. For job-local image views, this extends to the parent
    ///     tp::JobResourcePool.
    ImageView createView(ImageViewSetup subviewSetup);

    /// Returns the associated Vulkan @vksymbol{VkImageView} handle if it exists, `VK_NULL_HANDLE`
    /// otherwise.
    /// @remarks
    ///     If the viewed image is a job-local image, the @vksymbol{VkImageView} handle will exist only after
    ///     the tp::Job has been enqueued.
    VkImageViewHandle vkGetImageViewHandle() const;

    /// Resolves and returns the underlying @vksymbol{VkImage} handle of this view or `VK_NULL_HANDLE` if it doesn't
    /// exist.
    /// @param baseMipLevel
    ///     An output parameter to which the base mip level of the view will be set if the underlying
    ///     image exists.
    /// @param baseArrayLevel
    ///     An output parameter to which the base array level of the view will be set if the underlying
    ///     image exists.
    /// @remarks
    ///     If the viewed image is a job-local image, the underlying @vksymbol{VkImage} handle will exist only after
    ///     the tp::Job has been enqueued.
    VkImageHandle vkResolveImageHandle(uint32_t* baseMipLevel, uint32_t* baseArrayLevel) const;

private:
    friend class ImageImpl;
    friend class JobLocalImageImpl;
    friend bool operator==(const ImageView&, const ImageView&);

    ImageView(ImageImpl* persistentImage, ImageViewSetup setup);

    ImageView(JobLocalImageImpl* jobLocalImage, ImageViewSetup setup);

    std::variant<std::monostate, ImageImpl*, JobLocalImageImpl*> image;
    ImageViewSetup setup;
    mutable VkImageViewHandle vkCachedImageViewHandle = {};
};

/// Equality operator for tp::ImageView.
bool operator==(const ImageView& lhs, const ImageView& rhs);

/// Inequality operator for tp::ImageView.
inline bool operator!=(const ImageView& lhs, const ImageView& rhs) {
    return !(lhs == rhs);
}

/// Used as configuration for creating a new tp::Image object.
/// @see tp::Device::allocateImage
struct ImageSetup {
    ImageType type;
    ImageUsageMask usage;
    Format format;
    Extent3D extent;
    uint32_t mipLevelCount;
    uint32_t arrayLayerCount;
    MultisampleLevel sampleLevel;
    ArrayView<Format> compatibleFormats;
    ImageFlagMask flags;
    VkImageUsageFlags vkAdditionalUsage;
    VmaAllocationCreateFlags vmaAdditionalFlags;

    /// @param type
    ///     The type and dimensionality of the image.
    /// @param usage
    ///     A mask of tp::ImageUsage specifying the permitted set of usages of the new image.
    /// @param format
    ///     The format the data will be interpreted as.
    /// @param extent
    ///     The extent of the image in three dimensions.
    /// @param mipLevelCount
    ///     The number of mip levels the image should have.
    /// @param arrayLayerCount
    ///     The number of array layers the image should have.
    /// @param sampleLevel
    ///     The multisampling level of the image.
    /// @param compatibleFormats
    ///     The list of additional compatible formats that the tp::ImageView objects viewing this image are
    ///     permitted to have.
    /// @param flags
    ///     Additional flags for creation of the image.
    /// @param vkAdditionalUsage
    ///     A mask of additional Vulkan usage flags that will be passed to @vksymbol{VkImageCreateInfo}.
    /// @param vmaAdditionalFlags
    ///     A mask of additional VMA allocation create flags that will be passed to
    ///     @vmasymbol{VmaAllocationCreateInfo,struct_vma_allocation_create_info}
    /// @remarks
    ///     The extent must be compatible with the selected image type. For example 2D images
    ///     must have `extent.depth` equal to 1.
    ImageSetup(
        ImageType type,
        ImageUsageMask usage,
        Format format,
        Extent3D extent,
        uint32_t mipLevelCount = 1,
        uint32_t arrayLayerCount = 1,
        MultisampleLevel sampleLevel = MultisampleLevel::x1,
        ArrayView<Format> compatibleFormats = {},
        ImageFlagMask flags = ImageFlagMask::None(),
        VkBufferUsageFlags vkAdditionalUsage = 0,
        VmaAllocationCreateFlags vmaAdditionalFlags = 0)
        : type(type),
          usage(usage),
          format(format),
          extent(extent),
          mipLevelCount(mipLevelCount),
          arrayLayerCount(arrayLayerCount),
          sampleLevel(sampleLevel),
          compatibleFormats(compatibleFormats),
          flags(flags),
          vkAdditionalUsage(vkAdditionalUsage),
          vmaAdditionalFlags(vmaAdditionalFlags) {}
};

/// Represents a multidimensional array of data interpreted as textures or attachments.
///
/// It is generally not used directly, but instead gets passed to commands or descriptors through tp::ImageView objects
/// that view a contiguous range of its data.
///
/// @see tp::Device::allocateImage
/// @see @vksymbol{VkImage}
class Image : public Ownable {
public:
    /// Returns the type of the image.
    ImageType getType() const;

    /// Returns the format of the image.
    Format getFormat() const;

    /// Returns the extent of a specific mip level of the image.
    Extent3D getExtent(uint32_t mipLevel = 0) const;

    /// Returns the image subresource range encompassing the entire range of the image.
    ImageSubresourceRange getWholeRange() const;

    /// Returns the multisampling level of the image.
    MultisampleLevel getSampleLevel() const;

    /// Returns the memory location that the image has been allocated from.
    MemoryLocation getMemoryLocation() const;

    /// Returns the default tp::ImageView object that views the entire image subresource range with
    /// the same format and an identity component mapping.
    const ImageView& getDefaultView() const;

    /// Creates a view of the specified range of the image data.
    /// @param viewSetup
    ///     The setup structure describing the view.
    /// @remarks
    ///     The range of the new view must be fully contained inside the image.
    ImageView createView(ImageViewSetup viewSetup);

    /// Returns the associated @vmasymbol{VmaAllocation,struct_vma_allocation} handle.
    VmaAllocationHandle vmaGetMemoryAllocationHandle() const;

    /// Returns the associated @vksymbol{VkImage} handle.
    VkImageHandle vkGetImageHandle() const;

    /// Casting operator returning the default tp::ImageView object
    /// @see tp::Image::getDefaultView
    operator const ImageView&() const {
        return getDefaultView();
    }

    TEPHRA_MAKE_INTERFACE(Image);

protected:
    Image() {}
};

/// Equality operator for tp::Image.
inline bool operator==(const Image& lhs, const Image& rhs) {
    return lhs.vkGetImageHandle() == rhs.vkGetImageHandle();
}

/// Inequality operator for tp::Image.
inline bool operator!=(const Image& lhs, const Image& rhs) {
    return !(lhs == rhs);
}

}
