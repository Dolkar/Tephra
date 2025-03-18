#pragma once

#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

#include <array>
#include <atomic>

namespace tp {

class QueryBatchPool;

enum class QueryType {
    Timestamp,
    Render,
};

// Represents data for a reusable Tephra Query
struct QueryRecord {
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
    // Used to allow safe freeing of records
    uint64_t lastPendingSampleTimestamp;

    // Translates query to Vulkan values
    std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> decodeVkQueryType() const;

    // Updates itself from Vulkan query sample data
    void updateResults(ArrayView<uint64_t> sampleData, const JobSemaphore& semaphore);
};

// Holds a small batch of query samples for updating query records of the same type at once
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
    uint32_t allocateSamples(QueryRecord* record, uint32_t count);

    // Reads back data from the queries in this batch (assuming it is ready and the given semaphore is signalled),
    // updating the records
    void readback(DeviceContainer* deviceImpl, const JobSemaphore& semaphore);

    // Reset without reading anything back
    void reset() {
        usedCount = 0;
    }

private:
    // The parent pool the batch was allocated from
    QueryBatchPool* batchPool;
    Lifeguard<VkQueryPoolHandle> vkQueryPool;
    // The query record to update that is associated with each Vulkan query sample
    // Multiple consecutive samples for the same record will be considered as multiview samples
    std::array<QueryRecord*, MaxSampleCount> samples;
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
using QueryHandle = QueryRecord*;

// A proxy used for recording queries to command buffers
class QueryRecorder {
public:
    explicit QueryRecorder(QueryManager* manager) : manager(manager) {}

    void beginSampleRenderQueries(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const RenderQuery* const> queries,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

    void endSampleRenderQueries(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        ArrayParameter<const RenderQuery* const> queries);

    void sampleTimestampQuery(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkCommandBuffer,
        const QueryHandle& query,
        PipelineStage stage,
        uint32_t multiviewViewCount,
        const JobSemaphore& semaphore);

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

    // Submits batches for readback
    void submitBatches(ArrayParameter<QueryBatch*> batches, const JobSemaphore& semaphore);

    // Reads out all processed query samples and performs cleanup
    void update();

private:
    using PendingBatchQueue = std::deque<std::pair<JobSemaphore, QueryBatch*>>;

    // Locks and allocates a batch for the given query
    QueryBatch* allocateBatchForQuery(const QueryHandle& query);
    // Creates a new query record of the given type, assuming globalMutex is locked
    QueryHandle createQueryRecord(QueryType type, std::variant<std::monostate, RenderQueryType> subType);
    // Returns the batch pool for the given Vulkan query type, assuming globalMutex is locked
    QueryBatchPool& getOrCreatePool(VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics);

    DeviceContainer* deviceImpl;
    // Pools of query batches for each type, created on demand
    std::deque<QueryBatchPool> queryPools;
    // Pending batch queue for each device queue
    std::vector<PendingBatchQueue> pendingBatchQueues;
    // Pool for reusing query records (interface-facing query objects)
    ObjectPool<QueryRecord> recordPool;
    // List of query records to free once unused
    std::vector<QueryRecord*> recordsToFree;
    // Mutex synchronizing access to queryPools, recordPool and recordsToFree
    Mutex recordMutex;
    // Mutex synchronizing access to pendingBatchQueues and individual QueryBatchPools
    // Could potentially be an independent mutex for each queue
    Mutex batchMutex;
};

}
