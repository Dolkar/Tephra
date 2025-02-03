#pragma once

#include "../common_impl.hpp"
#include "../utils/object_pool.hpp"
#include <tephra/device.hpp>
#include <atomic>

namespace tp {

// Manages the synchronization timeline for all of the device queues. It uses a timeline semaphore for each queue
// with uniquely identifying timestamps. Each timestamp can be queried and waited upon.
// The first job starts at timestamp = 1
// Timestamp is either pending execution, meaning it has been assigned to a job that will eventually finish executing,
// or it has already been "reached", meaning its assigned job is done and its resources can be freed.
class TimelineManager {
public:
    explicit TimelineManager(DeviceContainer* deviceImpl);

    // To be called before use once the queue count is known
    void initializeQueues(uint32_t queueCount);

    // Creates a new unique timestamp for a job that will execute in this queue. Jobs in the same queue must be executed
    // in the order defined by these timestamps.
    uint64_t assignNextTimestamp(uint32_t queueDeviceIndex);

    // Returns the last timestamp that has been assigned to a job awaiting execution in any queue
    uint64_t getLastPendingTimestamp() const {
        return lastPendingTimestampGlobal.load(std::memory_order_relaxed);
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

    // Registers a callback to be called when the current last pending timestamp has been reached in all queues.
    // Used for resource cleanup
    void addCleanupCallback(CleanupCallback callback);

    // Registers a callback to be called when the current last pending timestamp has been reached in the specified
    // queue. Used for resource cleanup
    void addCleanupCallback(uint32_t queueDeviceIndex, CleanupCallback callback);

    // Updates the last reached timestamp of the queue and returns the new timestamp
    uint64_t updateQueue(uint32_t queueDeviceIndex);

    // Updates the last reached timestamp among all queues, issuing cleanup callbacks as needed
    void update();

    VkSemaphoreHandle vkGetQueueSemaphoreHandle(uint32_t queueDeviceIndex) const;

    TEPHRA_MAKE_NONCOPYABLE(TimelineManager);
    TEPHRA_MAKE_NONMOVABLE(TimelineManager);
    ~TimelineManager();

private:
    class Callbacks {
    public:
        static constexpr const uint32_t GlobalQueueIndex = ~0u;

        void initializeQueues(uint32_t queueCount) {
            queueCallbacks.resize(queueCount);
        }

        void addCleanupCallback(uint32_t queueIndex, uint64_t pendingTimestamp, CleanupCallback callback);

        void issueCallbacks(uint32_t queueIndex, uint64_t reachedTimestamp);

    private:
        struct CallbackInfo {
            uint64_t timestamp = 0;
            std::vector<CleanupCallback> cleanupCallbacks;

            void clear() {
                timestamp = 0;
                cleanupCallbacks.clear();
            }
        };

        // A queue of cleanup callbacks to be issued when the timestamp has been reached in all queues.
        std::deque<CallbackInfo*> globalCallbacks;
        // A queue of cleanup callbacks to be issued per queue
        std::vector<std::deque<CallbackInfo*>> queueCallbacks;
        ObjectPool<CallbackInfo> pool;
        Mutex mutex;
    };

    struct QueueSemaphore {
        VkSemaphoreHandle vkSemaphoreHandle;
        // Last timestamp value used for a job currently executing in this queue
        std::atomic<uint64_t> lastPendingTimestamp = 0;
        // Last known reached value of the timestamp
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

    // A monotonically incrementing counter for generating unique, consecutive timestamps
    std::atomic<uint64_t> timestampCounterGlobal = 0;
    // The last timestamp value assigned to any job
    std::atomic<uint64_t> lastPendingTimestampGlobal = 0;
    // The last known value of the timestamp reached in all queues
    std::atomic<uint64_t> lastReachedTimestampGlobal = 0;

    // One timeline semaphore for each queue
    std::vector<QueueSemaphore> queueSemaphores;
    Callbacks callbacks;
};

}
