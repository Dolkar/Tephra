#pragma once

#include "common_impl.hpp"
#include <tephra/image.hpp>
#include <tephra/format_compatibility.hpp>
#include <unordered_map>

namespace tp {

inline bool operator==(const ImageViewSetup& lhs, const ImageViewSetup& rhs) {
    return lhs.viewType == rhs.viewType && lhs.subresourceRange.aspectMask == rhs.subresourceRange.aspectMask &&
        lhs.subresourceRange.baseMipLevel == rhs.subresourceRange.baseMipLevel &&
        lhs.subresourceRange.mipLevelCount == rhs.subresourceRange.mipLevelCount &&
        lhs.subresourceRange.baseArrayLayer == rhs.subresourceRange.baseArrayLayer &&
        lhs.subresourceRange.arrayLayerCount == rhs.subresourceRange.arrayLayerCount && lhs.format == rhs.format &&
        lhs.componentMapping.r == rhs.componentMapping.r && lhs.componentMapping.g == rhs.componentMapping.g &&
        lhs.componentMapping.b == rhs.componentMapping.b && lhs.componentMapping.a == rhs.componentMapping.a;
}

struct ImageViewSetupHash {
    constexpr std::size_t operator()(const ImageViewSetup& setup) const {
        const uint64_t fibMul = 11400714819323198485ull; // 2^64 / phi
        uint64_t hash = static_cast<uint64_t>(setup.viewType);
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.subresourceRange.aspectMask);
        hash = hash * fibMul ^ setup.subresourceRange.baseMipLevel;
        hash = hash * fibMul ^ setup.subresourceRange.mipLevelCount;
        hash = hash * fibMul ^ setup.subresourceRange.baseArrayLayer;
        hash = hash * fibMul ^ setup.subresourceRange.arrayLayerCount;
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.format);
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.componentMapping.r);
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.componentMapping.g);
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.componentMapping.b);
        hash = hash * fibMul ^ static_cast<uint32_t>(setup.componentMapping.a);
        return hash;
    }
};

class ImageImpl : public Image {
public:
    ImageImpl(
        DeviceContainer* deviceImpl,
        ImageSetup imageSetup,
        Lifeguard<VkImageHandle>&& imageHandle,
        Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
        DebugTarget debugTarget);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    ImageType getType_() const {
        return type;
    }

    Format getFormat_() const {
        return defaultView.getFormat();
    }

    Extent3D getExtent_(uint32_t mipLevel) const;

    ImageSubresourceRange getWholeRange_() const {
        return defaultView.getWholeRange();
    }

    MultisampleLevel getSampleLevel_() const {
        return sampleLevel;
    }

    MemoryLocation getMemoryLocation_() const;

    const ImageView& getDefaultView_() const {
        return defaultView;
    }

    ImageView createView_(ImageViewSetup viewSetup);

    VmaAllocationHandle vmaGetMemoryAllocationHandle_() const {
        return memoryAllocationHandle.vkGetHandle();
    }

    VkImageHandle vkGetImageHandle_() const {
        return imageHandle.vkGetHandle();
    }

    void destroyHandles(bool immediately);

    static VkImageViewHandle vkGetImageViewHandle(const ImageView& imageView);

    static ImageImpl& getImageImpl(const ImageView& imageView);

    static ImageViewSetup getDefaultViewSetup(const ImageSetup& imageSetup);

    TEPHRA_MAKE_NONCOPYABLE(ImageImpl);
    TEPHRA_MAKE_NONMOVABLE(ImageImpl);
    ~ImageImpl();

private:
    friend class ImageView;

    using ImageViewHandleMap = std::unordered_map<ImageViewSetup, VkImageViewHandle, ImageViewSetupHash>;

    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    Lifeguard<VmaAllocationHandle> memoryAllocationHandle;
    Lifeguard<VkImageHandle> imageHandle;

    ImageType type;
    Extent3D extent;
    MultisampleLevel sampleLevel;

    ImageView defaultView;
    bool canHaveVulkanViews;
    ImageViewHandleMap viewHandleMap;
    VkImageViewHandle vkDefaultViewHandle;
};

}
