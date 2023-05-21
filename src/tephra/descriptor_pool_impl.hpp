#pragma once

#include "common_impl.hpp"
#include <tephra/descriptor.hpp>
#include <tephra/device.hpp>
#include <unordered_map>
#include <deque>

namespace tp {

// An invalid descriptor type to be ignored and not passed on to Vulkan
constexpr DescriptorType IgnoredDescriptorType = static_cast<DescriptorType>(~0);

// A descriptor set to be freed
struct DescriptorSetToFree {
    VkDescriptorSetHandle vkDescriptorSetHandle;
    uint64_t timestampToWaitOn;
};

class TimelineManager;

// A pool entry for managing all sets of a particular descriptor set layout
struct DescriptorPoolEntry {
    // Needed for freeing descriptor sets with just this entry
    TimelineManager* timelineManager = nullptr;
    uint32_t allocatedSetCount = 0;
    uint32_t reservedSetCount = 0;
    std::deque<VkDescriptorSetHandle> freeSets;
    Mutex setsToFreeMutex;
    std::deque<DescriptorSetToFree> setsToFree;
    // Cached value of the first timestamp in setsToFree so we can access it without a lock
    uint64_t firstTimestampToWaitOn = ~0ull;
};

class DescriptorPoolImpl : public DescriptorPool {
public:
    DescriptorPoolImpl(
        DeviceContainer* deviceImpl,
        DescriptorPoolSetup setup,
        uint32_t localQueueIndex, // The device index of a queue its local to, ~0 otherwise
        DebugTarget debugTarget)
        : debugTarget(std::move(debugTarget)),
          deviceImpl(deviceImpl),
          setup(std::move(setup)),
          localQueueIndex(localQueueIndex) {}

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    DeviceContainer* getParentDeviceImpl() {
        return deviceImpl;
    }

    // Allocates descriptor sets according to the given setups and the requested reserves
    // Returns a pointer that needs to be used for freeing the allocated sets
    DescriptorPoolEntry* allocateDescriptorSets_(
        const DescriptorSetLayout* descriptorSetLayout,
        ArrayParameter<const DescriptorSetSetup> descriptorSetSetups,
        ArrayView<VkDescriptorSetHandle> vkAllocatedDescriptorSets);

    // Adds a request to reserve the given number of descriptor sets of this layout
    void reserve_(const DescriptorSetLayout* descriptorSetLayout, uint32_t descriptorSetCount);

    // Queues this descriptor set to be freed in a thread safe way
    static void queueFreeDescriptorSet(
        VkDescriptorSetHandle vkSetToFree,
        DescriptorPoolEntry* mapEntry,
        uint64_t timestampToWaitOn);

    static void makeUpdateTemplate(
        ArrayParameter<const DescriptorBinding> descriptorBindings,
        ScratchVector<VkDescriptorUpdateTemplateEntry>* entries);

    TEPHRA_MAKE_NONCOPYABLE(DescriptorPoolImpl);
    TEPHRA_MAKE_NONMOVABLE(DescriptorPoolImpl);
    ~DescriptorPoolImpl();

private:
    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    DescriptorPoolSetup setup;
    uint32_t localQueueIndex;

    std::vector<Lifeguard<VkDescriptorPoolHandle>> allocatedPools;
    std::unordered_map<VkDescriptorSetLayoutHandle, DescriptorPoolEntry> descriptorSetMap;

    // Deduce image layouts from the descriptor layout and assign them to the descriptor data
    static void deduceDescriptorImageLayouts(
        const DescriptorSetLayout* descriptorSetLayout,
        const DescriptorSetSetup& descriptorSetSetup);

    // Attempt to free descriptor sets that are no longer in use
    void tryFreeDescriptorSets(DescriptorPoolEntry& mapEntry);

    // Allocates a new descriptor pool to satisfy the set allocations
    Lifeguard<VkDescriptorPoolHandle> allocateDescriptorPool(
        const DescriptorSetLayout* descriptorSetLayout,
        const DescriptorPoolEntry& mapEntry,
        uint32_t setsToAllocate,
        ScratchVector<VkDescriptorSetHandle>* vkDescriptorSetHandles) const;
};

}
