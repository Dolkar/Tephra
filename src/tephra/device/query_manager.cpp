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
    handle->maxResultsHistorySize = tp::max(size, QueryEntry::MinMaxResultsHistorySize);
}

BaseQuery::BaseQuery(QueryManager* parentManager, Handle handle) : parentManager(parentManager), handle(handle) {
    TEPHRA_ASSERT(parentManager != nullptr && handle != nullptr);
}

BaseQuery::~BaseQuery() noexcept {
    if (!isNull()) {
        parentManager->queueFreeQuery(handle);
    }
}

QueryBatch::QueryBatch(QueryBatchPool* batchPool, Lifeguard<VkQueryPoolHandle> vkQueryPool)
    : batchPool(batchPool), vkQueryPool(std::move(vkQueryPool)) {}

void QueryBatch::assignToJob(const JobSemaphore& semaphore) {
    TEPHRA_ASSERT(semaphore.isNull());
    this->semaphore = semaphore;
}

uint32_t QueryBatch::allocateSamples(QueryEntry* entry, uint32_t count) {
    TEPHRA_ASSERT(!semaphore.isNull());
    TEPHRA_ASSERT(entry != nullptr);
    TEPHRA_ASSERT(entry->batchPool == batchPool);

    if (usedCount + count > MaxSampleCount) {
        return InvalidIndex;
    } else {
        uint32_t firstIndex = usedCount;
        usedCount += count;

        for (uint32_t i = 0; i < count; i++) {
            samples[firstIndex + i] = entry;
        }

        return firstIndex;
    }
}

void QueryBatch::readbackAndReset(DeviceContainer* deviceImpl) {
    TEPHRA_ASSERT(usedCount <= MaxSampleCount);

    uint64_t sampleData[MaxSampleCount];
    deviceImpl->getLogicalDevice()->getQueryResultsAndReset(
        vkQueryPool.vkGetHandle(), 0, usedCount, tp::viewRange(sampleData, 0, usedCount));

    // Consecutive samples with the same entry will be updated together
    uint32_t si = 0;
    for (uint32_t i = 1; i < usedCount; i++) {
        if (samples[si] != samples[i]) {
            samples[si]->updateResults(tp::viewRange(sampleData, si, i - si), semaphore);
            si = i;
        }
    }
    if (si < usedCount)
        samples[si]->updateResults(tp::viewRange(sampleData, si, usedCount - si), semaphore);

    // Now reset
    usedCount = 0;
    semaphore = {};
}

QueryBatch* QueryBatchPool::allocateBatch(const JobSemaphore& semaphore) {
    QueryBatch* batch = pool.acquireExisting();
    if (batch == nullptr) {
        VkQueryPoolHandle vkQueryPool = deviceImpl->getLogicalDevice()->createQueryPool(
            vkQueryType, pipelineStatistics, QueryBatch::MaxSampleCount);
        batch = pool.acquireNew(this, pool.objectsAllocated(), deviceImpl->vkMakeHandleLifeguard(vkQueryPool));
    }

    return batch;
}

void QueryBatchPool::freeBatch(QueryBatch* queryBatch) {
    pool.release(queryBatch);
}

std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> QueryEntry::decodeVkQueryType() const {
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

void QueryEntry::updateResults(ArrayView<uint64_t> sampleData, const tp::JobSemaphore& semaphore) {
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

    if (semaphore.timestamp <= resultToUpdate->jobSemaphore.timestamp)
        return;

    // Sample can have multiple entries if used during multiview. Here we decide how to combine them.
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

QueryManager::QueryManager(DeviceContainer* deviceImpl, const VulkanCommandInterface* vkiCommands)
    : deviceImpl(deviceImpl), vkiCommands(vkiCommands) {}

void QueryManager::createTimestampQueries(ArrayParameter<TimestampQuery* const> queries) {
    std::lock_guard<Mutex> lock(globalMutex);

    for (std::size_t i = 0; i < queries.size(); i++) {
        QueryType type = QueryType::Timestamp;
        *queries[i] = TimestampQuery(this, createQuery(type, {}));
    }
}

void QueryManager::createRenderQueries(
    ArrayParameter<const RenderQueryType> queryTypes,
    ArrayParameter<RenderQuery* const> queries) {
    TEPHRA_ASSERT(queryTypes.size() == queries.size());
    std::lock_guard<Mutex> lock(globalMutex);

    for (std::size_t i = 0; i < queryTypes.size(); i++) {
        QueryType type = QueryType::Render;
        RenderQueryType subType = queryTypes[i];
        *queries[i] = RenderQuery(this, createQuery(type, subType));
    }
}

void QueryManager::beginSampleRenderQueries(
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const RenderQuery* const> queries,
    uint32_t multiviewViewCount,
    const JobSemaphore& semaphore) {
    std::lock_guard<Mutex> lock(globalMutex);

    for (const RenderQuery* queryPtr : queries) {
        QueryHandle query = getQueryHandle(*queryPtr);
        TEPHRA_ASSERT(query->type == QueryType::Render);
        TEPHRA_ASSERTD(
            query->beginVkQueryIndex == QueryEntry::InvalidIndex, "Render query is already in a begun state.");

        // Allocate and record sample
        uint32_t vkQueryIndex = queryPools[query->poolIndex].allocateVkQueries(multiviewViewCount);
        pendingSamples.push_back(QuerySample(query, vkQueryIndex, multiviewViewCount, semaphore));

        bool isPrecise = query->type == QueryType::Render &&
            std::get<RenderQueryType>(query->subType) == RenderQueryType::OcclusionPrecise;
        cmdBeginQuery(vkCommandBuffer, query->poolIndex, vkQueryIndex, isPrecise);

        // Record the query index for the matching ending command
        query->beginVkQueryIndex = vkQueryIndex;
        query->lastPendingSampleTimestamp = semaphore.timestamp;
    }
}

void QueryManager::endSampleRenderQueries(
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const RenderQuery* const> queries) {
    std::lock_guard<Mutex> lock(globalMutex);

    for (const RenderQuery* queryPtr : queries) {
        QueryHandle query = getQueryHandle(*queryPtr);
        TEPHRA_ASSERTD(
            query->beginVkQueryIndex != QueryEntry::InvalidIndex, "Render query expected to be in a begun state.");

        cmdEndQuery(vkCommandBuffer, query->poolIndex, query->beginVkQueryIndex);
        query->beginVkQueryIndex = QueryEntry::InvalidIndex;
    }
}

void QueryManager::sampleTimestampQuery(
    VkCommandBufferHandle vkCommandBuffer,
    const QueryHandle& query,
    PipelineStage stage,
    uint32_t multiviewViewCount,
    const JobSemaphore& semaphore) {
    std::lock_guard<Mutex> lock(globalMutex);

    // Allocate and record sample
    uint32_t vkQueryIndex = queryPools[query->poolIndex].allocateVkQueries(multiviewViewCount);
    pendingSamples.push_back(QuerySample(query, vkQueryIndex, multiviewViewCount, semaphore));

    cmdWriteTimestamp(vkCommandBuffer, query->poolIndex, vkQueryIndex, stage);
    query->lastPendingSampleTimestamp = semaphore.timestamp;
}

void QueryManager::queueFreeQuery(const QueryHandle& query) {
    TEPHRA_ASSERT(query != nullptr);
    std::lock_guard<Mutex> lock(globalMutex);
    return entriesToFree.push_back(query);
}

void QueryManager::update() {
    std::lock_guard<Mutex> lock(globalMutex);

    // Find and readout all already processed samples, with simple caching of the last encountered semaphore
    ScratchVector<QuerySample> samplesToReadout;
    {
        JobSemaphore previousSignalledSemaphore;
        auto removeIt = std::remove_if(
            pendingSamples.begin(), pendingSamples.end(), [&](const QuerySample& sample) mutable {
                if (sample.semaphore.queue == previousSignalledSemaphore.queue &&
                    sample.semaphore.timestamp <= previousSignalledSemaphore.timestamp) {
                    samplesToReadout.push_back(sample);
                    return true;
                } else if (deviceImpl->getTimelineManager()->wasTimestampReachedInQueue(
                               deviceImpl->getQueueMap()->getQueueUniqueIndex(sample.semaphore.queue),
                               sample.semaphore.timestamp)) {
                    previousSignalledSemaphore = sample.semaphore;
                    samplesToReadout.push_back(sample);
                    return true;
                }
                return false;
            });
        pendingSamples.erase(removeIt, pendingSamples.end());
    }

    readoutSamples(samplesToReadout);

    // Cleanup freed queries
    {
        uint64_t lastReachedTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestampInAllQueues();
        auto removeIt = std::remove_if(entriesToFree.begin(), entriesToFree.end(), [&](QueryEntry* entry) {
            bool doFree = entry->lastPendingSampleTimestamp <= lastReachedTimestamp;
            entryPool.release(entry);
            return true;
        });
        entriesToFree.erase(removeIt, entriesToFree.end());
    }
}

QueryHandle QueryManager::createQuery(QueryType type, std::variant<std::monostate, RenderQueryType> subType) {
    QueryEntry* query = entryPool.acquireExisting();
    if (query == nullptr)
        query = entryPool.acquireNew();

    query->type = type;
    query->subType = subType;
    query->maxResultsHistorySize = QueryEntry::MinMaxResultsHistorySize;
    query->lastResultIndex = 0;

    auto [vkType, pipelineStatistics] = query->decodeVkQueryType();
    query->poolIndex = getOrCreatePool(vkType, pipelineStatistics);
    query->beginVkQueryIndex = QueryEntry::InvalidIndex;
    query->lastPendingSampleTimestamp = 0;

    return query;
}

uint32_t QueryManager::getOrCreatePool(VkQueryType vkQueryType, VkQueryPipelineStatisticFlagBits pipelineStatistics) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(queryPools.size()); i++) {
        const QueryPool& pool = queryPools[i];
        if (pool.getVkQueryType() == vkQueryType &&
            (vkQueryType != VK_QUERY_TYPE_PIPELINE_STATISTICS ||
             pipelineStatistics == pool.getPipelineStatisticsFlags())) {
            return i;
        }
    }

    queryPools.emplace_back(deviceImpl, vkQueryType, pipelineStatistics);
    return static_cast<uint32_t>(queryPools.size() - 1);
}

void QueryManager::cmdBeginQuery(
    VkCommandBufferHandle vkCommandBuffer,
    uint32_t poolIndex,
    uint32_t vkQueryIndex,
    bool isPrecise) {
    TEPHRA_ASSERT(poolIndex < queryPools.size());
    auto [vkPool, query] = queryPools[poolIndex].lookupQuery(vkQueryIndex);

    vkiCommands->cmdBeginQuery(vkCommandBuffer, vkPool, query, isPrecise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
}

void QueryManager::cmdEndQuery(VkCommandBufferHandle vkCommandBuffer, uint32_t poolIndex, uint32_t vkQueryIndex) {
    TEPHRA_ASSERT(poolIndex < queryPools.size());
    auto [vkPool, query] = queryPools[poolIndex].lookupQuery(vkQueryIndex);

    vkiCommands->cmdEndQuery(vkCommandBuffer, vkPool, query);
}

void QueryManager::cmdWriteTimestamp(
    VkCommandBufferHandle vkCommandBuffer,
    uint32_t poolIndex,
    uint32_t vkQueryIndex,
    PipelineStage stage) {
    TEPHRA_ASSERT(poolIndex < queryPools.size());
    auto [vkPool, query] = queryPools[poolIndex].lookupQuery(vkQueryIndex);

    vkiCommands->cmdWriteTimestamp(vkCommandBuffer, vkCastConvertibleEnum(stage), vkPool, query);
}

}
