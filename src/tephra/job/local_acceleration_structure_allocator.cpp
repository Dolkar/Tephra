#include "local_acceleration_structure_allocator.hpp"
#include "../device/device_container.hpp"
#include <algorithm>

namespace tp {

AccelerationStructureBuilder* JobLocalAccelerationStructureAllocator::acquireBuilder(
    const AccelerationStructureSetup& setup,
    uint64_t jobId) {
    AccelerationStructureBuilder* builder = builderPool.acquireExisting();
    if (builder == nullptr)
        builder = builderPool.acquireNew();
    builder->reset(deviceImpl, setup);

    acquiredBuilders.push_back({ jobId, builder });
    return builder;
}

void JobLocalAccelerationStructureAllocator::releaseBuilders(uint64_t jobId) {
    auto it = std::remove_if(acquiredBuilders.begin(), acquiredBuilders.end(), [this, jobId](auto entry) {
        auto [builderJobId, builder] = entry;
        if (jobId == builderJobId) {
            builderPool.release(builder);
        }
        return jobId == builderJobId;
    });
    acquiredBuilders.erase(it, acquiredBuilders.end());
}

void JobLocalAccelerationStructureAllocator::acquireJobResources(
    JobLocalAccelerationStructures* resources,
    uint64_t currentTimestamp) {
    for (auto& accelerationStructure : resources->accelerationStructures) {
        // At this point we can assume that all backing buffers have already been allocated
        auto key = AccelerationStructureKey(
            accelerationStructure.getBuilder()->getType(), accelerationStructure.getBackingBufferView());

        VkAccelerationStructureHandleKHR vkHandle;
        auto it = handleMap.find(key);
        if (it != handleMap.end()) {
            // Reuse existing handle and update timestamp
            vkHandle = it->second.handle.vkGetHandle();
            TEPHRA_ASSERT(currentTimestamp >= it->second.lastUsedTimestamp);
            it->second.lastUsedTimestamp = currentTimestamp;
        } else {
            // Allocate a new handle and add it to the map
            auto handleLifeguard = deviceImpl->vkMakeHandleLifeguard(
                deviceImpl->getLogicalDevice()->createAccelerationStructureKHR(
                    accelerationStructure.getBuilder()->getType(), accelerationStructure.getBackingBufferView()));
            vkHandle = handleLifeguard.vkGetHandle();

            handleMap[key] = AccelerationStructureEntry(std::move(handleLifeguard), currentTimestamp);
        }

        accelerationStructure.assignHandle(vkHandle);
    }
}

void JobLocalAccelerationStructureAllocator::trim(uint64_t upToTimestamp) {
    for (auto it = handleMap.begin(); it != handleMap.end();) {
        if (it->second.lastUsedTimestamp <= upToTimestamp) {
            it = handleMap.erase(it);
        } else {
            ++it;
        }
    }
}

JobLocalAccelerationStructureAllocator::AccelerationStructureKey::AccelerationStructureKey(
    AccelerationStructureType type,
    const BufferView& backingBuffer)
    : type(type) {
    vkBuffer = backingBuffer.vkResolveBufferHandle(&offset);
    size = backingBuffer.getSize();
}

}
