#pragma once

#include "../common_impl.hpp"
#include "logical_device.hpp"
#include "cross_queue_sync.hpp"
#include "memory_allocator.hpp"
#include <tuple>
#include <deque>

namespace tp {

class CommandPool;

// Handles the delayed destruction of Vulkan handles
class DeferredDestructor {
public:
    DeferredDestructor(LogicalDevice* logicalDevice, MemoryAllocator* memoryAllocator, CrossQueueSync* crossQueueSync)
        : logicalDevice(logicalDevice), memoryAllocator(memoryAllocator), crossQueueSync(crossQueueSync) {}

    // Destroys the given handle immediately
    template <typename T>
    void destroyImmediately(T handle);

    // Destroys all previously queued handles up to (and including) this timestamp
    void destroyUpToTimestamp(uint64_t timestamp) {
        // Forward the call to all of the typed queues in order
        std::apply([=](auto&&... args) { ((args.destroyUpToTimestamp(*this, timestamp)), ...); }, queues);
    }

    // Will destroy the given handle once timestamp is reached in all device queues
    // Can only be used for the queuable types
    template <typename T>
    void queueForDestruction(T handle, uint64_t timestamp) {
        std::get<DestructionQueue<T>>(queues).queueForDestruction(*this, handle, timestamp);
    }

    TEPHRA_MAKE_NONCOPYABLE(DeferredDestructor);
    TEPHRA_MAKE_NONMOVABLE(DeferredDestructor);
    ~DeferredDestructor() {
        // Ensure everything is destroyed
        destroyUpToTimestamp(~0);
    }

private:
    // Queue for delayed destruction of handles of a particular type
    template <typename T>
    class DestructionQueue {
    public:
        void queueForDestruction(DeferredDestructor& destructor, T handle, uint64_t timestamp);

        void destroyUpToTimestamp(DeferredDestructor& destructor, uint64_t timestamp);

    private:
        uint64_t lastDestroyedTimestamp = 0;
        Mutex mutex;
        // Destruction queue of (timestamp, handle) pairs in increasing order
        std::deque<std::pair<uint64_t, T>> queue;
    };

    // The storage of destruction queues for each typed handle that needs to be queued
    // Order is important here - handle types will get destroyed from top to bottom
    using DestructionQueuesTuple = std::tuple<
        DestructionQueue<VkPipelineHandle>,
        DestructionQueue<VkDescriptorPoolHandle>,
        DestructionQueue<VkBufferViewHandle>,
        DestructionQueue<VkBufferHandle>,
        DestructionQueue<VkImageViewHandle>,
        DestructionQueue<VkImageHandle>,
        DestructionQueue<VkSamplerHandle>,
        DestructionQueue<VkSwapchainHandleKHR>,
        DestructionQueue<VkSemaphoreHandle>,
        DestructionQueue<VmaAllocationHandle>>;

    LogicalDevice* logicalDevice;
    MemoryAllocator* memoryAllocator;
    CrossQueueSync* crossQueueSync;
    DestructionQueuesTuple queues;
};

// Composition of multiple lambdas into one type, distinguished by the parameter overloads
template <class... Ts>
struct CompositeLambda : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
CompositeLambda(Ts...) -> CompositeLambda<Ts...>;

template <typename T>
void DeferredDestructor::destroyImmediately(T handle) {
    LogicalDevice* ld = logicalDevice;
    CompositeLambda{
        [ld](VkShaderModuleHandle h) { ld->destroyShaderModule(h); },
        [ld](VkDescriptorSetLayoutHandle h) { ld->destroyDescriptorSetLayout(h); },
        [ld](VkDescriptorUpdateTemplateHandle h) { ld->destroyDescriptorUpdateTemplate(h); },
        [ld](VkPipelineLayoutHandle h) { ld->destroyPipelineLayout(h); },
        [ld](VkPipelineCacheHandle h) { ld->destroyPipelineCache(h); },
        [ld](VkPipelineHandle h) { ld->destroyPipeline(h); },
        [ld](VkDescriptorPoolHandle h) { ld->destroyDescriptorPool(h); },
        [ld](VkBufferViewHandle h) { ld->destroyBufferView(h); },
        [this](VkBufferHandle h) {
            this->crossQueueSync->broadcastResourceForget(h);
            this->logicalDevice->destroyBuffer(h);
        },
        [ld](VkImageViewHandle h) { ld->destroyImageView(h); },
        [this](VkImageHandle h) {
            this->crossQueueSync->broadcastResourceForget(h);
            this->logicalDevice->destroyImage(h);
        },
        [ld](VkSamplerHandle h) { ld->destroySampler(h); },
        [ld](VkSwapchainHandleKHR h) { ld->destroySwapchainKHR(h); },
        [ld](VkSemaphoreHandle h) { ld->destroySemaphore(h); },
        [this](VmaAllocationHandle h) { this->memoryAllocator->freeAllocation(h); },
    }(handle);
}

template <typename T>
void DeferredDestructor::DestructionQueue<T>::queueForDestruction(
    DeferredDestructor& destructor,
    T handle,
    uint64_t timestamp) {
    // Don't queue what we can destroy right now
    if (timestamp <= lastDestroyedTimestamp) {
        destructor.destroyImmediately(handle);
    } else {
        std::lock_guard<Mutex> mutexLock(mutex);
        // The timestamp is assumed to be recent
        queue.emplace_back(timestamp, handle);
    }
}

template <typename T>
void DeferredDestructor::DestructionQueue<T>::destroyUpToTimestamp(DeferredDestructor& destructor, uint64_t timestamp) {
    // Try returning early without acquiring a lock
    if (timestamp <= lastDestroyedTimestamp)
        return;

    std::lock_guard<Mutex> mutexLock(mutex);

    // Updating this timestamp early will help new destructions be more likely to get destroyed immediately
    // without having to wait for the lock to be released
    if (timestamp > lastDestroyedTimestamp)
        lastDestroyedTimestamp = timestamp;

    // Destroy handles up to the given timestamp
    while (!queue.empty() && queue.front().first <= timestamp) {
        destructor.destroyImmediately(queue.front().second);
        queue.pop_front();
    }
}

}
