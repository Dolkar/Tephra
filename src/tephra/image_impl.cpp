
#include "image_impl.hpp"
#include "device/device_container.hpp"

namespace tp {

ImageImpl::ImageImpl(
    DeviceContainer* deviceImpl,
    ImageSetup imageSetup,
    Lifeguard<VkImageHandle> imageHandle,
    Lifeguard<VmaAllocationHandle> memoryAllocationHandle,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      deviceImpl(deviceImpl),
      memoryAllocationHandle(std::move(memoryAllocationHandle)),
      imageHandle(std::move(imageHandle)),
      type(imageSetup.type),
      extent(imageSetup.extent),
      sampleLevel(imageSetup.sampleLevel),
      defaultView(this, getDefaultViewSetup(imageSetup)) {
    // Create the default view Vulkan handle if the image can have views
    canHaveVulkanViews = imageSetup.usage.containsAny(
        ImageUsage::SampledImage | ImageUsage::StorageImage | ImageUsage::ColorAttachment |
        ImageUsage::DepthStencilAttachment | ImageUsage::InputAttachment);

    createView_(defaultView.setup);
    vkDefaultViewHandle = viewHandleMap[defaultView.setup];
}

Extent3D ImageImpl::getExtent_(uint32_t mipLevel) const {
    // TODO: This is incorrect for corner sampled images
    return Extent3D(
        tp::max(extent.width >> mipLevel, 1u),
        tp::max(extent.height >> mipLevel, 1u),
        tp::max(extent.depth >> mipLevel, 1u));
}

MemoryLocation ImageImpl::getMemoryLocation_() const {
    return deviceImpl->getMemoryAllocator()->getAllocationLocation(memoryAllocationHandle.vkGetHandle());
}

ImageView ImageImpl::createView_(ImageViewSetup viewSetup) {
    // Make sure the setup is unique
    ImageSubresourceRange fullRange = defaultView.getWholeRange();
    if (viewSetup.subresourceRange.mipLevelCount == VK_REMAINING_MIP_LEVELS) {
        viewSetup.subresourceRange.mipLevelCount = fullRange.mipLevelCount - viewSetup.subresourceRange.baseMipLevel;
    }
    if (viewSetup.subresourceRange.arrayLayerCount == VK_REMAINING_ARRAY_LAYERS) {
        viewSetup.subresourceRange.arrayLayerCount = fullRange.arrayLayerCount -
            viewSetup.subresourceRange.arrayLayerCount;
    }
    if (viewSetup.format == Format::Undefined) {
        viewSetup.format = defaultView.getFormat();
    }

    ImageView view = ImageView(this, std::move(viewSetup));

    if (canHaveVulkanViews) {
        VkImageViewHandle& vkImageViewHandle = viewHandleMap[view.setup];

        if (vkImageViewHandle.isNull()) {
            vkImageViewHandle = deviceImpl->getLogicalDevice()->createImageView(imageHandle.vkGetHandle(), view.setup);
        }
    }

    return view;
}

void ImageImpl::destroyHandles(bool immediately) {
    if (imageHandle.isNull())
        return;

    // Free all the image views
    for (const std::pair<ImageViewSetup, VkImageViewHandle>& imageViewPair : viewHandleMap) {
        VkImageViewHandle vkImageViewHandle = imageViewPair.second;
        // Create a temporary lifeguard here to avoid the unnecessary overhead of storing them in viewHandleMap
        Lifeguard<VkImageViewHandle> lifeguard = deviceImpl->vkMakeHandleLifeguard(vkImageViewHandle);
        lifeguard.destroyHandle(immediately);
    }
    viewHandleMap.clear();

    imageHandle.destroyHandle(immediately);
    memoryAllocationHandle.destroyHandle(immediately);
}

VkImageViewHandle ImageImpl::vkGetImageViewHandle(const ImageView& imageView) {
    ImageImpl& image = getImageImpl(imageView);
    auto mapHit = image.viewHandleMap.find(imageView.setup);
    if (mapHit != image.viewHandleMap.end()) {
        return mapHit->second;
    } else {
        return VkImageViewHandle();
    }
}

ImageImpl& ImageImpl::getImageImpl(const ImageView& imageView) {
    TEPHRA_ASSERT(!imageView.isNull());
    TEPHRA_ASSERT(!imageView.viewsJobLocalImage());
    return *std::get<ImageImpl*>(imageView.image);
}

ImageViewSetup ImageImpl::getDefaultViewSetup(const ImageSetup& imageSetup) {
    bool isArray = imageSetup.arrayLayerCount > 1;

    ImageViewType defaultViewType;
    if (isArray) {
        if (imageSetup.type == ImageType::Image1D)
            defaultViewType = ImageViewType::View1DArray;
        else
            defaultViewType = ImageViewType::View2DArray;
        TEPHRA_ASSERT(imageSetup.type != ImageType::Image3D);
    } else {
        if (imageSetup.type == ImageType::Image1D)
            defaultViewType = ImageViewType::View1D;
        else if (imageSetup.type == ImageType::Image2D)
            defaultViewType = ImageViewType::View2D;
        else
            defaultViewType = ImageViewType::View3D;
    }

    ImageAspectMask aspectMask = getFormatClassProperties(imageSetup.format).aspectMask;
    return ImageViewSetup(
        defaultViewType, { aspectMask, 0, imageSetup.mipLevelCount, 0, imageSetup.arrayLayerCount }, imageSetup.format);
}

}
