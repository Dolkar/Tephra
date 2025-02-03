#pragma once

#include "logical_device.hpp"
#include "memory_allocator.hpp"
#include "deferred_destructor.hpp"
#include "cross_queue_sync.hpp"
#include "query_manager.hpp"
#include "queue_state.hpp"
#include "queue_map.hpp"
#include "timeline_manager.hpp"
#include "command_pool.hpp"
#include "../application/application_container.hpp"
#include "../common_impl.hpp"
#include <tephra/device.hpp>
#include <tephra/physical_device.hpp>

namespace tp {

class DeviceContainer : public Device {
public:
    DeviceContainer(ApplicationContainer* appContainer, const DeviceSetup& deviceSetup, DebugTarget debugTarget)
        : debugTarget(std::move(debugTarget)),
          appContainer(appContainer),
          physicalDevice(deviceSetup.physicalDevice),
          queueMap(physicalDevice, deviceSetup.queues),
          logicalDevice(appContainer->getInstance(), &queueMap, deviceSetup),
          memoryAllocator(this, appContainer->getInstance(), deviceSetup.memoryAllocatorSetup),
          commandPoolPool(this),
          crossQueueSync(this),
          deferredDestructor(&logicalDevice, &memoryAllocator, &crossQueueSync),
          timelineManager(this),
          queryManager(this, &commandPoolPool.getVkiCommands()) {
        // Initialize queue states
        for (uint32_t queueIndex = 0; queueIndex < queueMap.getQueueInfos().size(); queueIndex++) {
            queueStates.push_back(std::make_unique<QueueState>(this, queueIndex));
        }

        timelineManager.initializeQueues(static_cast<uint32_t>(queueStates.size()));
    }

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    const ApplicationContainer* getParentAppImpl() const {
        return appContainer;
    }

    ApplicationContainer* getParentAppImpl() {
        return appContainer;
    }

    const PhysicalDevice* getPhysicalDevice() const {
        return physicalDevice;
    }

    const QueueMap* getQueueMap() const {
        return &queueMap;
    }

    const LogicalDevice* getLogicalDevice() const {
        return &logicalDevice;
    }

    LogicalDevice* getLogicalDevice() {
        return &logicalDevice;
    }

    const MemoryAllocator* getMemoryAllocator() const {
        return &memoryAllocator;
    }

    MemoryAllocator* getMemoryAllocator() {
        return &memoryAllocator;
    }

    const DeferredDestructor* getDeferredDestructor() const {
        return &deferredDestructor;
    }

    DeferredDestructor* getDeferredDestructor() {
        return &deferredDestructor;
    }

    const CommandPoolPool* getCommandPoolPool() const {
        return &commandPoolPool;
    }

    CommandPoolPool* getCommandPoolPool() {
        return &commandPoolPool;
    }

    const CrossQueueSync* getCrossQueueSync() const {
        return &crossQueueSync;
    }

    CrossQueueSync* getCrossQueueSync() {
        return &crossQueueSync;
    }

    const TimelineManager* getTimelineManager() const {
        return &timelineManager;
    }

    TimelineManager* getTimelineManager() {
        return &timelineManager;
    }

    QueryManager* getQueryManager() {
        return &queryManager;
    }

    const QueueState* getQueueState(uint32_t queueUniqueIndex) const {
        return queueStates[queueUniqueIndex].get();
    }

    QueueState* getQueueState(uint32_t queueUniqueIndex) {
        return queueStates[queueUniqueIndex].get();
    }

    void updateDeviceProgress_();

    TEPHRA_MAKE_NONCOPYABLE(DeviceContainer);
    TEPHRA_MAKE_NONMOVABLE(DeviceContainer);
    ~DeviceContainer();

private:
    DebugTarget debugTarget;
    ApplicationContainer* appContainer;
    const PhysicalDevice* physicalDevice;

    QueueMap queueMap;
    LogicalDevice logicalDevice;
    MemoryAllocator memoryAllocator;
    CommandPoolPool commandPoolPool;
    CrossQueueSync crossQueueSync;
    std::vector<std::unique_ptr<QueueState>> queueStates;
    DeferredDestructor deferredDestructor;
    TimelineManager timelineManager;
    QueryManager queryManager;
};

}
