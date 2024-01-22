
#include "local_images.hpp"
#include "resource_pool_container.hpp"
#include <algorithm>

namespace tp {

JobLocalImageImpl::JobLocalImageImpl(
    ImageSetup setup,
    uint64_t localImageIndex,
    std::deque<ImageView>* jobPendingImageViews,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      localImageIndex(localImageIndex),
      setup(setup),
      jobPendingImageViews(jobPendingImageViews) {
    // Have to copy the compatible formats to the image class to ensure the data is kept around
    compatibleFormats.insert(compatibleFormats.begin(), setup.compatibleFormats.begin(), setup.compatibleFormats.end());
    if (std::find(compatibleFormats.begin(), compatibleFormats.end(), setup.format) == compatibleFormats.end()) {
        compatibleFormats.push_back(setup.format);
    }
    this->setup.compatibleFormats = view(compatibleFormats);
}

Extent3D JobLocalImageImpl::getExtent(uint32_t mipLevel) const {
    // TODO: This is incorrect for corner sampled images
    return Extent3D(
        tp::max(setup.extent.width >> mipLevel, 1u),
        tp::max(setup.extent.height >> mipLevel, 1u),
        tp::max(setup.extent.depth >> mipLevel, 1u));
}

ImageView JobLocalImageImpl::createDefaultView() {
    return createView(ImageImpl::getDefaultViewSetup(setup));
}

ImageView JobLocalImageImpl::createView(ImageViewSetup viewSetup) {
    // Resolve to actual size
    if (viewSetup.subresourceRange.arrayLayerCount == VK_REMAINING_ARRAY_LAYERS) {
        viewSetup.subresourceRange.arrayLayerCount = setup.arrayLayerCount - viewSetup.subresourceRange.baseArrayLayer;
    }

    if (hasUnderlyingImage()) {
        // Just create a view of the underlying image
        viewSetup.subresourceRange.baseArrayLayer += underlyingImageLayerOffset;
        return static_cast<ImageImpl*>(underlyingImage)->createView_(std::move(viewSetup));
    } else {
        // No resource assigned yet, so make it a view of this local image and add it to the
        // pending list to create a VkImageView when the underlying image gets assigned
        ImageView view = ImageView(this, std::move(viewSetup));
        jobPendingImageViews->push_back(view);
        return view;
    }
}

void JobLocalImageImpl::createPendingImageViews(std::deque<ImageView>& jobPendingImageViews) {
    for (ImageView& imageView : jobPendingImageViews) {
        JobLocalImageImpl& localImage = getImageImpl(imageView);
        // It not have been assigned an underlying image if it's never used
        if (!localImage.hasUnderlyingImage())
            continue;

        ImageViewSetup underlyingViewSetup = imageView.setup;
        underlyingViewSetup.subresourceRange.baseArrayLayer += localImage.underlyingImageLayerOffset;
        static_cast<ImageImpl*>(localImage.underlyingImage)->createView_(std::move(underlyingViewSetup));
    }
}

VkImageViewHandle JobLocalImageImpl::vkGetImageViewHandle(const ImageView& imageView) {
    return ImageImpl::vkGetImageViewHandle(getViewToUnderlyingImage(imageView));
}

ImageView JobLocalImageImpl::getViewToUnderlyingImage(const ImageView& imageView) {
    JobLocalImageImpl& localImage = getImageImpl(imageView);
    TEPHRA_ASSERT(localImage.hasUnderlyingImage());

    ImageViewSetup underlyingViewSetup = imageView.setup;
    underlyingViewSetup.subresourceRange.baseArrayLayer += localImage.underlyingImageLayerOffset;

    ImageImpl* underlyingImage = static_cast<ImageImpl*>(localImage.underlyingImage);
    return ImageView(underlyingImage, underlyingViewSetup);
}

JobLocalImageImpl& JobLocalImageImpl::getImageImpl(const ImageView& imageView) {
    TEPHRA_ASSERT(imageView.viewsJobLocalImage());
    return *std::get<JobLocalImageImpl*>(imageView.image);
}

ImageView JobLocalImages::acquireNewImage(ImageSetup setup, DebugTarget debugTarget) {
    images.emplace_back(setup, images.size(), &pendingImageViews, std::move(debugTarget));
    usageRanges.emplace_back();
    return images.back().createDefaultView();
}

void JobLocalImages::createPendingImageViews() {
    JobLocalImageImpl::createPendingImageViews(pendingImageViews);
    pendingImageViews.clear();
}

void JobLocalImages::markImageUsage(const ImageView& imageView, uint64_t usageNumber) {
    uint64_t imageIndex = getLocalImageIndex(imageView);
    usageRanges[imageIndex].update(usageNumber);
}

uint64_t JobLocalImages::getLocalImageIndex(const ImageView& imageView) const {
    return JobLocalImageImpl::getImageImpl(imageView).getLocalIndex();
}

const ResourceUsageRange& JobLocalImages::getImageUsage(const ImageView& imageView) const {
    uint64_t bufferIndex = getLocalImageIndex(imageView);
    return usageRanges[bufferIndex];
}

void JobLocalImages::clear() {
    images.clear();
    pendingImageViews.clear();
    usageRanges.clear();
}

}
