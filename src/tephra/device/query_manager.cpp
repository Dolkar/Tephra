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

std::pair<VkQueryPoolHandle, uint32_t> QueryPool::lookupQuery(uint32_t index) const {
    uint32_t vkPoolIndex = index / QueriesInPool;
    uint32_t firstQueryInPool = index % QueriesInPool;

    TEPHRA_ASSERT(vkPoolIndex <= vkQueryPools.size());
    return { vkQueryPools[vkPoolIndex].vkGetHandle(), firstQueryInPool };
}

uint32_t QueryPool::allocateVkQueries(uint32_t count) {
    TEPHRA_ASSERT(count != 0);
    int fittingRange = -1;
    for (int i = 0; i < freeRanges.size(); i++) {
        auto [rangeFirst, rangeCount] = freeRanges[i];
        if (count <= rangeCount) {
            fittingRange = i;
            break;
        }
    }

    // Allocate a new pool
    if (fittingRange == -1) {
        fittingRange = static_cast<int>(freeRanges.size());
        freeRanges.push_back({ static_cast<uint32_t>(vkQueryPools.size()) * QueriesInPool, QueriesInPool });

        VkQueryPoolHandle vkQueryPool = deviceImpl->getLogicalDevice()->createQueryPool(
            vkQueryType, pipelineStatistics, QueriesInPool);
        vkQueryPools.push_back(deviceImpl->vkMakeHandleLifeguard(vkQueryPool));
    }

    auto& [rangeFirst, rangeCount] = freeRanges[fittingRange];
    uint32_t firstIndex = rangeFirst;

    if (rangeCount == count) {
        freeRanges.erase(freeRanges.begin() + fittingRange);
    } else {
        rangeFirst += count;
        rangeCount -= count;
    }

    return firstIndex;
}

void QueryPool::readbackAndFreeVkQueries(uint32_t firstIndex, uint32_t count, ArrayView<uint64_t> data) {
    TEPHRA_ASSERT(count == data.size());
    uint32_t vkPoolIndex = firstIndex / QueriesInPool;
    uint32_t firstQueryInPool = firstIndex % QueriesInPool;
    TEPHRA_ASSERT(vkPoolIndex <= vkQueryPools.size());
    TEPHRA_ASSERT(firstQueryInPool + count <= QueriesInPool);

    deviceImpl->getLogicalDevice()->getQueryResultsAndReset(
        vkQueryPools[vkPoolIndex].vkGetHandle(), firstQueryInPool, count, data);

    freeRanges.push_back({ vkPoolIndex * QueriesInPool + firstQueryInPool, count });
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

void QueryEntry::updateResults(ArrayView<uint64_t> queryData, const tp::JobSemaphore& semaphore) {
    TEPHRA_ASSERT(queryData.size() > 0);
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

    // QueryData can have multiple entries if used during multiview. Here we decide how to combine them.
    // It's possible to expose all of them to the user, but that is rarely needed, is implementation
    // dependent and would complicate the API.
    uint64_t newResultValue = 0;
    switch (type) {
    case QueryType::Timestamp:
        // Just use the first view for timestamps
        newResultValue = queryData[0];
        break;
    case QueryType::Render:
        // Render query types should be summed together
        for (uint64_t value : queryData)
            newResultValue += value;
        break;
    default:
        TEPHRA_ASSERTD(false, "Unexpected QueryType");
    }

    // Overwrite oldest result
    *resultToUpdate = { semaphore, newResultValue };

    QueryResult* lastResult = &resultsHistory[lastResultIndex];
    // Expecting at least two results in history
    TEPHRA_ASSERT(lastResult != resultToUpdate);

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
    // TODO: We might want to aggregate pipeline statistics queries in the future.
    // We'll need to delay pool creation for them until this point, where we combine them together.
    // This means we'll have multiple QuerySamples backed by a single Vulkan query.
    // QuerySample will need to store its own poolIndex as well, as the entries will only cache the last one.
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

QueryManager::QuerySample::QuerySample(
    QueryEntry* entry,
    uint32_t vkQueryIndex,
    uint32_t multiviewViewCount,
    const JobSemaphore& semaphore)
    : entry(entry), vkQueryIndex(vkQueryIndex), vkQueryCount(multiviewViewCount), semaphore(semaphore) {
    TEPHRA_ASSERT(vkQueryCount <= MaxQueryCount);
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

void QueryManager::readoutSamples(const ScratchVector<QuerySample>& samples) {
    // There is an optimization opportunity here to aggregate query samples from the same pool to process multiple
    // of them with a single readbackAndFreeVkQueries call
    for (const QuerySample& sample : samples) {
        QueryEntry* entry = sample.entry;

        uint64_t queryData[QuerySample::MaxQueryCount];
        queryPools[entry->poolIndex].readbackAndFreeVkQueries(
            sample.vkQueryIndex, sample.vkQueryCount, tp::viewRange(queryData, 0, sample.vkQueryCount));

        entry->updateResults(tp::viewRange(queryData, 0, sample.vkQueryCount), sample.semaphore);
    }
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
