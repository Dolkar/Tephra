#pragma once

#include "../common_impl.hpp"
#include "../utils/object_pool.hpp"
#include <tephra/device.hpp>
#include <atomic>

namespace tp {

// Manages the synchronization timeline for all of the device queues. It uses a timeline semaphore for each queue
// with uniquely identifying timestamps. Each timestamp can be queried and waited upon.
// The first job starts at timestamp = 1
// Timestamp state is classified in three stages:
// Tracked timestamp - Assigned to a job being enqueued, but the job may still fail to get enqueued
// Pending timestamp - The job it was assigned to has been enqueued and is guaranteed to be signalled
// Reached timestamp - The timestamp has been reached on the device
class TimelineManager {
public:
    explicit TimelineManager(DeviceContainer* deviceImpl);

    // To be called before use once the queue count is known
    void initializeQueueSemaphores(uint32_t queueCount);

    // Creates a new unique timestamp to be tracked
    uint64_t trackNextTimestamp(uint32_t queueDeviceIndex);

    // Notifies the manager that the job associated with the given timestamp has been successfully enqueued and is
    // guaranteed to eventually be signalled.
    void markPendingTimestamp(uint64_t timestamp, uint32_t queueDeviceIndex);

    // Returns the last timestamp that has been tracked in any queue
    uint64_t getLastTrackedTimestamp() const {
        return lastTrackedTimestampGlobal.load(std::memory_order_relaxed);
    }

    // Returns the last timestamp that has been reached in the given queue
    uint64_t getLastReachedTimestamp(uint32_t queueDeviceIndex) const;

    // Returns the last timestamp that has been reached in all queues
    uint64_t getLastReachedTimestampInAllQueues() const {
        return lastReachedTimestampGlobal.load(std::memory_order_relaxed);
    }

    // Returns true when the queue semaphore has reached the given timestamp
    bool wasTimestampReachedInQueue(uint32_t queueDeviceIndex, uint64_t timestamp) const;

    // Returns true when semaphore timestamps in all queues have reached the given value
    bool wasTimestampReachedInAllQueues(uint64_t timestamp) const;

    // Waits for all of the given timestamps to be reached or until timeout is reached
    bool waitForTimestamps(
        ArrayParameter<const uint32_t> queueDeviceIndices,
        ArrayParameter<const uint64_t> timestamps,
        bool waitAll,
        Timeout timeout);

    // Registers a callback to be called when the last tracked timestamp has been reached in all queues.
    // Used for resource cleanup
    void addCleanupCallback(CleanupCallback callback);

    // Updates the last reached timestamp of the queue and returns the new timestamp
    uint64_t updateQueue(uint32_t queueDeviceIndex);

    // Updates the last reached timestamp among all queues, issuing cleanup callbacks as needed
    void update();

    VkSemaphoreHandle vkGetQueueSemaphoreHandle(uint32_t queueDeviceIndex) const;

    TEPHRA_MAKE_NONCOPYABLE(TimelineManager);
    TEPHRA_MAKE_NONMOVABLE(TimelineManager);
    ~TimelineManager();

private:
    struct CallbackInfo {
        uint64_t timestamp = 0;
        std::vector<CleanupCallback> cleanupCallbacks;

        void clear() {
            timestamp = 0;
            cleanupCallbacks.clear();
        }
    };

    struct QueueSemaphore {
        VkSemaphoreHandle vkSemaphoreHandle;
        // Last timestamp value used for a job currently executing in this queue
        std::atomic<uint64_t> lastPendingTimestamp = 0;
        // Last known reached value of the timestamp on the CPU side
        std::atomic<uint64_t> lastReachedTimestamp = 0;

        explicit QueueSemaphore(VkSemaphoreHandle vkSemaphoreHandle) : vkSemaphoreHandle(vkSemaphoreHandle) {}

        TEPHRA_MAKE_NONCOPYABLE(QueueSemaphore);
        QueueSemaphore(QueueSemaphore&& other) noexcept;
        QueueSemaphore& operator=(QueueSemaphore&& other) noexcept;
        ~QueueSemaphore() {
            TEPHRA_ASSERT(vkSemaphoreHandle.isNull());
        }
    };

    DeviceContainer* deviceImpl;

    // The timestamp that has been last tracked across any queue. It is incremented every time, so that timestamps
    // are unique and ordered by call order
    std::atomic<uint64_t> lastTrackedTimestampGlobal = 0;
    // The last timestamp value used for a job currently executing
    std::atomic<uint64_t> lastPendingTimestampGlobal = 0;
    // The last known value of the timestamp reached in all queues on the CPU side
    std::atomic<uint64_t> lastReachedTimestampGlobal = 0;

    // One timeline semaphore for each queue
    std::vector<QueueSemaphore> queueSemaphores;

    // A queue of cleanup callbacks to be issued when the timestamp has been reached in all queues.
    std::deque<CallbackInfo*> activeCallbacks;
    ObjectPool<CallbackInfo> callbackPool;
    Mutex callbackMutex;
};

}
