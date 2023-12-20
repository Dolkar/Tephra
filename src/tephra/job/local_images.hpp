#pragma once

#include "aliasing_suballocator.hpp"
#include "../image_impl.hpp"
#include "../common_impl.hpp"
#include <tephra/job.hpp>
#include <deque>

namespace tp {

class JobLocalImageImpl {
public:
    JobLocalImageImpl(
        DebugTarget debugTarget,
        ImageSetup setup,
        uint64_t localImageIndex,
        std::deque<ImageView>* jobPendingImageViews);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    const ImageSetup& getImageSetup() const {
        return setup;
    }

    Extent3D getExtent(uint32_t mipLevel) const;

    MultisampleLevel getSampleLevel() const {
        return setup.sampleLevel;
    }

    void assignUnderlyingImage(Image* image, uint32_t layerOffset) {
        underlyingImage = image;
        underlyingImageLayerOffset = layerOffset;
    }

    bool hasUnderlyingImage() const {
        return underlyingImage != nullptr;
    }

    const Image* getUnderlyingImage() const {
        return underlyingImage;
    }

    Image* getUnderlyingImage() {
        return underlyingImage;
    }

    uint64_t getLocalIndex() const {
        return localImageIndex;
    }

    ImageView createDefaultView();

    ImageView createView(ImageViewSetup setup);

    static void createPendingImageViews(std::deque<ImageView>& jobPendingImageViews);

    static VkImageViewHandle vkGetImageViewHandle(const ImageView& imageView);

    // Translates view of the local buffer to a view of the underlying resource.
    static ImageView getViewToUnderlyingImage(const ImageView& imageView);

    static JobLocalImageImpl* getImageImpl(const ImageView& imageView);

private:
    DebugTarget debugTarget;
    uint64_t localImageIndex;

    ImageSetup setup;
    std::vector<Format> compatibleFormats;

    Image* underlyingImage = nullptr;
    uint32_t underlyingImageLayerOffset = 0;
    std::deque<ImageView>* jobPendingImageViews;
};

class JobResourcePoolContainer;

class JobLocalImages {
public:
    explicit JobLocalImages(JobResourcePoolContainer* resourcePoolImpl) : resourcePoolImpl(resourcePoolImpl) {}

    ImageView acquireNewImage(ImageSetup setup, DebugTarget debugTarget);

    void createPendingImageViews();

    void markImageUsage(const ImageView& imageView, uint64_t usageNumber);

    const ResourceUsageRange& getImageUsage(const ImageView& imageView) const;

    const std::deque<JobLocalImageImpl>& getImages() const {
        return images;
    }

    void clear();

private:
    friend class JobLocalImageAllocator;

    uint64_t getLocalImageIndex(const ImageView& imageView) const;

    JobResourcePoolContainer* resourcePoolImpl;
    std::deque<JobLocalImageImpl> images; // The local images implementing access through views
    std::deque<ImageView> pendingImageViews; // Image views that need vkImageViews assigned
    std::deque<ResourceUsageRange> usageRanges; // The usages of the local images within the job
};

}
