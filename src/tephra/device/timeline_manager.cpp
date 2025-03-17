#include "timeline_manager.hpp"
#include "device_container.hpp"
#include "logical_device.hpp"

namespace tp {

// Updates the value of atomicVar to max(atomicVar, value), returns the updated value
inline uint64_t atomicStoreMax(std::atomic<uint64_t>& atomicVar, uint64_t value, std::memory_order storeOrder) {
    uint64_t previousValue = atomicVar.load(std::memory_order_relaxed);
    while (previousValue < value &&
           !atomicVar.compare_exchange_weak(previousValue, value, storeOrder, std::memory_order_relaxed)) {
    }
    return tp::max(previousValue, value);
}

TimelineManager::TimelineManager(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {}

void TimelineManager::initializeQueues(uint32_t queueCount) {
    TEPHRA_ASSERT(queueSemaphores.empty());

    for (uint32_t queueIndex = 0; queueIndex < queueCount; queueIndex++) {
        VkSemaphoreHandle vkSemaphoreHandle = deviceImpl->getLogicalDevice()->createSemaphore(true);

        // Name semaphore after its logical queue
        std::string semaphoreName = deviceImpl->getQueueMap()->getQueueInfos()[queueIndex].name + " semaphore";
        deviceImpl->getLogicalDevice()->setObjectDebugName(vkSemaphoreHandle, semaphoreName.c_str());

        queueSemaphores.emplace_back(vkSemaphoreHandle);
    }

    callbacks.initializeQueues(queueCount);
}

uint64_t TimelineManager::assignNextTimestamp(uint32_t queueDeviceIndex) {
    TEPHRA_ASSERT(queueDeviceIndex < queueSemaphores.size());
    QueueSemaphore& queueSemaphore = queueSemaphores[queueDeviceIndex];

    // Need to enforce these invariants:
    // - All timestamp values are monotonically increasing
    // - The queue's lastPendingTimestamp is always updated to a value larger than lastPendingTimestampGlobal
    // Under the assumption that we have exclusive write access to queueSemaphore.lastPendingTimestamp, we ensure
    // that by always updating lastPendingTimestampGlobal in order, one by one
    uint64_t previousTimestamp = timestampCounterGlobal.fetch_add(1, std::memory_order_relaxed);
    uint64_t newTimestamp = previousTimestamp + 1;
    atomicStoreMax(queueSemaphore.lastPendingTimestamp, newTimestamp, std::memory_order_release);

    while (true) {
        uint64_t expectedTimestamp = previousTimestamp;
        bool success = lastPendingTimestampGlobal.compare_exchange_weak(
            expectedTimestamp, newTimestamp, std::memory_order_release, std::memory_order_relaxed);
        if (success)
            return newTimestamp;
    }
}

uint64_t TimelineManager::getLastReachedTimestamp(uint32_t queueDeviceIndex) const {
    TEPHRA_ASSERT(queueDeviceIndex < queueSemaphores.size());
    const QueueSemaphore& queueSemaphore = queueSemaphores[queueDeviceIndex];
    return queueSemaphore.lastReachedTimestamp.load(std::memory_order_relaxed);
}

bool TimelineManager::wasTimestampReachedInQueue(uint32_t queueDeviceIndex, uint64_t timestamp) const {
    TEPHRA_ASSERTD(timestamp != 0, "Timestamp of 0 is guaranteed to be reached - suspicious pointless call");
    return getLastReachedTimestamp(queueDeviceIndex) >= timestamp;
}

bool TimelineManager::wasTimestampReachedInAllQueues(uint64_t timestamp) const {
    TEPHRA_ASSERTD(timestamp != 0, "Timestamp of 0 is guaranteed to be reached - suspicious pointless call");
    return getLastReachedTimestampInAllQueues() >= timestamp;
}

bool TimelineManager::waitForTimestamps(
    ArrayParameter<const uint32_t> queueDeviceIndices,
    ArrayParameter<const uint64_t> timestamps,
    bool waitAll,
    Timeout timeout) {
    ScratchVector<VkSemaphoreHandle> waitVkSemaphoreHandles;
    waitVkSemaphoreHandles.reserve(queueDeviceIndices.size());
    ScratchVector<uint64_t> waitTimestamps;
    waitTimestamps.reserve(queueDeviceIndices.size());

    // First check if the timestamps were reached already
    update();
    for (int i = 0; i < queueDeviceIndices.size(); i++) {
        bool wasReached = wasTimestampReachedInQueue(queueDeviceIndices[i], timestamps[i]);
        if (!waitAll && wasReached) {
            return true;
        } else if (!wasReached) {
            VkSemaphoreHandle vkSemaphoreHandle = queueSemaphores[queueDeviceIndices[i]].vkSemaphoreHandle;
            waitVkSemaphoreHandles.push_back(vkSemaphoreHandle);
            waitTimestamps.push_back(timestamps[i]);
        }
    }

    if (waitVkSemaphoreHandles.empty()) {
        return true;
    } else {
        // Wait for the rest
        bool hasFinished = deviceImpl->getLogicalDevice()->waitForSemaphores(
            view(waitVkSemaphoreHandles), view(waitTimestamps), waitAll, timeout);

        return hasFinished;
    }
}

void TimelineManager::addCleanupCallback(CleanupCallback callback) {
    uint64_t lastPendingTimestamp = getLastPendingTimestamp();

    if (wasTimestampReachedInAllQueues(lastPendingTimestamp)) {
        callback();
    } else {
        callbacks.addCleanupCallback(Callbacks::GlobalQueueIndex, lastPendingTimestamp, std::move(callback));
    }
}

void TimelineManager::addCleanupCallback(uint32_t queueDeviceIndex, CleanupCallback callback) {
    TEPHRA_ASSERT(queueDeviceIndex < queueSemaphores.size());
    uint64_t lastPendingTimestamp = getLastPendingTimestamp();

    if (wasTimestampReachedInQueue(queueDeviceIndex, lastPendingTimestamp)) {
        callback();
    } else {
        callbacks.addCleanupCallback(queueDeviceIndex, lastPendingTimestamp, std::move(callback));
    }
}

uint64_t TimelineManager::updateQueue(uint32_t queueDeviceIndex) {
    TEPHRA_ASSERT(queueDeviceIndex < queueSemaphores.size());
    QueueSemaphore& queueSemaphore = queueSemaphores[queueDeviceIndex];

    // Load the last global pending value first, so that it becomes conservative - After we load its value, we have a
    // guarantee that at least one queue has the same value or higher of its local lastPendingTimestamp. This is used
    // to detect whether or not there are any pending jobs in this queue.
    uint64_t lastPendingValueGlobal = lastPendingTimestampGlobal.load(std::memory_order_acquire);
    uint64_t lastPendingValue = queueSemaphore.lastPendingTimestamp.load(std::memory_order_acquire);
    uint64_t lastReachedValue = queueSemaphore.lastReachedTimestamp.load(std::memory_order_relaxed);

    if (lastReachedValue >= lastPendingValue) {
        // No timestamps left that could be signalled -> fast-forward the reached timestamp
        // Note: This is why lastReachedTimestamp can be greater than lastPendingTimestamp
        return atomicStoreMax(queueSemaphore.lastReachedTimestamp, lastPendingValueGlobal, std::memory_order_relaxed);
    }

    uint64_t newReachedValue = deviceImpl->getLogicalDevice()->getSemaphoreCounterValue(
        queueSemaphore.vkSemaphoreHandle);

    if (newReachedValue >= lastPendingValue) {
        // Again fast-forward if we can
        newReachedValue = tp::max(newReachedValue, lastPendingValueGlobal);
    }

    return atomicStoreMax(queueSemaphore.lastReachedTimestamp, newReachedValue, std::memory_order_relaxed);
}

void TimelineManager::update() {
    // Update all the queues individually, accumulating the latest timestamp value reached in all queues
    uint64_t minReachedTimestamp = getLastPendingTimestamp();
    for (int i = 0; i < queueSemaphores.size(); i++) {
        uint64_t queueReachedTimestamp = updateQueue(i);
        minReachedTimestamp = tp::min(minReachedTimestamp, queueReachedTimestamp);
        // Also issue per-queue callbacks
        callbacks.issueCallbacks(i, queueReachedTimestamp);
    }

    minReachedTimestamp = atomicStoreMax(lastReachedTimestampGlobal, minReachedTimestamp, std::memory_order_relaxed);

    // Issue global callbacks
    callbacks.issueCallbacks(Callbacks::GlobalQueueIndex, minReachedTimestamp);

    if constexpr (TephraValidationEnabled) {
        uint64_t lastPendingTimestamp = getLastPendingTimestamp();
        TEPHRA_ASSERT(lastPendingTimestamp >= minReachedTimestamp);
        if (lastPendingTimestamp - minReachedTimestamp >= 100) {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::Performance,
                "Too many jobs were enqueued before the last one finished (>100). "
                "This may delay the release of resources.");
        }
    }
}

VkSemaphoreHandle TimelineManager::vkGetQueueSemaphoreHandle(uint32_t queueDeviceIndex) const {
    TEPHRA_ASSERT(queueDeviceIndex < queueSemaphores.size());
    const QueueSemaphore& queueSemaphore = queueSemaphores[queueDeviceIndex];
    return queueSemaphore.vkSemaphoreHandle;
}

TimelineManager::~TimelineManager() {
    // Try to issue remaining callbacks
    // Do not propagate exceptions out of the destructor, they should already be logged
    try {
        deviceImpl->getLogicalDevice()->waitForDeviceIdle();
        update();
    } catch (const RuntimeError&) {
    }

    for (auto& queueSemaphore : queueSemaphores) {
        deviceImpl->getLogicalDevice()->destroySemaphore(queueSemaphore.vkSemaphoreHandle);
        queueSemaphore.vkSemaphoreHandle = {};
    }
}

TimelineManager::QueueSemaphore::QueueSemaphore(QueueSemaphore&& other) noexcept
    : vkSemaphoreHandle(other.vkSemaphoreHandle),
      lastPendingTimestamp(other.lastPendingTimestamp.load()),
      lastReachedTimestamp(other.lastReachedTimestamp.load()) {
    other.vkSemaphoreHandle = {};
}

TimelineManager::QueueSemaphore& TimelineManager::QueueSemaphore::operator=(QueueSemaphore&& other) noexcept {
    vkSemaphoreHandle = other.vkSemaphoreHandle;
    lastPendingTimestamp.store(other.lastPendingTimestamp.load());
    lastReachedTimestamp.store(other.lastReachedTimestamp.load());
    other.vkSemaphoreHandle = {};
    return *this;
}

void TimelineManager::Callbacks::addCleanupCallback(
    uint32_t queueIndex,
    uint64_t pendingTimestamp,
    CleanupCallback callback) {
    std::lock_guard<Mutex> mutexLock(mutex);

    std::deque<CallbackInfo*>& activeCallbacks = queueIndex == GlobalQueueIndex ? globalCallbacks :
                                                                                  queueCallbacks[queueIndex];

    if (!activeCallbacks.empty() && activeCallbacks.back()->timestamp >= pendingTimestamp) {
        // An entry already exists, we can just append the callback
        activeCallbacks.back()->cleanupCallbacks.push_back(std::move(callback));
    } else {
        // Add a new entry into the queue
        CallbackInfo* newCallbackInfo = pool.acquireExisting();
        if (newCallbackInfo == nullptr)
            newCallbackInfo = pool.acquireNew();
        newCallbackInfo->timestamp = pendingTimestamp;
        newCallbackInfo->cleanupCallbacks.push_back(std::move(callback));

        activeCallbacks.push_back(newCallbackInfo);
    }
}

void TimelineManager::Callbacks::issueCallbacks(uint32_t queueIndex, uint64_t reachedTimestamp) {
    std::deque<CallbackInfo*>& activeCallbacks = queueIndex == GlobalQueueIndex ? globalCallbacks :
                                                                                  queueCallbacks[queueIndex];
    // Early-out without locking
    if (activeCallbacks.empty())
        return;

    std::lock_guard<Mutex> mutexLock(mutex);
    while (!activeCallbacks.empty()) {
        auto* timestampInfo = activeCallbacks.front();
        if (reachedTimestamp >= timestampInfo->timestamp) {
            // Issue callbacks and release
            for (CleanupCallback& cleanupCallback : timestampInfo->cleanupCallbacks) {
                cleanupCallback();
            }
            timestampInfo->cleanupCallbacks.clear();

            pool.release(timestampInfo);
            activeCallbacks.pop_front();
        } else {
            break;
        }
    }
}
