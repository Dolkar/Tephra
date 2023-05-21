#pragma once

#include "../common_impl.hpp"
#include <tephra/job.hpp>
#include <deque>
#include <string>

namespace tp {

std::string getDeviceQueueName(const DeviceQueue& queue);

const char* getDeviceQueueTypeName(QueueType type);

struct QueueInfo {
    DeviceQueue identifier;
    uint32_t queueFamilyIndex;
    uint32_t queueIndexInFamily;
    VkQueueHandle vkQueueHandle;
    // Sometimes multiple logical queues may map to the same Vulkan queue. Then we need a mutex.
    Mutex* queueHandleMutex;
    std::string name;
};

class PhysicalDevice;
class LogicalDevice;

class QueueMap {
public:
    QueueMap(const PhysicalDevice* physicalDevice, ArrayView<const DeviceQueue> requestedQueues);

    void assignVkQueueHandles(const LogicalDevice* logicalDevice, ArrayView<const VkQueueHandle> vkQueueHandles);

    const QueueInfo& getQueueInfo(const DeviceQueue& queue) const {
        uint32_t queueIndex = getQueueUniqueIndex(queue);
        TEPHRA_ASSERT(queueIndex != ~0);
        return queueInfos[queueIndex];
    }

    uint32_t getQueueUniqueIndex(const DeviceQueue& queue) const {
        if (static_cast<uint32_t>(queue.type) >= QueueTypeEnumView::size())
            return ~0;
        else if (queue.index >= queueTypeCounts[static_cast<uint32_t>(queue.type)])
            return ~0;
        return queueTypeOffsets[static_cast<uint32_t>(queue.type)] + queue.index;
    }

    std::pair<uint32_t, uint32_t> getQueueFamilyUniqueIndices(QueueType queueType) const {
        if (static_cast<uint32_t>(queueType) >= QueueTypeEnumView::size())
            return { ~0, ~0 };
        uint32_t queueTypeOffset = queueTypeOffsets[static_cast<uint32_t>(queueType)];
        uint32_t queueTypeCount = queueTypeCounts[static_cast<uint32_t>(queueType)];
        return { queueTypeOffset, queueTypeOffset + queueTypeCount };
    }

    ArrayView<const QueueInfo> getQueueInfos() const {
        return view(queueInfos);
    }

    ArrayView<const uint32_t> getQueueFamilyCounts() const {
        return view(queueFamilyCounts);
    }

private:
    // Number of queues created for each Vulkan queue family
    std::vector<uint32_t> queueFamilyCounts;

    uint32_t queueTypeOffsets[QueueTypeEnumView::size()]{};
    uint32_t queueTypeCounts[QueueTypeEnumView::size()]{};
    std::vector<QueueInfo> queueInfos;
    std::deque<Mutex> physicalQueueMutexes;
};

}
