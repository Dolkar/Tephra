#pragma once

#include "queue_map.hpp"
#include "../job/accesses.hpp"
#include "../job/barriers.hpp"
#include "../common_impl.hpp"
#include <variant>
#include <vector>
#include <unordered_map>

namespace tp {

struct TimelinePeriod {
    DeviceQueue srcQueue;
    uint64_t fromTimestamp = 0; // Not inclusive
    uint64_t toTimestamp = 0; // Inclusive
};

// Handles synchronization of resource state across queues as well as queue family ownership transfers
class CrossQueueSync {
public:
    // An exported resource range
    struct ExportEntry {
        JobSemaphore semaphore;
        std::variant<NewBufferAccess, NewImageAccess> access;
        uint32_t currentQueueFamilyIndex;
        uint32_t dstQueueFamilyIndex;
    };

    explicit CrossQueueSync(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {}

    // Export a resource access to a given queue family
    template <typename TResourceAccess>
    void broadcastResourceExport(
        const JobSemaphore& semaphore,
        const TResourceAccess& exportedAccess,
        uint32_t dstQueueFamilyIndex);

    // Delete a resource from the states of all queues
    template <typename TResourceHandle>
    void broadcastResourceForget(const TResourceHandle& vkResourceHandle);

    // Gives incoming exports from given queue timelines. Entries that need a queue family ownership transfer
    // are required to be transferred.
    void queryIncomingExports(
        ArrayParameter<const TimelinePeriod> periods,
        uint32_t dstQueueFamilyIndex,
        ScratchVector<ExportEntry>& incomingExports);

private:
    using VkResourceHandle = std::variant<VkBufferHandle, VkImageHandle>;

    struct ExportCacheEntry {
        JobSemaphore semaphore;
        uint32_t dstQueueFamilyIndex;
        VkResourceHandle vkResourceHandle;
    };
    static constexpr std::size_t ExportCacheSize = 1024;

    DeviceContainer* deviceImpl;
    std::unordered_map<VkResourceHandle, std::vector<ExportEntry>> exportedResources;
    // Stores recent exports sorted by timestamp for fast access
    std::deque<ExportCacheEntry> exportCache;
    // TODO: May need to be lockless?
    mutable Mutex mutex;
};

}
