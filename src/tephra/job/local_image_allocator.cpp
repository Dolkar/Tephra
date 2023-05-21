
#include "local_image_allocator.hpp"
#include "../device/device_container.hpp"
#include "../image_impl.hpp"
#include <algorithm>

#ifdef TEPHRA_ENABLE_DEBUG_STATS
    #include <chrono>
#endif

namespace tp {

JobLocalImageAllocator::ImageClass::ImageClass(const ImageSetup& setup, bool alwaysUseFormatClass)
    : type(setup.type),
      usage(setup.usage),
      extent(setup.extent),
      mipLevelCount(setup.mipLevelCount),
      sampleLevel(setup.sampleLevel),
      flags(setup.flags) {
    // Convert potentially any number of compatible format to a stamp that can be used for comparing image classes
    int stampEnd = 0;
    if (alwaysUseFormatClass || setup.compatibleFormats.size() > FormatStampSize) {
        formatStamp[stampEnd++] = static_cast<uint32_t>(getFormatCompatibilityClass(setup.format));
    } else {
        // The list here is guaranteed to contain the image format itself
        for (; stampEnd < setup.compatibleFormats.size(); stampEnd++) {
            formatStamp[stampEnd] = static_cast<uint32_t>(setup.compatibleFormats[stampEnd]);
        }
        // Sort because order shouldn't matter
        std::sort(formatStamp, formatStamp + stampEnd);
    }

    for (int i = stampEnd; i < FormatStampSize; i++)
        formatStamp[i] = 0;
}

void JobLocalImageAllocator::ImageClass::conformImageSetupToClass(ImageSetup* setup, bool alwaysUseFormatClass) {
    if (alwaysUseFormatClass || setup->compatibleFormats.size() > FormatStampSize) {
        // The image class may be defined by the format compatibility class. In that case, manually enable the
        // mutable format flag without any restrictions to make every format in that class compatible.
        setup->compatibleFormats = {};
        setup->flags |= static_cast<ImageFlag>(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);
    }
}

bool operator==(const JobLocalImageAllocator::ImageClass& first, const JobLocalImageAllocator::ImageClass& second) {
    if (first.type != second.type || first.usage != second.usage)
        return false;

    for (int i = 0; i < JobLocalImageAllocator::ImageClass::FormatStampSize; i++)
        if (first.formatStamp[i] != second.formatStamp[i])
            return false;

    return first.extent.width == second.extent.width && first.extent.height == second.extent.height &&
        first.extent.depth == second.extent.depth && first.mipLevelCount == second.mipLevelCount &&
        first.sampleLevel == second.sampleLevel && first.flags == second.flags;
}

bool operator<(const JobLocalImageAllocator::ImageClass& lhs, const JobLocalImageAllocator::ImageClass& rhs) {
    if (lhs.type != rhs.type)
        return lhs.type < rhs.type;
    if (lhs.usage != rhs.usage)
        return lhs.usage < rhs.usage;

    for (int i = 0; i < JobLocalImageAllocator::ImageClass::FormatStampSize; i++)
        if (lhs.formatStamp[i] != rhs.formatStamp[i])
            return lhs.formatStamp[i] < rhs.formatStamp[i];

    if (lhs.extent.width != rhs.extent.width)
        return lhs.extent.width < rhs.extent.width;
    if (lhs.extent.height != rhs.extent.height)
        return lhs.extent.height < rhs.extent.height;
    if (lhs.extent.depth != rhs.extent.depth)
        return lhs.extent.depth < rhs.extent.depth;

    if (lhs.mipLevelCount != rhs.mipLevelCount)
        return lhs.mipLevelCount < rhs.mipLevelCount;
    if (lhs.sampleLevel != rhs.sampleLevel)
        return lhs.sampleLevel < rhs.sampleLevel;
    return static_cast<tp::ImageFlagMask::EnumValueType>(lhs.flags) <
        static_cast<tp::ImageFlagMask::EnumValueType>(rhs.flags);
}

JobLocalImageAllocator::JobLocalImageAllocator(DeviceContainer* deviceImpl, JobResourcePoolFlagMask poolFlags)
    : deviceImpl(deviceImpl), poolFlags(poolFlags) {}

void JobLocalImageAllocator::allocateJobImages(
    JobLocalImages* imageResources,
    uint64_t currentTimestamp,
    const char* jobName) {
    uint64_t imageBytesRequested = 0;
    uint64_t imageBytesCommitted = 0;

    // Group requests by their image class by sorting
    ScratchVector<std::pair<ImageClass, int>> assignList;
    for (int i = 0; i < imageResources->images.size(); i++) {
        ImageClass imageClass = ImageClass(
            imageResources->images[i].getImageSetup(), poolFlags.contains(JobResourcePoolFlag::AliasCompatibleFormats));
        assignList.emplace_back(imageClass, i);
    }
    std::sort(assignList.begin(), assignList.end());

    // Process each class individually
    ScratchVector<AssignInfo> assignInfos;
    int i = 0;
    while (i < assignList.size()) {
        ImageClass& imageClass = assignList[i].first;

        // Gather all image requests of this class
        do {
            int imageIndex = assignList[i].second;

            AssignInfo assignInfo;
            assignInfo.firstUsage = imageResources->usageRanges[imageIndex].firstUsage;
            assignInfo.lastUsage = imageResources->usageRanges[imageIndex].lastUsage;
            assignInfo.arrayLayerCount = imageResources->images[imageIndex].getImageSetup().arrayLayerCount;
            assignInfo.resourcePtr = &imageResources->images[imageIndex];
            assignInfos.push_back(assignInfo);

            if constexpr (StatisticEventsEnabled) {
                const ImageSetup& setup = imageResources->images[imageIndex].getImageSetup();
                imageBytesRequested += deviceImpl->getMemoryAllocator()->getImageMemoryRequirements(setup).size;
            }

            i++;
        } while (i < assignList.size() && !(imageClass < assignList[i].first));

        std::vector<BackingImage>& backingImages = backingImageMap[imageClass];

        // 3D images don't support array layers and so can't be aliased through them
        bool is3D = imageClass.type == ImageType::Image3D || imageClass.type == ImageType::Image3D2DArrayCompatible;
        uint64_t usedLayers;
        if (!poolFlags.contains(JobResourcePoolFlag::DisableSuballocation) && !is3D)
            usedLayers = allocateJobImageClass(backingImages, assignInfos, currentTimestamp);
        else
            usedLayers = allocateJobImageClassNoAlias(backingImages, assignInfos, currentTimestamp);

        if constexpr (StatisticEventsEnabled) {
            // Estimate memory per layer
            TEPHRA_ASSERT(!backingImages.empty());
            VmaAllocationHandle vmaAllocationHandle = backingImages[0].first->vmaGetMemoryAllocationHandle();
            uint64_t backingImageSize = deviceImpl->getMemoryAllocator()->getAllocationInfo(vmaAllocationHandle).size;
            uint32_t backingImageLayers = backingImages[0].first->getWholeRange().arrayLayerCount;
            uint64_t layerSize = backingImageSize / backingImageLayers;

            imageBytesCommitted += usedLayers * layerSize;
        }

        i++;
    }

    if constexpr (StatisticEventsEnabled) {
        reportStatisticEvent(StatisticEventType::JobLocalImageRequestedBytes, imageBytesRequested, jobName);
        reportStatisticEvent(StatisticEventType::JobLocalImageCommittedBytes, imageBytesCommitted, jobName);
    }

    imageResources->createPendingImageViews();
}

void JobLocalImageAllocator::trim(uint64_t upToTimestamp) {
    for (auto& [imageClass, backingImages] : backingImageMap) {
        auto removeIt = std::remove_if(
            backingImages.begin(),
            backingImages.end(),
            [deviceImpl = this->deviceImpl,
             &totalAllocationSize = this->totalAllocationSize,
             &totalAllocationCount = this->totalAllocationCount,
             upToTimestamp](const auto& el) {
                const auto& [backingImage, lastUseTimestamp] = el;
                bool trimmable = lastUseTimestamp <= upToTimestamp;

                if (trimmable) {
                    VmaAllocationHandle vmaAllocationHandle = backingImage->vmaGetMemoryAllocationHandle();
                    uint64_t backingImageSize =
                        deviceImpl->getMemoryAllocator()->getAllocationInfo(vmaAllocationHandle).size;
                    TEPHRA_ASSERT(totalAllocationSize >= backingImageSize);
                    TEPHRA_ASSERT(totalAllocationCount >= 1);
                    totalAllocationSize -= backingImageSize;
                    totalAllocationCount--;
                    // Destroy the handles immediately, since we already know the image isn't being used
                    backingImage->destroyHandles(true);
                }
                return trimmable;
            });
        backingImages.erase(removeIt, backingImages.end());
    }
}

uint64_t JobLocalImageAllocator::allocateJobImageClass(
    std::vector<BackingImage>& backingImages,
    ScratchVector<AssignInfo>& imagesToAlloc,
    uint64_t currentTimestamp) {
    // Suballocate the images from the backing allocations with aliasing by layers
    ScratchVector<uint64_t> backingImageLayers;
    backingImageLayers.reserve(backingImages.size());
    for (const auto& [backingImage, lastUseTimestamp] : backingImages) {
        backingImageLayers.push_back(backingImage->getWholeRange().arrayLayerCount);
    }

    AliasingSuballocator suballocator(view(backingImageLayers));

    // Sort images by the number of array layers - wouldn't want a large array to have to be allocated fresh
    // because a single image stole its original allocation
    std::sort(imagesToAlloc.begin(), imagesToAlloc.end(), [](const AssignInfo& left, const AssignInfo& right) {
        return left.arrayLayerCount > right.arrayLayerCount;
    });

    // Index and offset of leftover images that didn't fit
    ScratchVector<std::pair<int, uint32_t>> leftoverImages;
    leftoverImages.reserve(imagesToAlloc.size());
    uint32_t leftoverLayers = 0;

    for (int i = 0; i < imagesToAlloc.size(); i++) {
        auto [backingImageIndex, backingOffset] = suballocator.allocate(
            imagesToAlloc[i].arrayLayerCount, ResourceUsageRange(imagesToAlloc[i]), 1);
        uint32_t layerOffset = static_cast<uint32_t>(backingOffset);
        if (backingImageIndex < backingImages.size()) {
            // The allocation fits - assign and update timestamp
            auto& [backingImage, lastUseTimestamp] = backingImages[backingImageIndex];
            imagesToAlloc[i].resourcePtr->assignUnderlyingImage(backingImage.get(), layerOffset);
            lastUseTimestamp = currentTimestamp;
        } else {
            // It doesn't, remember it so we can allocate a new backing image for it
            leftoverImages.emplace_back(i, layerOffset);
            leftoverLayers = tp::max(leftoverLayers, layerOffset + imagesToAlloc[i].arrayLayerCount);
        }
    }

    if (leftoverImages.empty())
        return suballocator.getUsedSize();

    // Some of the images still haven't been assigned. Create a new backing image to host them.
    // Use the first image's setup as a reference
    ImageSetup backingSetup = imagesToAlloc[leftoverImages[0].first].resourcePtr->getImageSetup();
    ImageClass::conformImageSetupToClass(
        &backingSetup, poolFlags.contains(JobResourcePoolFlag::AliasCompatibleFormats));

    // Don't do overallocations for image layers, their size is less likely to vary as much
    backingSetup.arrayLayerCount = leftoverLayers;

    BackingImage newEntry = std::make_pair(allocateBackingImage(deviceImpl, backingSetup), currentTimestamp);
    ImageImpl* newBackingImage = newEntry.first.get();
    VmaAllocationHandle vmaAllocationHandle = newBackingImage->vmaGetMemoryAllocationHandle();
    totalAllocationCount++;
    totalAllocationSize += deviceImpl->getMemoryAllocator()->getAllocationInfo(vmaAllocationHandle).size;

    // Insert the new backing image to the list so that the largest image appears first
    auto pos = std::find_if(backingImages.begin(), backingImages.end(), [leftoverLayers](const auto& entry) {
        return entry.first->getWholeRange().arrayLayerCount < leftoverLayers;
    });
    backingImages.insert(pos, std::move(newEntry));

    // Assign the leftover resources to the new backing image
    for (auto& [imageIndex, layerOffset] : leftoverImages) {
        imagesToAlloc[imageIndex].resourcePtr->assignUnderlyingImage(newBackingImage, layerOffset);
    }

    return suballocator.getUsedSize();
}

uint64_t JobLocalImageAllocator::allocateJobImageClassNoAlias(
    std::vector<BackingImage>& backingImages,
    ScratchVector<AssignInfo>& imagesToAlloc,
    uint64_t currentTimestamp) {
    // Sort images by the number of array layers - wouldn't want a large array to have to be allocated fresh
    // because a single image stole its original allocation
    std::sort(imagesToAlloc.begin(), imagesToAlloc.end(), [](const AssignInfo& left, const AssignInfo& right) {
        return left.arrayLayerCount > right.arrayLayerCount;
    });

    ScratchVector<std::unique_ptr<ImageImpl>> newBackingImages;
    newBackingImages.reserve(imagesToAlloc.size());
    uint64_t totalLayers = 0;

    int backingImageIndex = 0;
    for (AssignInfo& imageToAlloc : imagesToAlloc) {
        ImageImpl* backingImage;
        if (backingImageIndex < backingImages.size() &&
            imageToAlloc.arrayLayerCount <= backingImages[backingImageIndex].first->getWholeRange().arrayLayerCount) {
            // Can reuse existing backing image
            backingImage = backingImages[backingImageIndex].first.get();
            backingImages[backingImageIndex].second = currentTimestamp;
            backingImageIndex++;
        } else {
            // Create a new backing image
            ImageSetup backingSetup = imageToAlloc.resourcePtr->getImageSetup();
            ImageClass::conformImageSetupToClass(
                &backingSetup, poolFlags.contains(JobResourcePoolFlag::AliasCompatibleFormats));
            newBackingImages.emplace_back(allocateBackingImage(deviceImpl, backingSetup));
            backingImage = newBackingImages.back().get();

            VmaAllocationHandle vmaAllocationHandle = backingImage->vmaGetMemoryAllocationHandle();
            totalAllocationCount++;
            totalAllocationSize += deviceImpl->getMemoryAllocator()->getAllocationInfo(vmaAllocationHandle).size;
        }

        deviceImpl->getLogicalDevice()->setObjectDebugName(
            backingImage->vkGetImageHandle(), imageToAlloc.resourcePtr->getDebugTarget()->getObjectName());
        imageToAlloc.resourcePtr->assignUnderlyingImage(backingImage, 0);
        totalLayers += imageToAlloc.arrayLayerCount;
    }

    // Insert the new backing images to the list so that the largest image appears first
    for (std::unique_ptr<ImageImpl>& newBackingImage : newBackingImages) {
        auto pos = std::find_if(backingImages.begin(), backingImages.end(), [&newBackingImage](const auto& entry) {
            return entry.first->getWholeRange().arrayLayerCount < newBackingImage->getWholeRange().arrayLayerCount;
        });
        backingImages.insert(pos, std::make_pair(std::move(newBackingImage), currentTimestamp));
    }

    return totalLayers;
}

std::unique_ptr<ImageImpl> JobLocalImageAllocator::allocateBackingImage(
    DeviceContainer* deviceImpl,
    const ImageSetup& setup) {
    auto [imageHandleLifeguard, allocationHandleLifeguard] = deviceImpl->getMemoryAllocator()->allocateImage(setup);

    return std::make_unique<ImageImpl>(
        deviceImpl,
        setup,
        std::move(imageHandleLifeguard),
        std::move(allocationHandleLifeguard),
        DebugTarget::makeSilent());
}
}
