#include "query_manager.hpp"
#include "device_container.hpp"

#include <algorithm>

namespace tp {

BaseQuery::BaseQuery(BaseQuery&& other) noexcept : parentManager(other.parentManager), handle(other.handle) {
    if (!other.isNull()) {
        other.parentManager = nullptr;
    }
}

BaseQuery& BaseQuery::operator=(BaseQuery&& other) noexcept {
    std::swap(parentManager, other.parentManager);
    std::swap(handle, other.handle);
    return *this;
}

QueryResult BaseQuery::getLastResult() const {
    TEPHRA_ASSERT(!isNull());
    if (handle->resultsHistory.empty())
        return {};
    else
        return handle->resultsHistory[handle->lastResultIndex];
}

QueryResult BaseQuery::getJobResult(const JobSemaphore& jobSemaphore) const {
    TEPHRA_ASSERT(!isNull());
    for (const QueryResult& result : handle->resultsHistory) {
        if (jobSemaphore == result.jobSemaphore)
            return result;
    }
    return {};
}

void BaseQuery::setMaxHistorySize(uint32_t size) {
    TEPHRA_ASSERT(!isNull());
    handle->maxResultsHistorySize = tp::max(size, QueryRecord::MinMaxResultsHistorySize);
}

BaseQuery::BaseQuery(QueryManager* parentManager, Handle handle) : parentManager(parentManager), handle(handle) {
    TEPHRA_ASSERT(parentManager != nullptr && handle != nullptr);
}

BaseQuery::~BaseQuery() noexcept {
    if (!isNull()) {
        parentManager->queueFreeQuery(handle);
    }
}

std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> QueryRecord::decodeVkQueryType() const {
    switch (type) {
    case QueryType::Timestamp:
        return { VK_QUERY_TYPE_TIMESTAMP, VkQueryPipelineStatisticFlagBits() };
    case QueryType::Render: {
        RenderQueryType renderQueryType = std::get<RenderQueryType>(subType);
        switch (renderQueryType) {
        case RenderQueryType::Occlusion:
        case RenderQueryType::OcclusionPrecise:
            return { VK_QUERY_TYPE_OCCLUSION, VkQueryPipelineStatisticFlagBits() };
        case RenderQueryType::InputAssemblyVertices:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT };
        case RenderQueryType::InputAssemblyPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT };
        case RenderQueryType::VertexShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT };
        case RenderQueryType::GeometryShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT };
        case RenderQueryType::GeometryShaderPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT };
        case RenderQueryType::ClippingInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT };
        case RenderQueryType::ClippingPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT };
        case RenderQueryType::FragmentShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT };
        case RenderQueryType::TessellationControlShaderPatches:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                     VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT };
        case RenderQueryType::TessellationEvaluationShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                     VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT };
        default:
            TEPHRA_ASSERTD(false, "Unexpected RenderQueryType");
            return {};
        }
    }
    default:
        TEPHRA_ASSERTD(false, "Unexpected QueryType");
        return {};
    }
}

void QueryRecord::updateResults(ArrayView<uint64_t> sampleData, const JobSemaphore& semaphore) {
    TEPHRA_ASSERT(sampleData.size() > 0);
    TEPHRA_ASSERT(!semaphore.isNull());

    // Find the oldest result to overwrite
    QueryResult* resultToUpdate = resultsHistory.data();
    if (resultsHistory.size() >= maxResultsHistorySize) {
        for (std::size_t i = 1; i < resultsHistory.size(); i++) {
            QueryResult* result = &resultsHistory[i];
            // Null job semaphores have timestamp 0, so they'll be treated as oldest
            if (result->jobSemaphore.timestamp <= resultToUpdate->jobSemaphore.timestamp)
                resultToUpdate = result;
        }
    } else {
        resultToUpdate = &resultsHistory.emplace_back();
    }

    if (semaphore.timestamp < resultToUpdate->jobSemaphore.timestamp)
        return;
    TEPHRA_ASSERT(semaphore.timestamp != resultToUpdate->jobSemaphore.timestamp);

    // One query can have multiple samples if used during multiview. Here we decide how to combine them.
    // It's possible to expose all of them to the user, but that is rarely needed, is implementation
    // dependent and would complicate the API.
    uint64_t newResultValue = 0;
    switch (type) {
    case QueryType::Timestamp:
        // Just use the first view for timestamps
        newResultValue = sampleData[0];
        break;
    case QueryType::Render:
        // Render query types should be summed together
        for (uint64_t value : sampleData)
            newResultValue += value;
        break;
    default:
        TEPHRA_ASSERTD(false, "Unexpected QueryType");
    }

    // Overwrite oldest result
    *resultToUpdate = { semaphore, newResultValue };
    QueryResult* lastResult = &resultsHistory[lastResultIndex];

    // If this is the newest result, also update the index
    if (semaphore.timestamp > lastResult->jobSemaphore.timestamp) {
        lastResultIndex = static_cast<uint32_t>(std::distance(resultsHistory.data(), resultToUpdate));
    }
}

QueryBatch::QueryBatch(QueryBatchPool* batchPool, Lifeguard<VkQueryPoolHandle> vkQueryPool)
    : batchPool(batchPool), vkQueryPool(std::move(vkQueryPool)) {}

uint32_t QueryBatch::allocateSamples(QueryRecord* record, uint32_t count) {
    TEPHRA_ASSERT(record != nullptr);
    TEPHRA_ASSERT(record->batchPool == batchPool);
    TEPHRA_ASSERT(count <= getRemainingSampleCount());

    uint32_t firstIndex = usedCount;
    usedCount += count;

    for (uint32_t i = 0; i < count; i++) {
        samples[firstIndex + i] = record;
    }

    return firstIndex;
}

void QueryBatch::readback(DeviceContainer* deviceImpl, const JobSemaphore& semaphore) {
    TEPHRA_ASSERT(usedCount <= MaxSampleCount);
    TEPHRA_ASSERT(!semaphore.isNull());

    if (usedCount == 0)
        return;

    uint64_t sampleData[MaxSampleCount];
    deviceImpl->getLogicalDevice()->getQueryResultsAndReset(
        vkQueryPool.vkGetHandle(), 0, usedCount, tp::viewRange(sampleData, 0, usedCount));

    // Consecutive samples with the same record will be updated together
    uint32_t si = 0;
    for (uint32_t i = 1; i < usedCount; i++) {
        if (samples[si] != samples[i]) {
            samples[si]->updateResults(tp::viewRange(sampleData, si, i - si), semaphore);
            si = i;
        }
    }
    if (si < usedCount)
        samples[si]->updateResults(tp::viewRange(sampleData, si, usedCount - si), semaphore);
}

QueryBatch* QueryBatchPool::allocateBatch() {
    QueryBatch* batch = pool.acquireExisting();

    if (batch == nullptr) {
        VkQueryPoolHandle vkQueryPool = deviceImpl->getLogicalDevice()->createQueryPool(
            vkQueryType, pipelineStatistics, QueryBatch::MaxSampleCount);
        batch = pool.acquireNew(this, deviceImpl->vkMakeHandleLifeguard(vkQueryPool));
    }

    return batch;
}

void QueryBatchPool::freeBatch(QueryBatch* queryBatch) {
    pool.release(queryBatch);
}

void QueryRecorder::beginSampleRenderQueries(
    const VulkanCommandInterface* vkiCommands,
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const RenderQuery* const> queries,
    uint32_t multiviewViewCount) {
    TEPHRA_ASSERT(!jobSemaphore.isNull());
    for (const RenderQuery* queryPtr : queries) {
        QueryHandle query = getQueryHandle(*queryPtr);
        TEPHRA_ASSERT(query->type == QueryType::Render);
        TEPHRA_ASSERTD(query->beginScopeVkQueryPool.isNull(), "Render query is already in a begun state.");

        // Allocate and record sample
        QueryBatch* batch = getBatch(query, multiviewViewCount);

        uint32_t vkQueryIndex = batch->allocateSamples(query, multiviewViewCount);
        bool isPrecise = query->type == QueryType::Render &&
            std::get<RenderQueryType>(query->subType) == RenderQueryType::OcclusionPrecise;
        vkiCommands->cmdBeginQuery(
            vkCommandBuffer, batch->vkGetQueryPoolHandle(), vkQueryIndex, isPrecise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);

        // Store the query index for the matching ending command
        query->beginScopeVkQueryPool = batch->vkGetQueryPoolHandle();
        query->beginScopeQueryIndex = vkQueryIndex;

        query->lastPendingSampleTimestamp = jobSemaphore.timestamp;
    }
}

void QueryRecorder::setJobSemaphore(const JobSemaphore& semaphore) {
    TEPHRA_ASSERT(jobSemaphore.isNull());
    TEPHRA_ASSERT(!semaphore.isNull());
    jobSemaphore = semaphore;
}

void QueryRecorder::endSampleRenderQueries(
    const VulkanCommandInterface* vkiCommands,
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const RenderQuery* const> queries) {
    for (const RenderQuery* queryPtr : queries) {
        QueryHandle query = getQueryHandle(*queryPtr);
        TEPHRA_ASSERT(query->type == QueryType::Render);
        TEPHRA_ASSERTD(!query->beginScopeVkQueryPool.isNull(), "Render query expected to be in a begun state.");

        vkiCommands->cmdEndQuery(vkCommandBuffer, query->beginScopeVkQueryPool, query->beginScopeQueryIndex);
        query->beginScopeVkQueryPool = {};
    }
}

void QueryRecorder::sampleTimestampQuery(
    const VulkanCommandInterface* vkiCommands,
    VkCommandBufferHandle vkCommandBuffer,
    const QueryHandle& query,
    PipelineStage stage,
    uint32_t multiviewViewCount) {
    TEPHRA_ASSERT(!jobSemaphore.isNull());
    TEPHRA_ASSERT(query->type == QueryType::Timestamp);

    // Allocate and record sample
    QueryBatch* batch = getBatch(query, multiviewViewCount);

    uint32_t vkQueryIndex = batch->allocateSamples(query, multiviewViewCount);
    vkiCommands->cmdWriteTimestamp(
        vkCommandBuffer, vkCastConvertibleEnum(stage), batch->vkGetQueryPoolHandle(), vkQueryIndex);

    query->lastPendingSampleTimestamp = jobSemaphore.timestamp;
}

void QueryRecorder::retrieveBatchesAndReset(ScratchVector<QueryBatch*>& batchList) {
    for (QueryBatch* batch : usedBatches)
        batchList.push_back(batch);

    usedBatches.clear();
    jobSemaphore = {};
}

void QueryRecorder::reset() {
    if (!usedBatches.empty()) {
        manager->freeDiscardedBatches(view(usedBatches));
    }
    usedBatches.clear();
    jobSemaphore = {};
}

QueryBatch* QueryRecorder::getBatch(const QueryHandle& query, uint32_t sampleCount) {
    auto [vkQueryType, pipelineStatistics] = query->decodeVkQueryType();
    auto checkBatch = [&](QueryBatch* batch) -> bool {
        TEPHRA_ASSERT(batch != nullptr);
        if (batch->getRemainingSampleCount() < sampleCount)
            return false;

        QueryBatchPool* pool = batch->getPool();
        return vkQueryType == pool->getVkQueryType() && pipelineStatistics == pool->getPipelineStatisticsFlag();
    };

    // Check used batches
    for (QueryBatch* batch : usedBatches) {
        if (checkBatch(batch))
            return batch;
    }

    // Allocate a new batch
    QueryBatch* newBatch = manager->allocateBatchForQuery(query);
    TEPHRA_ASSERT(checkBatch(newBatch));
    usedBatches.push_back(newBatch);
    return newBatch;
}

QueryManager::QueryManager(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {
    pendingBatchQueues.resize(deviceImpl->getQueueMap()->getQueueInfos().size());
}

void QueryManager::createTimestampQueries(ArrayParameter<TimestampQuery* const> queries) {
    std::lock_guard<Mutex> lock(recordMutex);

    for (std::size_t i = 0; i < queries.size(); i++) {
        QueryType type = QueryType::Timestamp;
        *queries[i] = TimestampQuery(this, createQueryRecord(type, {}));
    }
}

void QueryManager::createRenderQueries(
    ArrayParameter<const RenderQueryType> queryTypes,
    ArrayParameter<RenderQuery* const> queries) {
    TEPHRA_ASSERT(queryTypes.size() == queries.size());
    std::lock_guard<Mutex> lock(recordMutex);

    for (std::size_t i = 0; i < queryTypes.size(); i++) {
        QueryType type = QueryType::Render;
        RenderQueryType subType = queryTypes[i];
        *queries[i] = RenderQuery(this, createQueryRecord(type, subType));
    }
}

void QueryManager::queueFreeQuery(const QueryHandle& query) {
    TEPHRA_ASSERT(query != nullptr);
    std::lock_guard<Mutex> lock(recordMutex);
    return recordsToFree.push_back(query);
}

void QueryManager::freeDiscardedBatches(ArrayParameter<QueryBatch*> batches) {
    std::lock_guard<Mutex> lock(batchMutex);
    for (QueryBatch* batch : batches) {
        batch->getPool()->freeBatch(batch);
    }
}

void QueryManager::awaitBatches(ArrayParameter<QueryBatch*> batches, const JobSemaphore& semaphore) {
    TEPHRA_ASSERT(!semaphore.isNull());
    std::lock_guard<Mutex> lock(batchMutex);

    PendingBatchQueue& batchQueue = pendingBatchQueues[deviceImpl->getQueueMap()->getQueueUniqueIndex(semaphore.queue)];
    for (QueryBatch* batch : batches) {
        batchQueue.push_back({ semaphore, batch });
    }
}

void QueryManager::update() {
    // Readout batches
    {
        std::lock_guard<Mutex> lock(batchMutex);
        for (uint32_t queueIndex = 0; queueIndex < static_cast<uint32_t>(pendingBatchQueues.size()); queueIndex++) {
            PendingBatchQueue& batchQueue = pendingBatchQueues[queueIndex];

            uint64_t reachedTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestamp(queueIndex);

            while (!batchQueue.empty() && reachedTimestamp >= batchQueue.front().first.timestamp) {
                QueryBatch* batch = batchQueue.front().second;
                batch->readback(deviceImpl, batchQueue.front().first);
                batch->getPool()->freeBatch(batch);
                batchQueue.pop_front();
            }
        }
    }

    // Cleanup freed queries
    {
        std::lock_guard<Mutex> lock(recordMutex);
        uint64_t lastReachedTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestampInAllQueues();
        auto removeIt = std::remove_if(recordsToFree.begin(), recordsToFree.end(), [&](QueryRecord* record) {
            bool doFree = record->lastPendingSampleTimestamp <= lastReachedTimestamp;
            recordPool.release(record);
            return true;
        });
        recordsToFree.erase(removeIt, recordsToFree.end());
    }
}

QueryBatch* QueryManager::allocateBatchForQuery(const QueryHandle& query) {
    TEPHRA_ASSERT(query != nullptr);
    std::lock_guard<Mutex> lock(batchMutex);

    return query->batchPool->allocateBatch();
}

QueryHandle QueryManager::createQueryRecord(QueryType type, std::variant<std::monostate, RenderQueryType> subType) {
    QueryRecord* query = recordPool.acquireExisting();
    if (query == nullptr)
        query = recordPool.acquireNew();

    query->type = type;
    query->subType = subType;
    auto [vkType, pipelineStatistics] = query->decodeVkQueryType();
    query->batchPool = &getOrCreatePool(vkType, pipelineStatistics);
    query->maxResultsHistorySize = QueryRecord::MinMaxResultsHistorySize;
    query->lastResultIndex = 0;
    query->beginScopeVkQueryPool = {};
    query->beginScopeQueryIndex = ~0u;
    query->lastPendingSampleTimestamp = 0;

    return query;
}

QueryBatchPool& QueryManager::getOrCreatePool(
    VkQueryType vkQueryType,
    VkQueryPipelineStatisticFlagBits pipelineStatistics) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(queryPools.size()); i++) {
        QueryBatchPool& pool = queryPools[i];
        if (pool.getVkQueryType() == vkQueryType &&
            (vkQueryType != VK_QUERY_TYPE_PIPELINE_STATISTICS ||
             pipelineStatistics == pool.getPipelineStatisticsFlag())) {
            return pool;
        }
    }

    queryPools.emplace_back(deviceImpl, vkQueryType, pipelineStatistics);
    return queryPools.back();
}

}
