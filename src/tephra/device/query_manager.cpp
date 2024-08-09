#include "query_manager.hpp"
#include "device_container.hpp"

namespace tp {

QueryResult BaseQuery::getResult() const {
    return parentManager->getQueryResult(handle);
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
        fittingRange = freeRanges.size();
        freeRanges.push_back({ vkQueryPools.size() * QueriesInPool, QueriesInPool });

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

    // The range can span multiple pools...
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

void QueryManager::createTimestampQueries(ArrayParameter<TimestampQuery* const> queries) {
    for (std::size_t i = 0; i < queries.size(); i++) {
        QueryType type = QueryType::Scoped;
        *queries[i] = TimestampQuery(this, createQuery(type, {}));
    }
}

void QueryManager::createScopedQueries(
    ArrayParameter<const ScopedQueryType> queryTypes,
    ArrayParameter<ScopedQuery* const> queries) {
    TEPHRA_ASSERT(queryTypes.size() == queries.size());
    for (std::size_t i = 0; i < queryTypes.size(); i++) {
        QueryType type = QueryType::Scoped;
        ScopedQueryType subType = queryTypes[i];
        *queries[i] = ScopedQuery(this, createQuery(type, subType));
    }
}

void QueryManager::beginSampleScopedQueries(
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const ScopedQuery> queries,
    uint32_t multiviewViewCount,
    const tp::JobSemaphore& semaphore) {
    // TODO: We might want to aggregate pipeline statistics queries in the future.
    // We'll need to delay pool creation for them until this point, where we combine them together.
    // This means we'll have multiple QuerySamples backed by a single Vulkan query.
    // QuerySample will need to store its own poolIndex as well, as the entries will only cache the last one.

    for (const ScopedQuery& query : queries) {
        QueryEntry* entry = reinterpret_cast<QueryEntry*>(query.handle);
        TEPHRA_ASSERTD(entry->beginVkQueryIndex == InvalidIndex, "Scoped query is already in a begun state.");

        // Allocate and record sample
        uint32_t vkQueryIndex = queryPools[entry->poolIndex].allocateVkQueries(multiviewViewCount);
        pendingSamples.push_back(QuerySample(entry, vkQueryIndex, multiviewViewCount, semaphore));

        bool isPrecise = entry->type == QueryType::Scoped &&
            std::get<ScopedQueryType>(entry->subType) == ScopedQueryType::OcclusionPrecise;
        cmdBeginQuery(vkCommandBuffer, entry->poolIndex, vkQueryIndex, isPrecise);

        // Record the query index for the matching ending command
        entry->beginVkQueryIndex = vkQueryIndex;
        entry->lastPendingSampleTimestamp = semaphore.timestamp;
    }
}

void QueryManager::endSampleScopedQueries(
    VkCommandBufferHandle vkCommandBuffer,
    ArrayParameter<const ScopedQuery> queries) {
    for (const ScopedQuery& query : queries) {
        QueryEntry* entry = reinterpret_cast<QueryEntry*>(query.handle);
        TEPHRA_ASSERTD(entry->beginVkQueryIndex != InvalidIndex, "Scoped query expected to be in a begun state.");

        cmdEndQuery(vkCommandBuffer, entry->poolIndex, entry->beginVkQueryIndex);
        entry->beginVkQueryIndex = InvalidIndex;
    }
}

void QueryManager::writeTimestampQuery(
    VkCommandBufferHandle vkCommandBuffer,
    const TimestampQuery& query,
    PipelineStage stage,
    uint32_t multiviewViewCount,
    const tp::JobSemaphore& semaphore) {
    QueryEntry* entry = reinterpret_cast<QueryEntry*>(query.handle);

    // Allocate and record sample
    uint32_t vkQueryIndex = queryPools[entry->poolIndex].allocateVkQueries(multiviewViewCount);
    pendingSamples.push_back(QuerySample(entry, vkQueryIndex, multiviewViewCount, semaphore));

    cmdWriteTimestamp(vkCommandBuffer, entry->poolIndex, vkQueryIndex, stage);

    entry->lastPendingSampleTimestamp = semaphore.timestamp;
}

void QueryManager::update() {
    // Find and remove all already processed samples, with simple caching of the last encountered semaphore
    ScratchVector<QuerySample> samplesToReadout;
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

    readoutSamples(samplesToReadout);
}

QueryResult QueryManager::getQueryResult(BaseQuery::Handle handle) const {
    TEPHRA_ASSERT(handle != nullptr);
    return reinterpret_cast<QueryEntry*>(handle)->result;
}

void QueryManager::queueFreeQuery(BaseQuery::Handle handle) {
    TEPHRA_ASSERT(handle != nullptr);
    return entriesToFree.push_back(reinterpret_cast<QueryEntry*>(handle));
}

QueryManager::QuerySample::QuerySample(
    QueryEntry* entry,
    uint32_t vkQueryIndex,
    uint32_t multiviewViewCount,
    const tp::JobSemaphore& semaphore)
    : entry(entry), vkQueryIndex(vkQueryIndex), vkQueryCount(multiviewViewCount), semaphore(semaphore) {}

BaseQuery::Handle QueryManager::createQuery(QueryType type, std::variant<std::monostate, ScopedQueryType> subType) {
    QueryEntry* entry = entryPool.acquireExisting();
    if (entry == nullptr)
        entry = entryPool.acquireNew();

    entry->type = type;
    entry->subType = subType;
    entry->result = {};

    auto [vkType, pipelineStatistics] = entry->decodeVkQueryType();
    entry->poolIndex = getOrCreatePool(vkType, pipelineStatistics);
    entry->beginVkQueryIndex = InvalidIndex;
    entry->lastPendingSampleTimestamp = 0;

    return reinterpret_cast<BaseQuery::Handle>(entry);
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

void QueryManager::readoutSamples(const ScratchVector<QuerySample>& samples) {}

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

std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> QueryManager::QueryEntry::decodeVkQueryType() const {
    switch (type) {
    case QueryType::Timestamp:
        return { VK_QUERY_TYPE_TIMESTAMP, VkQueryPipelineStatisticFlagBits() };
    case QueryType::Scoped: {
        ScopedQueryType scopedType = std::get<ScopedQueryType>(subType);
        switch (scopedType) {
        case ScopedQueryType::Occlusion:
        case ScopedQueryType::OcclusionPrecise:
            return { VK_QUERY_TYPE_OCCLUSION, VkQueryPipelineStatisticFlagBits() };
        case ScopedQueryType::InputAssemblyVertices:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT };
        case ScopedQueryType::InputAssemblyPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT };
        case ScopedQueryType::VertexShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT };
        case ScopedQueryType::GeometryShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT };
        case ScopedQueryType::GeometryShaderPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT };
        case ScopedQueryType::ClippingInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT };
        case ScopedQueryType::ClippingPrimitives:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT };
        case ScopedQueryType::FragmentShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT };
        case ScopedQueryType::TessellationControlShaderPatches:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                     VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT };
        case ScopedQueryType::TessellationEvaluationShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                     VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT };
        case ScopedQueryType::ComputeShaderInvocations:
            return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT };
        default:
            TEPHRA_ASSERTD(false, "Unexpected ScopedQueryType");
            return {};
        }
    }
    default:
        TEPHRA_ASSERTD(false, "Unexpected QueryType");
        return {};
    }
}

}
