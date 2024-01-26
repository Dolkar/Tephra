#pragma once

#include "local_acceleration_structures.hpp"
#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"
#include <unordered_map>

namespace tp {

class JobLocalAccelerationStructureAllocator {
public:
    explicit JobLocalAccelerationStructureAllocator(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {}

    AccelerationStructureBuilder* acquireBuilder(const AccelerationStructureSetup& setup, uint64_t jobId);

    void releaseBuilders(uint64_t jobId);

    // Assigns or creates Vulkan acceleration structure objects based on the allocated buffers
    void acquireJobResources(JobLocalAccelerationStructures* resources, uint64_t currentTimestamp);

    // Frees all resources that were last used up to the given timestamp
    void trim(uint64_t upToTimestamp);

private:
    struct AccelerationStructureKey {
        AccelerationStructureType type;
        VkBufferHandle vkBuffer;
        uint64_t offset;
        uint64_t size;

        AccelerationStructureKey(AccelerationStructureType type, const BufferView& backingBuffer);

        bool operator==(const AccelerationStructureKey& other) const {
            return type == other.type && vkBuffer == other.vkBuffer && offset == other.offset && size == other.size;
        }
    };

    struct AccelerationStructureKeyHash {
        std::size_t operator()(const AccelerationStructureKey& key) const {
            const uint64_t fibMul = 11400714819323198485ull; // 2^64 / phi
            uint64_t hash = static_cast<uint64_t>(key.type);
            hash = hash * fibMul ^ reinterpret_cast<uint64_t>(key.vkBuffer.vkRawHandle);
            hash = hash * fibMul ^ static_cast<uint64_t>(key.offset);
            hash = hash * fibMul ^ static_cast<uint64_t>(key.size);
            return hash;
        }
    };

    struct AccelerationStructureEntry {
        Lifeguard<VkAccelerationStructureHandleKHR> handle;
        uint64_t lastUsedTimestamp;

        AccelerationStructureEntry() : handle(), lastUsedTimestamp(0) {}
        AccelerationStructureEntry(Lifeguard<VkAccelerationStructureHandleKHR> handle, uint64_t timestamp)
            : handle(std::move(handle)), lastUsedTimestamp(timestamp) {}
    };

    DeviceContainer* deviceImpl;

    std::vector<std::pair<uint64_t, AccelerationStructureBuilder*>> acquiredBuilders;
    ObjectPool<AccelerationStructureBuilder> builderPool;
    std::unordered_map<AccelerationStructureKey, AccelerationStructureEntry, AccelerationStructureKeyHash> handleMap;
};

}
