#include "cross_queue_sync.hpp"
#include "device_container.hpp"
#include "queue_state.hpp"

#include <algorithm>

namespace tp {

template <typename TResourceAccess>
void CrossQueueSync::broadcastResourceExport(
    const JobSemaphore& semaphore,
    const TResourceAccess& exportedAccess,
    uint32_t dstQueueFamilyIndex) {
    uint32_t srcQueueFamilyIndex = deviceImpl->getQueueMap()->getQueueInfo(semaphore.queue).queueFamilyIndex;

    std::lock_guard<Mutex> resourceLock(mutex);
    std::vector<ExportEntry>& resourceEntry = exportedResources[exportedAccess.vkResourceHandle];

    // Check for and remove fully overlapping accesses
    auto removeIt = std::remove_if(resourceEntry.begin(), resourceEntry.end(), [&exportedAccess](const auto& entry) {
        return doesAccessRangeContainAnother(exportedAccess.range, std::get<TResourceAccess>(entry.access).range);
    });
    resourceEntry.erase(removeIt, resourceEntry.end());

    // Add the new entry
    ExportEntry newEntry{ semaphore, exportedAccess, srcQueueFamilyIndex, dstQueueFamilyIndex };
    resourceEntry.push_back(newEntry);

    // And add it to the export cache - assume it should be near the end of the sorted list
    ExportCacheEntry cacheEntry{ semaphore, dstQueueFamilyIndex, exportedAccess.vkResourceHandle };

    if (exportCache.size() >= ExportCacheSize) {
        exportCache.pop_front();
    }

    auto insertIt = exportCache.rbegin();
    for (; insertIt != exportCache.rend(); ++insertIt) {
        if (insertIt->semaphore.timestamp <= semaphore.timestamp)
            break;
    }

    exportCache.insert(insertIt.base(), cacheEntry);
}

template <typename TResourceHandle>
void CrossQueueSync::broadcastResourceForget(const TResourceHandle& vkResourceHandle) {
    {
        std::lock_guard<Mutex> resourceLock(mutex);
        exportedResources.erase(vkResourceHandle);
    }

    // Remove it from per-queue synchronization state as well
    for (uint32_t queueIndex = 0; queueIndex < deviceImpl->getQueueMap()->getQueueInfos().size(); queueIndex++) {
        deviceImpl->getQueueState(queueIndex)->forgetResource(vkResourceHandle);
    }
}

void CrossQueueSync::queryIncomingExports(
    ArrayParameter<const TimelinePeriod> periods,
    uint32_t dstQueueFamilyIndex,
    ScratchVector<ExportEntry>& incomingExports) {
    std::lock_guard<Mutex> resourceLock(mutex);

    auto isSemaphoreInRange = [&](const JobSemaphore& semaphore) -> bool {
        for (const TimelinePeriod& period : periods) {
            if (period.srcQueue == semaphore.queue && semaphore.timestamp > period.fromTimestamp &&
                semaphore.timestamp <= period.toTimestamp)
                return true;
        }
        return false;
    };

    auto processExports = [&](std::vector<ExportEntry>& resourceEntry) {
        for (ExportEntry& exportEntry : resourceEntry) {
            // Consider only relevant entries
            if (exportEntry.dstQueueFamilyIndex == dstQueueFamilyIndex && isSemaphoreInRange(exportEntry.semaphore)) {
                incomingExports.push_back(exportEntry);
                // Assume the queue family ownership transfer will be done upon return
                exportEntry.currentQueueFamilyIndex = exportEntry.dstQueueFamilyIndex;
            }
        }
    };

    // Iterate over exports within the largest from - to timestamp range
    uint64_t minFromTimestamp = ~0;
    uint64_t maxToTimestamp = 0;
    for (const TimelinePeriod& period : periods) {
        minFromTimestamp = tp::min(minFromTimestamp, period.fromTimestamp);
        maxToTimestamp = tp::max(maxToTimestamp, period.toTimestamp);
    }

    auto comp = [](uint64_t value, const ExportCacheEntry& element) {
        return value < element.semaphore.timestamp;
    };
    auto startIt = std::upper_bound(exportCache.begin(), exportCache.end(), minFromTimestamp, comp);
    if (startIt == exportCache.begin() && exportCache.size() >= ExportCacheSize) {
        // Outside the range of the cache, fall back to iterating over all resources
        for (auto& el : exportedResources)
            processExports(el.second);
    } else {
        // Iterate over the relevant range of the cache
        for (auto it = startIt; it != exportCache.end(); ++it) {
            const ExportCacheEntry& entry = *it;
            if (entry.semaphore.timestamp > maxToTimestamp)
                break;

            if (entry.dstQueueFamilyIndex == dstQueueFamilyIndex && isSemaphoreInRange(entry.semaphore)) {
                auto entryHit = exportedResources.find(entry.vkResourceHandle);
                if (entryHit != exportedResources.end()) {
                    // Otherwise the entry was forgotten, but is still in cache
                    processExports(entryHit->second);
                }
            }
        }
    }
}

template void CrossQueueSync::broadcastResourceExport(
    const JobSemaphore& semaphore,
    const NewBufferAccess& exportedAccess,
    uint32_t dstQueueFamilyIndex);
template void CrossQueueSync::broadcastResourceExport(
    const JobSemaphore& semaphore,
    const NewImageAccess& exportedAccess,
    uint32_t dstQueueFamilyIndex);
template void CrossQueueSync::broadcastResourceForget(const VkBufferHandle& vkResourceHandle);
template void CrossQueueSync::broadcastResourceForget(const VkImageHandle& vkResourceHandle);

}
