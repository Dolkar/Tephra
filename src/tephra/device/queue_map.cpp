
#include "queue_map.hpp"
#include "logical_device.hpp"
#include <tephra/physical_device.hpp>
#include <algorithm>

namespace tp {

std::string getDeviceQueueName(const DeviceQueue& queue) {
    static constexpr std::size_t nameBufSize = 32;
    char nameBuf[nameBufSize];

    if (queue.type == QueueType::Graphics && queue.index == 0) {
        snprintf(nameBuf, nameBufSize, "%s queue", getDeviceQueueTypeName(queue.type));
    } else {
        snprintf(nameBuf, nameBufSize, "%s[%u] queue", getDeviceQueueTypeName(queue.type), queue.index);
    }

    return std::string(nameBuf);
}

const char* getDeviceQueueTypeName(QueueType type) {
    switch (type) {
    case QueueType::Graphics:
        return "Graphics";
    case QueueType::Compute:
        return "Compute";
        break;
    case QueueType::Transfer:
        return "Transfer";
        break;
    case QueueType::External:
        return "External";
        break;
    default:
        return "Undefined";
    }
}

QueueMap::QueueMap(const PhysicalDevice* physicalDevice, ArrayView<const DeviceQueue> requestedQueues) {
    TEPHRA_ASSERT(!requestedQueues.empty());
    // All queues are expected to be populated
    queueInfos.reserve(requestedQueues.size());

    // Iterate over queues in descending order of importance, mapping them to the chosen queue families
    // The appropriate queue families were already chosen by the physical device. This just maps individual
    // queues to their indices in the family
    uint32_t queueStateOffset = 0;
    for (QueueType queueType : QueueTypeEnumView()) {
        QueueTypeInfo queueTypeInfo = physicalDevice->getQueueTypeInfo(queueType);
        if (queueTypeInfo.queueCount == 0)
            continue;

        if (queueTypeInfo.queueFamilyIndex >= queueFamilyCounts.size()) {
            queueFamilyCounts.resize(queueTypeInfo.queueFamilyIndex + 1, 0);
        }

        uint32_t queueTypeCount = 0;
        uint32_t queueFamilyCount = queueFamilyCounts[queueTypeInfo.queueFamilyIndex];

        for (const DeviceQueue& queue : requestedQueues) {
            if (queue.type == queueType) {
                // If we run out of available device queues in the family, wrap around to the first queue.
                // Multiple Tephra queues can be mapped to the same Vulkan queue.
                uint32_t queueIndexInFamily = queueFamilyCount % queueTypeInfo.queueCount;

                QueueInfo info;
                info.identifier = queue;
                info.queueFamilyIndex = queueTypeInfo.queueFamilyIndex;
                info.queueIndexInFamily = queueIndexInFamily;
                info.vkQueueHandle = {}; // Queue handle and mutex will be assigned once created
                info.queueHandleMutex = nullptr;
                info.name = getDeviceQueueName(queue);
                queueInfos.push_back(info);

                queueFamilyCount++;
                queueTypeCount++;
            }
        }

        queueFamilyCounts[queueTypeInfo.queueFamilyIndex] = queueFamilyCount;
        queueTypeOffsets[static_cast<int>(queueType)] = queueStateOffset;
        queueTypeCounts[static_cast<int>(queueType)] = queueTypeCount;
        queueStateOffset += queueTypeCount;
    }

    // Fix the number of requested queues per family to be at most its queue count.
    for (QueueType queueType : QueueTypeEnumView()) {
        QueueTypeInfo queueTypeInfo = physicalDevice->getQueueTypeInfo(queueType);
        if (queueTypeInfo.queueCount == 0)
            continue;

        uint32_t& queueFamilyCount = queueFamilyCounts[queueTypeInfo.queueFamilyIndex];
        queueFamilyCount = tp::min(queueFamilyCount, queueTypeInfo.queueCount);
    }
}

void QueueMap::assignVkQueueHandles(const LogicalDevice* logicalDevice, ArrayView<const VkQueueHandle> vkQueueHandles) {
    TEPHRA_ASSERT(vkQueueHandles.size() == queueInfos.size());

    for (uint32_t i = 0; i < queueInfos.size(); i++) {
        queueInfos[i].vkQueueHandle = vkQueueHandles[i];
    }

    // Gather queues with shared Vulkan handles
    ScratchVector<uint32_t> indices(queueInfos.size());
    for (uint32_t i = 0; i < queueInfos.size(); i++) {
        indices[i] = i;
    }
    std::sort(indices.begin(), indices.end(), [queueInfos = this->queueInfos](uint32_t l, uint32_t r) {
        return queueInfos[l].vkQueueHandle.vkRawHandle < queueInfos[r].vkQueueHandle.vkRawHandle;
    });

    // Add mutexes and name the Vulkan queues according to what logical queues map to them
    auto processSharedQueues = [this, logicalDevice, &indices](uint32_t start, uint32_t end) {
        TEPHRA_ASSERT(start != end);

        // Handle unique Vulkan queue handle
        QueueInfo& firstQueueInfo = queueInfos[indices[start]];
        std::string queueName = std::string(getDeviceQueueTypeName(firstQueueInfo.identifier.type)) + "[";

        // Assign one shared mutex per Vulkan queue and list mapping in debug name
        physicalQueueMutexes.emplace_back();
        Mutex* mutex = &physicalQueueMutexes.back();

        for (uint32_t i = start; i < end; i++) {
            QueueInfo& logicalQueueInfo = queueInfos[indices[i]];
            TEPHRA_ASSERT(firstQueueInfo.vkQueueHandle == logicalQueueInfo.vkQueueHandle);
            logicalQueueInfo.queueHandleMutex = mutex;

            queueName += std::to_string(logicalQueueInfo.identifier.index);
            if (i + 1 != end)
                queueName += ",";
        }
        queueName += "] queue";

        logicalDevice->setObjectDebugName(firstQueueInfo.vkQueueHandle, queueName.c_str());
    };

    VkQueueHandle prevHandle = queueInfos[indices.front()].vkQueueHandle;
    uint32_t streakStart = 0;
    for (uint32_t streakEnd = 1; streakEnd < indices.size(); streakEnd++) {
        if (queueInfos[indices[streakEnd]].vkQueueHandle != prevHandle) {
            processSharedQueues(streakStart, streakEnd);
            streakStart = streakEnd;
        }
    }
    processSharedQueues(streakStart, static_cast<uint32_t>(indices.size()));
}

ScratchVector<std::unique_lock<Mutex>> QueueMap::lockPhysicalQueues() {
    // We don't need to worry about deadlocking, because queue mutexes only get locked one at a time (like on submit)
    // or here, where we always lock them all in the same order (for deviceWaitIdle)
    ScratchVector<std::unique_lock<Mutex>> queueLocks;
    queueLocks.reserve(physicalQueueMutexes.size());

    for (Mutex& mutex : physicalQueueMutexes) {
        queueLocks.emplace_back(mutex);
    }

    return queueLocks;
}

}
