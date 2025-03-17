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

enum class QueryType {
    Timestamp,
    Render,
};

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
    // How many results can be stored
    uint32_t maxResultsHistorySize;
    // The index of the most recent result
    uint32_t lastResultIndex;
    // When recording a scoped query, stores the pool of the begin query
    VkQueryPoolHandle beginScopeVkQueryPool;
    // When recording a scoped query, stores the index of the begin query within its pool
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
    static constexpr uint32_t MaxSampleCount = 32u;

    QueryBatch(QueryBatchPool* batchPool, Lifeguard<VkQueryPoolHandle> vkQueryPool);

    QueryBatchPool* getPool() {
        return batchPool;
    }

    VkQueryPoolHandle vkGetQueryPoolHandle() const {
        return vkQueryPool.vkGetHandle();
    }

    // Returns the remaining number of samples that this batch holds
    uint32_t getRemainingSampleCount() const {
        return MaxSampleCount - usedCount;
    }

    // Allocates a range of consecutive samples for a single query (more than one is needed for multiview)
    // Returns the index of the first one
    uint32_t allocateSamples(QueryEntry* entry, uint32_t count);

    // Reads back data from the queries in this batch (assuming it is ready and the given semaphore is signalled),
    // updates the entries and resets itself
    void readbackAndReset(DeviceContainer* deviceImpl, const JobSemaphore& semaphore);

private:
    // The parent pool the batch was allocated from
    QueryBatchPool* batchPool;
    Lifeguard<VkQueryPoolHandle> vkQueryPool;
    // The Tephra query entry to update that is associated with each Vulkan query sample
    // Multiple consecutive samples for the same entry will be considered as multiview samples
    std::array<QueryEntry*, MaxSampleCount> samples;
    uint32_t usedCount = 0;
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

    VkQueryPipelineStatisticFlagBits getPipelineStatisticsFlag() const {
        return pipelineStatistics;
    }

    QueryBatch* allocateBatch();

    void freeBatch(QueryBatch* queryBatch);

private:
    VkQueryType vkQueryType;
    VkQueryPipelineStatisticFlagBits pipelineStatistics;
    DeviceContainer* deviceImpl;
    ObjectPool<QueryBatch> pool;
};

class QueryManager;

// A non-owning query handle
using QueryHandle = QueryEntry*;

// A proxy used for recording queries to command buffers
class QueryRecorder {
public:
    explicit QueryRecorder(QueryManager* manager) : manager(manager) {}

    void beginSampleRenderQueries(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const RenderQuery* const> queries,
        uint32_t multiviewViewCount);

    void endSampleRenderQueries(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const RenderQuery* const> queries);

    void sampleTimestampQuery(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        const QueryHandle& query,
        PipelineStage stage,
        uint32_t multiviewViewCount);

    // Adds all used batches to the list and resets itself
    void retrieveBatchesToSubmit(std::vector<QueryBatch*>& batchList);

    static QueryHandle getQueryHandle(const BaseQuery& query) {
        return query.handle;
    }

private:
    // Returns or allocates a batch for the given query
    QueryBatch* getBatch(const QueryHandle& query, uint32_t sampleCount);

    QueryManager* manager;
    std::vector<QueryBatch*> usedBatches;
};

// Global manager for all queries
class QueryManager {
    friend class QueryRecorder;

public:
    explicit QueryManager(DeviceContainer* deviceImpl);

    void createTimestampQueries(ArrayParameter<TimestampQuery* const> queries);
    void createRenderQueries(
        ArrayParameter<const RenderQueryType> queryTypes,
        ArrayParameter<RenderQuery* const> queries);

    void queueFreeQuery(const QueryHandle& query);
    // Recycles batches that weren't submitted
    void freeDiscardedBatches(ArrayParameter<QueryBatch*> batches);
    // Submits batches
    void submitBatches(ArrayParameter<QueryBatch*> batches, const JobSemaphore& semaphore);

    // Reads out all processed query samples and performs cleanup
    void update();

private:
    using PendingBatchQueue = std::deque<std::pair<JobSemaphore, QueryBatch*>>;

    // Allocates a batch for the given query
    QueryBatch* allocateBatchForQuery(const QueryHandle& query);
    // Creates a new Tephra query of the given type, assuming globalMutex is locked
    QueryHandle createQuery(QueryType type, std::variant<std::monostate, RenderQueryType> subType);
    // Translates query to the index of the pool
    uint32_t queryToPoolIndex(const QueryHandle& query);

    DeviceContainer* deviceImpl;
    std::vector<QueryBatchPool> queryPools;
    // Pending batch queue for each device queue
    std::vector<PendingBatchQueue> pendingBatchQueues;
    ObjectPool<QueryEntry> entryPool;
    std::vector<QueryEntry*> entriesToFree;
    // For now we just use this global mutex to sync all query operations
    Mutex globalMutex;
};

}
