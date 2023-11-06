#pragma once

#include "../vulkan/interface.hpp"
#include "../common_impl.hpp"
#include <tephra/physical_device.hpp>
#include <deque>

namespace tp {

class CommandPoolPool;

class CommandPool {
public:
    CommandPool(VkCommandPoolHandle vkCommandPoolHandle, CommandPoolPool* commandPoolPool, QueueType queueType);

    // Resets the command buffers allocated from this pool, allowing them to be reused
    void reset();

    // Returns a free primary command buffer handle, allocating new one if necessary
    VkCommandBufferHandle acquirePrimaryCommandBuffer(const char* debugName);

    // Returns a free secondary command buffer handle, allocating new ones if necessary
    VkCommandBufferHandle acquireSecondaryCommandBuffer(const char* debugName);

    QueueType getQueueType() const {
        return queueType;
    }

    VkCommandPoolHandle vkGetCommandPoolHandle() const {
        return vkCommandPoolHandle;
    }

    ~CommandPool();

private:
    VkCommandPoolHandle vkCommandPoolHandle;
    CommandPoolPool* commandPoolPool;
    QueueType queueType;
    std::vector<VkCommandBufferHandle> primaryBuffers;
    uint32_t usedPrimaryBuffers;
    std::vector<VkCommandBufferHandle> secondaryBuffers;
    uint32_t usedSecondaryBuffers;
};

// A pool of command pools, yes
class CommandPoolPool {
public:
    explicit CommandPoolPool(DeviceContainer* deviceImpl);

    const DeviceContainer* getParentDeviceImpl() const {
        return deviceImpl;
    }

    CommandPool* acquirePool(QueueType queueType, const char* debugName);

    void releasePool(CommandPool* cmdPool);

    const VulkanCommandInterface& getVkiCommands() const {
        return vkiCommands;
    }

    TEPHRA_MAKE_NONCOPYABLE(CommandPoolPool);
    TEPHRA_MAKE_NONMOVABLE(CommandPoolPool);
    ~CommandPoolPool() = default;

private:
    friend class CommandPool;

    DeviceContainer* deviceImpl;
    VulkanCommandInterface vkiCommands;
    Mutex mutex;

    // A free list of command pools per queue type
    std::vector<CommandPool*> freeLists[QueueTypeEnumView::size()];
    std::deque<CommandPool> storage;

    void resetCommandPool(VkCommandPoolHandle vkCommandPoolHandle, bool releaseResources);

    void freeCommandPool(VkCommandPoolHandle vkCommandPoolHandle);

    void allocateCommandBuffers(
        VkCommandPoolHandle vkCommandPoolHandle,
        VkCommandBufferLevel level,
        ArrayView<VkCommandBufferHandle> buffers);
};

}
