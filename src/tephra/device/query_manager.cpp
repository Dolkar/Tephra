#include "query_manager.hpp"
#include "device_container.hpp"

#include <algorithm>

namespace tp {

const tp::JobSemaphore& BaseQuery::getResultJobSemaphore() const {
    TEPHRA_ASSERT(handle != nullptr);
    return handle->result.jobSemaphore;
}

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

BaseQuery::~BaseQuery() noexcept {
    if (!isNull()) {
        parentManager->queueFreeQuery(handle);
    }
}

uint64_t TimestampQuery::getResult() const {
    TEPHRA_ASSERT(handle != nullptr);
    return handle->result.value;
}

double TimestampQuery::getResultSeconds() const {
    TEPHRA_ASSERT(!isNull());
    return parentManager->convertTimestampToSeconds(getResult());
}

uint64_t RenderQuery::getResult() const {
    TEPHRA_ASSERT(handle != nullptr);
    return handle->result.value;
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

    // The range is allowed to span multiple pools
    while (true) {
        TEPHRA_ASSERT(vkPoolIndex <= vkQueryPools.size());

        uint32_t queryCountInPool = tp::max(firstQueryInPool + count, QueriesInPool) - firstQueryInPool;
        deviceImpl->getLogicalDevice()->getQueryResultsAndReset(
            vkQueryPools[vkPoolIndex].vkGetHandle(),
            firstQueryInPool,
            queryCountInPool,
            tp::viewRange(data, 0, queryCountInPool));

        freeRanges.push_back({ vkPoolIndex * QueriesInPool + firstQueryInPool, queryCountInPool });
        if (queryCountInPool == count)
            return;

        // Advance to next pool
        count -= queryCountInPool;
        vkPoolIndex++;
        firstQueryInPool = 0;
        data = tp::viewRange(data, queryCountInPool, data.size() - queryCountInPool);
    }
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

void QueryEntry::updateResult(ArrayView<uint64_t> queryData, const tp::JobSemaphore& semaphore) {
    // QueryData can have multiple entries if used during multiview. Here we decide how to combine them.
    // It's possible to expose all of them to the user, but that is rarely needed, is implementation
    // dependent and would complicate the API.
    TEPHRA_ASSERT(queryData.size() > 0);
    TEPHRA_ASSERT(!semaphore.isNull());

    if (semaphore.timestamp < result.jobSemaphore.timestamp)
        return;

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

    result = { newResultValue, semaphore };
}

QueryManager::QueryManager(DeviceContainer* deviceImpl, const VulkanCommandInterface* vkiCommands)
    : deviceImpl(deviceImpl), vkiCommands(vkiCommands) {
    const VkPhysicalDeviceLimits& limits = deviceImpl->getPhysicalDevice()->vkQueryFeatures<VkPhysicalDeviceLimits>();
    double nanosecondsToSeconds = 1.0E-9;
    ticksToSecondsFactor = limits.timestampPeriod * nanosecondsToSeconds;
}

void QueryManager::createTimestampQueries(ArrayParameter<TimestampQuery* const> queries) {
    std::lock_guard<Mutex> lock(globalMutex);

    for (std::size_t i = 0; i < queries.size(); i++) {
        QueryType type = QueryType::Render;
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
    ArrayParameter<const QueryHandle> queries,
    uint32_t multiviewViewCount,
    const JobSemaphore& semaphore) {
    // TODO: We might want to aggregate pipeline statistics queries in the future.
    // We'll need to delay pool creation for them until this point, where we combine them together.
    // This means we'll have multiple QuerySamples backed by a single Vulkan query.
    // QuerySample will need to store its own poolIndex as well, as the entries will only cache the last one.
    std::lock_guard<Mutex> lock(globalMutex);

    for (QueryEntry* query : queries) {
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
    ArrayParameter<const QueryHandle> queries) {
    std::lock_guard<Mutex> lock(globalMutex);

    for (QueryEntry* query : queries) {
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
                } else if (deviceImpl->isJobSemaphoreSignalled(sample.semaphore)) {
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

double QueryManager::convertTimestampToSeconds(uint64_t timestampQueryResult) const {
    return ticksToSecondsFactor * timestampQueryResult;
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
    query->result = {};

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

        entry->updateResult(tp::viewRange(queryData, 0, sample.vkQueryCount), sample.semaphore);
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
