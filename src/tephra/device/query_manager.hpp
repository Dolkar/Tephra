#pragma once

#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

namespace tp {

struct QueryResult {
    uint64_t value = 0;
    tp::JobSemaphore jobSemaphore = {};
};

// Represents a growing pool of Vulkan queries of the same type and properties
class QueryPool {
public:
    static constexpr uint32_t QueriesInPool = 64;

    QueryPool(DeviceContainer* deviceImpl, VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics)
        : deviceImpl(deviceImpl), vkQueryType(vkQueryType), pipelineStatistics(pipelineStatistics) {}

    VkQueryType getVkQueryType() const {
        return vkQueryType;
    }

    VkQueryPipelineStatisticFlagBits getPipelineStatisticsFlags() const {
        return pipelineStatistics;
    }

    std::pair<VkQueryPoolHandle, uint32_t> lookupQuery(uint32_t index) const;

    // Allocates a range of consecutive queries (needed for multiview)
    uint32_t allocateVkQueries(uint32_t count);

    void readbackAndFreeVkQueries(uint32_t firstIndex, uint32_t count, ArrayView<uint64_t> data);

private:
    VkQueryType vkQueryType;
    VkQueryPipelineStatisticFlagBits pipelineStatistics;
    DeviceContainer* deviceImpl;
    std::vector<tp::Lifeguard<VkQueryPoolHandle>> vkQueryPools;
    std::vector<std::pair<uint32_t, uint32_t>> freeRanges;
};

class QueryManager {
public:
    explicit QueryManager(DeviceContainer* deviceImpl, const VulkanCommandInterface* vkiCommands);

    void createTimestampQueries(ArrayParameter<TimestampQuery* const> queries);
    void createScopedQueries(
        ArrayParameter<const ScopedQueryType> queryTypes,
        ArrayParameter<ScopedQuery* const> queries);

    void beginSampleScopedQueries(
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const ScopedQuery> queries,
        uint32_t multiviewViewCount,
        const tp::JobSemaphore& semaphore);

    void endSampleScopedQueries(VkCommandBufferHandle vkCommandBuffer, ArrayParameter<const ScopedQuery> queries);

    void writeTimestampQuery(
        VkCommandBufferHandle vkCommandBuffer,
        const TimestampQuery& query,
        PipelineStage stage,
        uint32_t multiviewViewCount,
        const tp::JobSemaphore& semaphore);

    void update();

    const QueryResult& getQueryResult(BaseQuery::Handle handle) const;
    double convertTimestampToSeconds(uint64_t timestampQueryResult) const;

    void queueFreeQuery(BaseQuery::Handle handle);

private:
    static constexpr uint32_t InvalidIndex = ~0u;

    struct QueryEntry {
        QueryType type;
        std::variant<std::monostate, ScopedQueryType> subType;
        QueryResult result;
        uint32_t poolIndex;
        // Index of the last query in the pool for scoped queries. Cleared after the scope ends.
        uint32_t beginVkQueryIndex;
        // Used to allow safe freeing of entries
        uint64_t lastPendingSampleTimestamp;

        std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> decodeVkQueryType() const;

        void updateResult(ArrayView<uint64_t> queryData, const tp::JobSemaphore& semaphore);
    };

    struct QuerySample {
        static constexpr uint32_t MaxQueryCount = 8;

        QuerySample(
            QueryEntry* entry,
            uint32_t vkQueryIndex,
            uint32_t multiviewViewCount,
            const tp::JobSemaphore& semaphore);

        QueryEntry* entry;
        uint32_t vkQueryIndex;
        uint32_t vkQueryCount;
        tp::JobSemaphore semaphore;
    };

    BaseQuery::Handle createQuery(QueryType type, std::variant<std::monostate, ScopedQueryType> subType);

    uint32_t getOrCreatePool(VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics);

    void readoutSamples(const ScratchVector<QuerySample>& samples);

    void cmdBeginQuery(VkCommandBufferHandle vkCommandBuffer, uint32_t poolIndex, uint32_t vkQueryIndex, bool isPrecise);

    void cmdEndQuery(VkCommandBufferHandle vkCommandBuffer, uint32_t poolIndex, uint32_t vkQueryIndex);

    void cmdWriteTimestamp(
        VkCommandBufferHandle vkCommandBuffer,
        uint32_t poolIndex,
        uint32_t vkQueryIndex,
        PipelineStage stage);

    DeviceContainer* deviceImpl;
    const VulkanCommandInterface* vkiCommands;
    std::vector<QueryPool> queryPools;
    ObjectPool<QueryEntry> entryPool;
    std::vector<QueryEntry*> entriesToFree;
    std::vector<QuerySample> pendingSamples;
    double ticksToSecondsFactor;
};

}
