#include "descriptor_pool_impl.hpp"
#include "device/device_container.hpp"

namespace tp {

OverallocationBehavior::OverallocationBehavior(float requestFactor, float growFactor, uint64_t minAllocationSize)
    : requestFactor(requestFactor), growFactor(growFactor), minAllocationSize(minAllocationSize) {}

uint64_t OverallocationBehavior::apply(uint64_t requestedSize, uint64_t poolSize) const {
    uint64_t request = std::max(static_cast<uint64_t>(requestedSize * requestFactor), requestedSize);
    uint64_t growth = static_cast<uint64_t>(poolSize * std::max(growFactor - 1.0f, 0.0f));
    return std::max(std::max(request, growth), minAllocationSize);
}

OverallocationBehavior OverallocationBehavior::Exact() {
    return OverallocationBehavior(1.0f, 1.0f, 0);
}

DescriptorPoolSetup::DescriptorPoolSetup(OverallocationBehavior overallocationBehavior)
    : overallocationBehavior(overallocationBehavior) {}

void DescriptorPool::allocateDescriptorSets(
    const DescriptorSetLayout* descriptorSetLayout,
    ArrayParameter<const DescriptorSetSetup> descriptorSetSetups,
    ArrayParameter<DescriptorSet* const> allocatedDescriptorSets) {
    auto poolImpl = static_cast<DescriptorPoolImpl*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(poolImpl->getDebugTarget(), "allocateDescriptorSets", nullptr);
    if constexpr (TephraValidationEnabled) {
        if (descriptorSetLayout == nullptr) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The 'descriptorSetLayout' parameter is nullptr.");
            return;
        }
        if (descriptorSetSetups.size() != allocatedDescriptorSets.size()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The sizes of the 'descriptorSetSetups' (",
                descriptorSetSetups.size(),
                ") and 'allocatedDescriptorSets' (",
                allocatedDescriptorSets.size(),
                ") arrays do not match.");
        }
        for (int i = 0; i < descriptorSetSetups.size(); i++) {
            const DescriptorSetSetup& setSetup = descriptorSetSetups[i];
            descriptorSetLayout->debugValidateDescriptors(
                setSetup.descriptors, setSetup.flags.contains(DescriptorSetFlag::IgnoreNullDescriptors));
        }
    }

    ScratchVector<VkDescriptorSetHandle> vkAllocatedDescriptorSets(allocatedDescriptorSets.size());
    DescriptorPoolEntry* mapEntry = poolImpl->allocateDescriptorSets_(
        descriptorSetLayout, descriptorSetSetups, view(vkAllocatedDescriptorSets));
    for (int i = 0; i < allocatedDescriptorSets.size(); i++) {
        *allocatedDescriptorSets[i] = DescriptorSet(vkAllocatedDescriptorSets[i], mapEntry);
        poolImpl->getParentDeviceImpl()->getLogicalDevice()->setObjectDebugName(
            vkAllocatedDescriptorSets[i], descriptorSetSetups[i].debugName);
    }
}

void DescriptorPool::reserve(const DescriptorSetLayout* descriptorSetLayout, uint32_t descriptorSetCount) {
    auto poolImpl = static_cast<DescriptorPoolImpl*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(poolImpl->getDebugTarget(), "reserve", nullptr);
    if constexpr (TephraValidationEnabled) {
        if (descriptorSetLayout == nullptr) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The 'descriptorSetLayout' parameter is nullptr.");
        }
    }

    poolImpl->reserve_(descriptorSetLayout, descriptorSetCount);
}

DescriptorPoolEntry* DescriptorPoolImpl::allocateDescriptorSets_(
    const DescriptorSetLayout* descriptorSetLayout,
    ArrayParameter<const DescriptorSetSetup> descriptorSetSetups,
    ArrayView<VkDescriptorSetHandle> vkAllocatedDescriptorSets) {
    TEPHRA_ASSERT(descriptorSetSetups.size() == vkAllocatedDescriptorSets.size());
    if (descriptorSetSetups.empty())
        return nullptr;

    // Figure out the image layouts
    for (const DescriptorSetSetup& setSetup : descriptorSetSetups) {
        deduceDescriptorImageLayouts(descriptorSetLayout, setSetup);
    }

    VkDescriptorSetLayoutHandle vkSetLayoutHandle = descriptorSetLayout->vkGetDescriptorSetLayoutHandle();
    TEPHRA_ASSERT(!vkSetLayoutHandle.isNull());
    DescriptorPoolEntry& mapEntry = descriptorSetMap[vkSetLayoutHandle];
    if (mapEntry.timelineManager == nullptr) {
        mapEntry.timelineManager = deviceImpl->getTimelineManager();
    }

    uint32_t setsToAllocate = static_cast<uint32_t>(descriptorSetSetups.size());

    // Try to satisfy requested sets with existing free allocations
    if (setsToAllocate <= mapEntry.freeSets.size()) {
        setsToAllocate = 0;
    } else {
        tryFreeDescriptorSets(mapEntry);

        if (setsToAllocate <= mapEntry.freeSets.size()) {
            setsToAllocate = 0;
        } else {
            setsToAllocate -= static_cast<uint32_t>(mapEntry.freeSets.size());
        }
    }

    // Satisfy the rest by allocating a new pool
    if (setsToAllocate > 0) {
        ScratchVector<VkDescriptorSetHandle> vkSetHandles;
        allocatedPools.push_back(allocateDescriptorPool(descriptorSetLayout, mapEntry, setsToAllocate, &vkSetHandles));

        mapEntry.freeSets.insert(mapEntry.freeSets.end(), vkSetHandles.begin(), vkSetHandles.end());
        mapEntry.allocatedSetCount += static_cast<uint32_t>(vkSetHandles.size());
        mapEntry.reservedSetCount = 0;
    }

    // Now that we know that enough sets are allocated, assign and update them to the given data
    for (int i = 0; i < descriptorSetSetups.size(); i++) {
        const DescriptorSetSetup& setSetup = descriptorSetSetups[i];
        VkDescriptorSetLayoutHandle vkSetLayoutHandle = descriptorSetLayout->vkGetDescriptorSetLayoutHandle();

        TEPHRA_ASSERT(!mapEntry.freeSets.empty());
        VkDescriptorSetHandle vkSetHandle = mapEntry.freeSets.back();
        mapEntry.freeSets.pop_back();

        // Update existing unused descriptor set of the same layout
        auto vkUpdateTemplateHandle = descriptorSetLayout->vkGetDescriptorUpdateTemplateHandle();

        if (setSetup.flags.contains(tp::DescriptorSetFlag::IgnoreNullDescriptors)) {
            // We can't use templated update with null descriptor sets
            deviceImpl->getLogicalDevice()->updateDescriptorSet(
                vkSetHandle, descriptorSetLayout->getBindings(), setSetup.descriptors);
        } else {
            deviceImpl->getLogicalDevice()->updateDescriptorSetWithTemplate(
                vkSetHandle, vkUpdateTemplateHandle, setSetup.descriptors);
        }
        vkAllocatedDescriptorSets[i] = vkSetHandle;
    }

    // Assumes pointers to descriptorSetMap entries don't get invalidated
    return &mapEntry;
}

void DescriptorPoolImpl::reserve_(const DescriptorSetLayout* descriptorSetLayout, uint32_t descriptorSetCount) {
    VkDescriptorSetLayoutHandle vkSetLayoutHandle = descriptorSetLayout->vkGetDescriptorSetLayoutHandle();
    TEPHRA_ASSERT(!vkSetLayoutHandle.isNull());

    DescriptorPoolEntry& mapEntry = descriptorSetMap[vkSetLayoutHandle];
    mapEntry.reservedSetCount += descriptorSetCount;
}

void DescriptorPoolImpl::queueFreeDescriptorSet(
    VkDescriptorSetHandle vkSetToFree,
    DescriptorPoolEntry* mapEntry,
    uint64_t timestampToWaitOn) {
    DescriptorSetToFree setToFree;
    setToFree.vkDescriptorSetHandle = vkSetToFree;
    setToFree.timestampToWaitOn = timestampToWaitOn;

    std::lock_guard<Mutex> mutexLock(mapEntry->setsToFreeMutex);
    if (mapEntry->setsToFree.empty()) {
        mapEntry->firstTimestampToWaitOn = setToFree.timestampToWaitOn;
    } else {
        TEPHRA_ASSERT(mapEntry->firstTimestampToWaitOn <= setToFree.timestampToWaitOn);
    }
    mapEntry->setsToFree.push_back(setToFree);
}

void DescriptorPoolImpl::makeUpdateTemplate(
    ArrayParameter<const DescriptorBinding> descriptorBindings,
    ScratchVector<VkDescriptorUpdateTemplateEntry>* entries) {
    entries->reserve(descriptorBindings.size());
    uint64_t nextDescriptorOffset = 0;

    const DescriptorBinding* previousBinding = nullptr;
    for (const DescriptorBinding& binding : descriptorBindings) {
        if (binding.arraySize == 0) {
            continue;
        } else if (binding.descriptorType == IgnoredDescriptorType) {
            nextDescriptorOffset += binding.arraySize;
            previousBinding = &binding;
        }

        if (previousBinding != nullptr && binding.bindingNumber == previousBinding->bindingNumber + 1 &&
            binding.descriptorType == previousBinding->descriptorType &&
            binding.stageMask == previousBinding->stageMask &&
            binding.immutableSamplers.empty() == previousBinding->immutableSamplers.empty()) {
            // Can reuse the last template entry
            VkDescriptorUpdateTemplateEntry& templateEntry = entries->back();
            templateEntry.descriptorCount += binding.arraySize;
        } else {
            VkDescriptorUpdateTemplateEntry templateEntry;
            templateEntry.dstBinding = binding.bindingNumber;
            templateEntry.dstArrayElement = 0; // Always fill the whole array
            templateEntry.descriptorCount = binding.arraySize;
            templateEntry.descriptorType = vkCastConvertibleEnum(binding.descriptorType);
            templateEntry.offset = nextDescriptorOffset * sizeof(Descriptor);
            templateEntry.stride = sizeof(Descriptor);
            entries->push_back(templateEntry);
        }

        nextDescriptorOffset += binding.arraySize;
        previousBinding = &binding;
    }
}

DescriptorPoolImpl::~DescriptorPoolImpl() {
    TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(&debugTarget);
}

void DescriptorPoolImpl::deduceDescriptorImageLayouts(
    const DescriptorSetLayout* descriptorSetLayout,
    const DescriptorSetSetup& descriptorSetSetup) {
    ArrayView<const DescriptorBinding> descriptorBindings = descriptorSetLayout->getBindings();

    uint32_t descriptorCount = static_cast<uint32_t>(descriptorSetSetup.descriptors.size());
    uint32_t descriptorIndex = 0;
    for (const DescriptorBinding& descriptorBinding : descriptorBindings) {
        uint32_t endIndex = tp::min(descriptorIndex + descriptorBinding.arraySize, descriptorCount);

        VkImageLayout imageLayout = vkGetImageLayoutForDescriptor(
            descriptorBinding.descriptorType,
            descriptorBinding.flags.contains(DescriptorBindingFlag::AliasStorageImage));
        if (imageLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
            for (; descriptorIndex < endIndex; descriptorIndex++) {
                // This patching is possible because vkDescriptorImageInfo is mutable for this exact purpose
                descriptorSetSetup.descriptors[descriptorIndex].vkDescriptorImageInfo.imageLayout = imageLayout;
            }
        } else {
            descriptorIndex = endIndex;
        }
    }
}

void DescriptorPoolImpl::tryFreeDescriptorSets(DescriptorPoolEntry& mapEntry) {
    // Early-out without locking
    uint64_t lastReachedTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestampInAllQueues();
    if (lastReachedTimestamp < mapEntry.firstTimestampToWaitOn)
        return;

    std::lock_guard<Mutex> mutexLock(mapEntry.setsToFreeMutex);
    while (!mapEntry.setsToFree.empty()) {
        DescriptorSetToFree& toFree = mapEntry.setsToFree.front();

        if (lastReachedTimestamp >= toFree.timestampToWaitOn) {
            mapEntry.setsToFree.pop_front();
            mapEntry.freeSets.push_back(toFree.vkDescriptorSetHandle);
        } else {
            mapEntry.firstTimestampToWaitOn = toFree.timestampToWaitOn;
            return;
        }
    }

    TEPHRA_ASSERT(mapEntry.setsToFree.empty());
    mapEntry.firstTimestampToWaitOn = ~0ull;
}

Lifeguard<VkDescriptorPoolHandle> DescriptorPoolImpl::allocateDescriptorPool(
    const DescriptorSetLayout* descriptorSetLayout,
    const DescriptorPoolEntry& mapEntry,
    uint32_t setsToAllocate,
    ScratchVector<VkDescriptorSetHandle>* vkDescriptorSetHandles) const {
    // Apply overallocation
    uint32_t setDescriptorCount = descriptorSetLayout->getDescriptorCount();
    uint32_t requestedDescriptorCount = setsToAllocate * setDescriptorCount;

    uint32_t allocDescriptorCount = static_cast<uint32_t>(
        setup.overallocationBehavior.apply(requestedDescriptorCount, mapEntry.allocatedSetCount * setDescriptorCount));
    setsToAllocate = tp::max(static_cast<uint32_t>(allocDescriptorCount / setDescriptorCount), setsToAllocate);

    // Apply requested reserves for this layout
    uint32_t newCount = mapEntry.allocatedSetCount + setsToAllocate;
    if (mapEntry.reservedSetCount > newCount) {
        setsToAllocate += mapEntry.reservedSetCount - newCount;
    }

    // Form pool sizes using data we precomputed in the layout
    ScratchVector<VkDescriptorPoolSize> poolSizes(descriptorSetLayout->vkPoolSizes.size());
    for (int i = 0; i < descriptorSetLayout->vkPoolSizes.size(); i++) {
        poolSizes[i].type = descriptorSetLayout->vkPoolSizes[i].type;
        poolSizes[i].descriptorCount = descriptorSetLayout->vkPoolSizes[i].descriptorCount * setsToAllocate;
    }

    // Create the pool
    VkDescriptorPoolHandle vkDescriptorPoolHandle = deviceImpl->getLogicalDevice()->createDescriptorPool(
        setsToAllocate, view(poolSizes));

    // Allocate the new free sets from it
    ScratchVector<VkDescriptorSetLayoutHandle> setLayouts;
    setLayouts.resize(setsToAllocate, descriptorSetLayout->vkGetDescriptorSetLayoutHandle());

    vkDescriptorSetHandles->resize(setsToAllocate);
    deviceImpl->getLogicalDevice()->allocateDescriptorSets(
        vkDescriptorPoolHandle, view(setLayouts), view(*vkDescriptorSetHandles));

    return deviceImpl->vkMakeHandleLifeguard(vkDescriptorPoolHandle);
}

}
