#pragma once

#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

#include <array>
#include <atomic>

namespace tp {

// queries 2.0:
//
// Query batch - Fixed size buffer of queries of some size, submitted at the same time
// Command buffers (and jobs) grab a batch when needed - this amortizes the allocation cost by the size of the batch
// Lookup of free batches can then be simplified (no need to track query ranges), literally just an object pool
// Batches are submitted for readback and reset as a whole as part of job submission

class QueryBatchPool;

// Represents data for a reusable Tephra Query
struct QueryEntry {
    // Store at least two so that we can ping-pong between the results
    static constexpr uint32_t MinMaxResultsHistorySize = 2u;

    QueryType type;
    std::variant<std::monostate, RenderQueryType> subType;
    // The parent pool to allocate query batches from
    QueryBatchPool* batchPool;
    // Unsorted list of results
    std::vector<QueryResult> resultsHistory;
    uint32_t maxResultsHistorySize;
    // The index of the most recent result
    uint32_t lastResultIndex;
    // When recording a scoped query, stores the pool of the begin query
    VkQueryPoolHandle beginScopeVkQueryPool;
    // When recording a scoped query, stores the inted of the begin query within its pool
    uint32_t beginScopeQueryIndex;
    // Used to allow safe freeing of entries
    uint64_t lastPendingSampleTimestamp;

    // Translates query to Vulkan values
    std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> decodeVkQueryType() const;

    // Updates itself from Vulkan query sample data
    void updateResults(ArrayView<uint64_t> sampleData, const JobSemaphore& semaphore);
};

// Holds a small batch of query samples for updating query entries of the same type at once
class QueryBatch {
public:
    static constexpr uint32_t MaxSampleCount = 64u;
    static constexpr uint32_t InvalidIndex = ~0u;

    QueryBatch(QueryBatchPool* batchPool, Lifeguard<VkQueryPoolHandle> vkQueryPool);

    // Begins collecting samples for the given job
    void assignToJob(const JobSemaphore& semaphore);

    // Allocates a range of consecutive samples for a single query (more than one is needed for multiview)
    // Returns the index of the first one
    uint32_t allocateSamples(QueryEntry* entry, uint32_t count);

    // Reads back data from the queries in this batch (assuming it is ready), updates the entries and resets itself
    void readbackAndReset(DeviceContainer* deviceImpl);

private:
    // The parent pool the batch was allocated from
    QueryBatchPool* batchPool;
    Lifeguard<VkQueryPoolHandle> vkQueryPool;
    JobSemaphore semaphore;
    // The Tephra query entry to update that is associated with each Vulkan query sample
    // Multiple consecutive samples for the same entry will be considered as multiview samples
    std::array<QueryEntry*, MaxSampleCount> samples;
    uint32_t usedCount = 0;
};

enum class QueryType {
    Timestamp,
    Render,
};

// Simple pool for query batches of the same type. Must be externally synchronized
class QueryBatchPool {
public:
    QueryBatchPool(
        DeviceContainer* deviceImpl,
        VkQueryType vkQueryType,
        VkQueryPipelineStatisticFlagBits pipelineStatistics)
        : deviceImpl(deviceImpl), vkQueryType(vkQueryType), pipelineStatistics(pipelineStatistics) {}

    VkQueryType getVkQueryType() const {
        return vkQueryType;
    }

    VkQueryPipelineStatisticFlagBits getPipelineStatisticsFlags() const {
        return pipelineStatistics;
    }

    QueryBatch* allocateBatch(const JobSemaphore& semaphore);

    void freeBatch(QueryBatch* queryBatch);

private:
    // Its own index, passed to batches so that they are self-sufficient
    VkQueryType vkQueryType;
    VkQueryPipelineStatisticFlagBits pipelineStatistics;
    DeviceContainer* deviceImpl;
    ObjectPool<QueryBatch> pool;
};

// A proxy used for recording queries to command buffers
class QueryRecorder {};

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
        ArrayParameter<const RenderQuery* const> queries,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

    void endSampleRenderQueries(VkCommandBufferHandle vkCommandBuffer, ArrayParameter<const RenderQuery* const> queries);

    void sampleTimestampQuery(
        VkCommandBufferHandle vkCommandBuffer,
        const QueryHandle& query,
        PipelineStage stage,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

    void queueFreeQuery(const QueryHandle& query);

    // Reads out all processed query samples and performs cleanup
    void update();

    static QueryHandle getQueryHandle(const BaseQuery& query) {
        return query.handle;
    }

private:
    // All the private methods assume globalMutex is locked
    QueryHandle createQuery(QueryType type, std::variant<std::monostate, RenderQueryType> subType);

    uint32_t getOrCreatePool(VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics);

    void cmdBeginQuery(VkCommandBufferHandle vkCommandBuffer, uint32_t poolIndex, uint32_t vkQueryIndex, bool isPrecise);

    void cmdEndQuery(VkCommandBufferHandle vkCommandBuffer, uint32_t poolIndex, uint32_t vkQueryIndex);

    void cmdWriteTimestamp(
        VkCommandBufferHandle vkCommandBuffer,
        uint32_t poolIndex,
        uint32_t vkQueryIndex,
        PipelineStage stage);

    DeviceContainer* deviceImpl;
    const VulkanCommandInterface* vkiCommands;
    std::deque<QueryBatchPool> queryPools;
    ObjectPool<QueryEntry> entryPool;
    std::vector<QueryEntry*> entriesToFree;
    std::vector<QueryBatchPool> pendingBatches;
    // For now we just use this global mutex to sync all query operations
    Mutex globalMutex;
};

}
