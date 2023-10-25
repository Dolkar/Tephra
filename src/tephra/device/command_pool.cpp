#include "command_pool.hpp"
#include "device_container.hpp"

namespace tp {

CommandPool::CommandPool(VkCommandPoolHandle vkCommandPoolHandle, CommandPoolPool* commandPoolPool, QueueType queueType)
    : vkCommandPoolHandle(vkCommandPoolHandle),
      commandPoolPool(commandPoolPool),
      queueType(queueType),
      usedCommandBuffers(0) {}

void CommandPool::reset() {
    commandPoolPool->resetCommandPool(vkCommandPoolHandle, false);
    usedCommandBuffers = 0;
}

VkCommandBufferHandle CommandPool::acquirePrimaryCommandBuffer(const char* debugName) {
    if (usedCommandBuffers == commandBuffers.size()) {
        VkCommandBufferHandle vkNewCommandBufferHandle;
        // Tephra only uses primary command buffers after dynamic rendering
        commandPoolPool->allocateCommandBuffers(
            vkCommandPoolHandle, VK_COMMAND_BUFFER_LEVEL_PRIMARY, viewOne(vkNewCommandBufferHandle));

        commandBuffers.push_back(vkNewCommandBufferHandle);
    }

    VkCommandBufferHandle vkCommandBufferHandle = commandBuffers[usedCommandBuffers++];
    commandPoolPool->getParentDeviceImpl()->getLogicalDevice()->setObjectDebugName(vkCommandBufferHandle, debugName);

    return vkCommandBufferHandle;
}

CommandPool::~CommandPool() {
    commandPoolPool->freeCommandPool(vkCommandPoolHandle);
}

CommandPoolPool::CommandPoolPool(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {
    vkiCommands = deviceImpl->getLogicalDevice()->loadDeviceInterface<VulkanCommandInterface>();
}

CommandPool* CommandPoolPool::acquirePool(QueueType queueType, const char* debugName) {
    std::lock_guard<Mutex> mutexLock(mutex);

    auto& freeList = freeLists[static_cast<int>(queueType)];
    if (!freeList.empty()) {
        CommandPool* pool = freeList.back();
        freeList.pop_back();
        pool->reset();
        return pool;
    }

    // All command buffers are relatively short lived in intended Tephra usage
    int flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPoolHandle vkCommandPoolHandle = deviceImpl->getLogicalDevice()->createCommandPool(
        queueType, static_cast<VkCommandPoolCreateFlagBits>(flags));

    deviceImpl->getLogicalDevice()->setObjectDebugName(vkCommandPoolHandle, debugName);

    storage.emplace_back(vkCommandPoolHandle, this, queueType);
    return &storage.back();
}

void CommandPoolPool::releasePool(CommandPool* cmdPool) {
    std::lock_guard<Mutex> mutexLock(mutex);

    auto& freeList = freeLists[static_cast<int>(cmdPool->getQueueType())];
    freeList.push_back(cmdPool);
}

void CommandPoolPool::resetCommandPool(VkCommandPoolHandle vkCommandPoolHandle, bool releaseResources) {
    VkCommandPoolResetFlagBits clearFlags = releaseResources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT :
                                                               static_cast<VkCommandPoolResetFlagBits>(0);
    throwRetcodeErrors(vkiCommands.resetCommandPool(
        deviceImpl->getLogicalDevice()->vkGetDeviceHandle(), vkCommandPoolHandle, clearFlags));
}

void CommandPoolPool::freeCommandPool(VkCommandPoolHandle vkCommandPoolHandle) {
    deviceImpl->getLogicalDevice()->destroyCommandPool(vkCommandPoolHandle);
}

void CommandPoolPool::allocateCommandBuffers(
    VkCommandPoolHandle vkCommandPoolHandle,
    VkCommandBufferLevel level,
    ArrayView<VkCommandBufferHandle> buffers) {
    VkCommandBufferAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = vkCommandPoolHandle;
    allocInfo.level = level;
    allocInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());

    throwRetcodeErrors(vkiCommands.allocateCommandBuffers(
        deviceImpl->getLogicalDevice()->vkGetDeviceHandle(), &allocInfo, vkCastTypedHandlePtr(buffers.data())));
}

}
