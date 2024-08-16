#pragma once

#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

namespace tp {

struct QueryResult {
    uint64_t value = 0;
    JobSemaphore jobSemaphore = {};
};

// Represents a growing pool of Vulkan queries of the same type and properties
// Synchronized externally by QueryManager
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
    std::vector<Lifeguard<VkQueryPoolHandle>> vkQueryPools;
    std::vector<std::pair<uint32_t, uint32_t>> freeRanges;
};

// Represents data for a reusable Tephra Query
struct QueryEntry {
    // Used for beginVkQueryIndex to signify the query has not yet begun
    static constexpr uint32_t InvalidIndex = ~0u;

    QueryType type;
    std::variant<std::monostate, RenderQueryType> subType;
    QueryResult result;
    uint32_t poolIndex;
    // Index of the last query in the pool for scoped queries. Cleared after the scope ends.
    uint32_t beginVkQueryIndex;
    // Used to allow safe freeing of entries
    uint64_t lastPendingSampleTimestamp;

    // Translates query to Vulkan values
    std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> decodeVkQueryType() const;

    // Updates itself from Vulkan query data
    void updateResult(ArrayView<uint64_t> queryData, const JobSemaphore& semaphore);
};

// A non-owning query handle
using QueryHandle = QueryEntry*;

// Global manager for all queries
class QueryManager {
public:
    explicit QueryManager(DeviceContainer* deviceImpl, const VulkanCommandInterface* vkiCommands);

    void createTimestampQueries(ArrayParameter<TimestampQuery* const> queries);
    void createRenderQueries(
        ArrayParameter<const RenderQueryType> queryTypes,
        ArrayParameter<RenderQuery* const> queries);

    void beginSampleRenderQueries(
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const QueryHandle> queries,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

    void endSampleRenderQueries(VkCommandBufferHandle vkCommandBuffer, ArrayParameter<const QueryHandle> queries);

    void sampleTimestampQuery(
        VkCommandBufferHandle vkCommandBuffer,
        const QueryHandle& query,
        PipelineStage stage,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

    void queueFreeQuery(const QueryHandle& query);

    // Reads out all processed query samples and performs cleanup
    void update();

    double convertTimestampToSeconds(uint64_t timestampQueryResult) const;

private:
    // Represents submitted one-off Vulkan queries that will update a Tephra query entry
    struct QuerySample {
        static constexpr uint32_t MaxQueryCount = 8;

        QuerySample(
            QueryEntry* entry,
            uint32_t vkQueryIndex,
            uint32_t multiviewViewCount,
            const JobSemaphore& semaphore);

        QueryEntry* entry;
        uint32_t vkQueryIndex;
        uint32_t vkQueryCount;
        JobSemaphore semaphore;
    };

    // All the private methods assume globalMutex is locked
    QueryHandle createQuery(QueryType type, std::variant<std::monostate, RenderQueryType> subType);

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
    // For now we just use this global mutex to sync all query operations
    Mutex globalMutex;
    double ticksToSecondsFactor;
};

}
