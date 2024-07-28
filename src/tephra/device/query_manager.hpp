#pragma once

#include "logical_device.hpp"
#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

namespace tp {

// Represents a growing pool of Vulkan queries of the same type and properties
class QueryPool {
public:
    static constexpr uint32_t QueriesInPool = 64;

    QueryPool(LogicalDevice* device, VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics)
        : device(device), vkQueryType(vkQueryType), pipelineStatistics(pipelineStatistics) {}

    VkQueryType getVkQueryType() const {
        return vkQueryType;
    }

    VkQueryPipelineStatisticFlagBits getPipelineStatisticsFlags() const {
        return pipelineStatistics;
    }

    // Allocates a range of consecutive queries (needed for multiview)
    uint32_t allocateVkQueries(uint32_t count);

    void readbackAndFreeVkQueries(uint32_t firstIndex, uint32_t count, ArrayView<uint64_t> data);

private:
    VkQueryType vkQueryType;
    VkQueryPipelineStatisticFlagBits pipelineStatistics;
    LogicalDevice* device;
    std::vector<VkQueryPoolHandle> vkQueryPools;
    std::vector<std::pair<uint32_t, uint32_t>> freeRanges;
};

class QueryManager {
public:
    explicit QueryManager(LogicalDevice* device) : device(device) {}

    void createQueries(ArrayParameter<const QueryType> queryTypes, ArrayParameter<Query* const> queries);

    QueryType getQueryType(Query::Handle handle) const;
    QueryResult getQueryResult(Query::Handle handle) const;

    void queueFreeQuery(Query::Handle handle);

private:
    struct QueryEntry {
        QueryType type;
        QueryResult result;
        // Cached value to avoid pool lookup
        uint32_t lastPoolIndex;
    };

    struct QuerySample {
        uint64_t semaphoreTimestamp;
        uint32_t poolIndex;
        uint32_t vkQueryIndex;
        uint32_t vkQueryCount;
        QueryEntry* entry;
    };

    uint32_t getOrCreatePool(VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics);

    LogicalDevice* device;
    std::vector<QueryPool> queryPools;
    ObjectPool<QueryEntry> entryPool;
    std::vector<QueryEntry*> entriesToFree;
    std::vector<QuerySample> pendingSamples;
};

}
