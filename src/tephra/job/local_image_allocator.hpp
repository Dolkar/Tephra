#pragma once

#include "local_images.hpp"

namespace tp {

class JobLocalImageAllocator {
public:
    JobLocalImageAllocator(DeviceContainer* deviceImpl, JobResourcePoolFlagMask poolFlags);

    void allocateJobImages(JobLocalImages* imageResources, uint64_t currentTimestamp, const char* jobName);

    void trim(uint64_t upToTimestamp);

    uint32_t getAllocationCount() const {
        return totalAllocationCount;
    }

    uint64_t getTotalSize() const {
        return totalAllocationSize;
    }

private:
    // Struct defining the requested image properties that must match between reused images
    struct ImageClass {
        static constexpr std::size_t FormatStampSize = 4;

        ImageType type;
        ImageUsage usage;
        uint32_t formatStamp[FormatStampSize];
        Extent3D extent;
        uint32_t mipLevelCount;
        MultisampleLevel sampleLevel;
        ImageFlagMask flags;

        ImageClass(const ImageSetup& setup, bool alwaysUseFormatClass);

        // Adjusts image setup's compatible formats to reflect the simplified format stamp
        static void conformImageSetupToClass(ImageSetup* setup, bool alwaysUseFormatClass);
    };

    friend bool operator==(
        const JobLocalImageAllocator::ImageClass& first,
        const JobLocalImageAllocator::ImageClass& second);

    friend bool operator<(const JobLocalImageAllocator::ImageClass& lhs, const JobLocalImageAllocator::ImageClass& rhs);

    struct ImageClassHash {
        constexpr std::size_t operator()(const ImageClass& imageClass) const {
            const uint64_t fibMul = 11400714819323198485ull; // 2^64 / phi
            uint64_t hash = static_cast<uint64_t>(imageClass.type);
            hash = hash * fibMul ^ static_cast<uint32_t>(imageClass.usage);
            for (int i = 0; i < ImageClass::FormatStampSize; i++)
                hash = hash * fibMul ^ static_cast<uint32_t>(imageClass.formatStamp[i]);
            hash = hash * fibMul ^ imageClass.extent.width;
            hash = hash * fibMul ^ imageClass.extent.height;
            hash = hash * fibMul ^ imageClass.extent.depth;
            hash = hash * fibMul ^ imageClass.mipLevelCount;
            hash = hash * fibMul ^ static_cast<uint32_t>(imageClass.sampleLevel);
            hash = hash * fibMul ^ static_cast<uint32_t>(imageClass.flags);
            return hash;
        }
    };

    // Backing image pointer along with its last used timestamp
    using BackingImage = std::pair<std::unique_ptr<ImageImpl>, uint64_t>;

    using ImageClassMap = std::unordered_map<ImageClass, std::vector<BackingImage>, ImageClassHash>;

    struct AssignInfo : ResourceUsageRange {
        uint32_t arrayLayerCount;
        JobLocalImageImpl* resourcePtr;
    };
    DeviceContainer* deviceImpl;
    JobResourcePoolFlagMask poolFlags;
    ImageClassMap backingImageMap;
    uint64_t totalAllocationSize = 0;
    uint32_t totalAllocationCount = 0;

    // Allocate requested images from the given backing group, returns the number of layers used
    uint64_t allocateJobImageClass(
        std::vector<BackingImage>& backingImages,
        ScratchVector<AssignInfo>& imagesToAlloc,
        uint64_t currentTimestamp);

    // Allocates requested images from the given backing group as individual images without aliasing
    uint64_t allocateJobImageClassNoAlias(
        std::vector<BackingImage>& backingImages,
        ScratchVector<AssignInfo>& imagesToAlloc,
        uint64_t currentTimestamp);

    // Helper function to allocate an internal backing image
    static std::unique_ptr<ImageImpl> allocateBackingImage(DeviceContainer* deviceImpl, const ImageSetup& setup);
};

}
