#pragma once

#include <tephra/physical_device.hpp>
#include <tephra/common.hpp>

namespace tp {

/// Refers to a device queue to which jobs can be submitted for execution.
/// Multiple different queues may map to the same Vulkan queue.
struct DeviceQueue {
    QueueType type;
    uint32_t index;

    /// Constructs a null tp::DeviceQueue.
    constexpr DeviceQueue() : type(QueueType::Undefined), index(0) {}

    /// @param type
    ///     The type of the queue.
    /// @param index
    ///     The index of the queue within the type.
    constexpr DeviceQueue(QueueType type, uint32_t index = 0) : type(type), index(index) {}

    /// Returns `true` if the queue is null and not valid for use unless specified otherwise.
    bool isNull() const {
        return type == QueueType::Undefined;
    }
};

/// Equality operator for tp::DeviceQueue
constexpr bool operator==(const DeviceQueue& lhs, const DeviceQueue& rhs) {
    return lhs.type == rhs.type && lhs.index == rhs.index;
}

/// Inequality operator for tp::DeviceQueue
constexpr bool operator!=(const DeviceQueue& lhs, const DeviceQueue& rhs) {
    return !(lhs == rhs);
}

/// A synchronization primitive created after enqueueing a tp::Job through tp::Device::enqueueJob.
///
/// It is created in an unsignalled state. Once the corresponding job finishes executing on the device,
/// the semaphore becomes signalled. Both the device and host can wait for the semaphore to become
/// signalled. Its status can also be checked at any point.
///
/// @see tp::Device::enqueueJob
/// @see tp::Device::isJobSemaphoreSignalled
/// @see tp::Device::waitForJobSemaphores
struct JobSemaphore {
    /// The queue that the corresponding Job was enqueued into.
    DeviceQueue queue;
    /// The global number identifying this Job semaphore and its order in the queue.
    uint64_t timestamp;

    /// Constructs a null tp::JobSemaphore
    JobSemaphore() : queue(QueueType::Undefined), timestamp(0) {}

    /// Returns `true` if the semaphore is null and not valid for use.
    bool isNull() const {
        return queue.type == QueueType::Undefined;
    }
};

/// Equality operator for tp::JobSemaphore
constexpr bool operator==(const JobSemaphore& lhs, const JobSemaphore& rhs) {
    return lhs.queue == rhs.queue && lhs.timestamp == rhs.timestamp;
}

/// Inequality operator for tp::JobSemaphore
constexpr bool operator!=(const JobSemaphore& lhs, const JobSemaphore& rhs) {
    return !(lhs == rhs);
}

/// A primitive for synchronizing against workloads other than jobs. It can be either a binary semaphore
/// in which case the timestamp is ignored, or a timeline semaphore that is treated as signalled when
/// the provided timestamp is reached.
/// @see @vksymbol{VkSemaphore}
struct ExternalSemaphore {
    VkSemaphoreHandle vkSemaphoreHandle;
    uint64_t timestamp;

    /// Constructs a null tp::ExternalSemaphore
    ExternalSemaphore() : vkSemaphoreHandle(), timestamp(0) {}

    /// Constructs a tp::ExternalSemaphore out of existing Vulkan semaphore and optionally a timestamp.
    /// @param vkSemaphoreHandle
    ///     The Vulkan @vksymbol{VkSemaphore} handle.
    /// @param timestamp
    ///     The timestamp to wait on for timeline semaphores. Ignored for binary semaphores.
    ExternalSemaphore(VkSemaphoreHandle vkSemaphoreHandle, uint64_t timestamp = 0)
        : vkSemaphoreHandle(vkSemaphoreHandle), timestamp(timestamp) {}

    /// Returns `true` if the semaphore is null and not valid for use.
    bool isNull() const {
        return vkSemaphoreHandle.isNull();
    }
};

/// Equality operator for tp::ExternalSemaphore
constexpr bool operator==(const ExternalSemaphore& lhs, const ExternalSemaphore& rhs) {
    return lhs.vkSemaphoreHandle == rhs.vkSemaphoreHandle && lhs.timestamp == rhs.timestamp;
}

/// Inequality operator for tp::ExternalSemaphore
constexpr bool operator!=(const ExternalSemaphore& lhs, const ExternalSemaphore& rhs) {
    return !(lhs == rhs);
}

/// Specifies a timeout duration for wait operations.
struct Timeout {
    uint64_t nanoseconds = 0;

    /// @param nanoseconds
    ///     Raw wait duration in nanoseconds.
    explicit constexpr Timeout(uint64_t nanoseconds) : nanoseconds(nanoseconds) {}

    /// Returns `true` if the tp::Timeout will wait indefinitely.
    bool isIndefinite() const {
        return nanoseconds == UINT64_MAX;
    }

    /// Returns a tp::Timeout using a duration in seconds.
    /// @param seconds
    ///     The wait duration in seconds.
    static Timeout Seconds(float seconds);

    /// Returns a tp::Timeout using a duration in milliseconds.
    /// @param milliseconds
    ///     The wait duration in milliseconds.
    static Timeout Milliseconds(float milliseconds);

    /// Returns a tp::Timeout value that will wait indefinitely.
    static constexpr Timeout Indefinite() {
        return Timeout(UINT64_MAX);
    }

    /// Returns a tp::Timeout value that will not wait at all.
    static constexpr Timeout NoWait() {
        return Timeout(0);
    }
};

}
