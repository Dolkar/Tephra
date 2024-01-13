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
        ImageSetup setup,
        uint64_t localImageIndex,
        std::deque<ImageView>* jobPendingImageViews,
        DebugTarget debugTarget);

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

    static JobLocalImageImpl& getImageImpl(const ImageView& imageView);

private:
    DebugTarget debugTarget;
    uint64_t localImageIndex;

    ImageSetup setup;
    std::vector<Format> compatibleFormats;

    Image* underlyingImage = nullptr;
    uint32_t underlyingImageLayerOffset = 0;
    std::deque<ImageView>* jobPendingImageViews;
};

// Once stored, it is not guaranteed that the persistent parent objects (ImageImpl) of views will be kept alive,
// so they need to be resolved immediately. But job-local resources need to be resolved later after they actually get
// created. This class handles resolving both at the right time.
class StoredImageView {
public:
    StoredImageView(const ImageView& view) : storedView(store(view)) {}

    bool isNull() const {
        if (std::holds_alternative<ImageView>(storedView)) {
            return std::get<ImageView>(storedView).isNull();
        } else {
            return std::get<ResolvedView>(storedView).vkImageHandle.isNull();
        }
    }

    // Used for attachment accesses so we don't have to grab the views from input structures
    const ImageView* getJobLocalView() const {
        if (std::holds_alternative<ImageView>(storedView)) {
            return &std::get<ImageView>(storedView);
        } else {
            return nullptr;
        }
    }

    ImageSubresourceRange getWholeRange() {
        resolve();

        ImageSubresourceRange wholeRange = std::get<ResolvedView>(storedView).subresourceRange;
        wholeRange.baseMipLevel = 0;
        wholeRange.baseArrayLayer = 0;
        return wholeRange;
    }

    Format getFormat() {
        resolve();
        return std::get<ResolvedView>(storedView).format;
    }

    VkImageViewHandle vkGetImageViewHandle() {
        resolve();
        return std::get<ResolvedView>(storedView).vkImageViewHandle;
    }

    VkImageHandle vkResolveImageHandle(uint32_t* baseMipLevel, uint32_t* baseArrayLevel) {
        resolve();
        *baseMipLevel = std::get<ResolvedView>(storedView).subresourceRange.baseMipLevel;
        *baseArrayLevel = std::get<ResolvedView>(storedView).subresourceRange.baseArrayLayer;
        return std::get<ResolvedView>(storedView).vkImageHandle;
    }

private:
    struct ResolvedView {
        ImageSubresourceRange subresourceRange;
        Format format;
        VkImageHandle vkImageHandle;
        VkImageViewHandle vkImageViewHandle;

        explicit ResolvedView(const ImageView& view) {
            subresourceRange = view.getWholeRange();
            format = view.getFormat();
            vkImageHandle = view.vkResolveImageHandle(&subresourceRange.baseMipLevel, &subresourceRange.baseArrayLayer);
            vkImageViewHandle = view.vkGetImageViewHandle();
        }
    };

    static std::variant<ResolvedView, ImageView> store(const ImageView& view) {
        if (!view.viewsJobLocalImage()) {
            return ResolvedView(view);
        } else {
            return view;
        }
    }

    void resolve() {
        if (std::holds_alternative<ImageView>(storedView)) {
            storedView = ResolvedView(std::get<ImageView>(storedView));
            TEPHRA_ASSERTD(
                !std::get<ResolvedView>(storedView).vkImageHandle.isNull(),
                "Job-local images must be resolvable at this point");
        }
    }

    std::variant<ResolvedView, ImageView> storedView;
};

class JobResourcePoolContainer;

class JobLocalImages {
public:
    explicit JobLocalImages(JobResourcePoolContainer* resourcePoolImpl) : resourcePoolImpl(resourcePoolImpl) {}

    ImageView acquireNewImage(ImageSetup setup, const char* debugName);

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
