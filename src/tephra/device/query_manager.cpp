
#include "query_manager.hpp"

namespace tp {

QueryType Query::getType() const {
    return parentManager->getQueryType(handle);
}

QueryResult Query::getResult() const {
    return parentManager->getQueryResult(handle);
}

Query::Query(Query&& other) noexcept : parentManager(other.parentManager), handle(other.handle) {
    if (!other.isNull()) {
        other.parentManager = nullptr;
    }
}

Query& Query::operator=(Query&& other) noexcept {
    std::swap(parentManager, other.parentManager);
    std::swap(handle, other.handle);
    return *this;
}

Query::~Query() noexcept {
    if (!isNull()) {
        parentManager->queueFreeQuery(handle);
    }
}

std::pair<VkQueryType, VkQueryPipelineStatisticFlagBits> decodeVkQueryType(QueryType type) {
    switch (type) {
    case tp::QueryType::Timestamp:
        return { VK_QUERY_TYPE_TIMESTAMP, VkQueryPipelineStatisticFlagBits() };
    case tp::QueryType::VisibleFragmentSamples:
    case tp::QueryType::VisibleFragmentSamplesPrecise:
        return { VK_QUERY_TYPE_OCCLUSION, VkQueryPipelineStatisticFlagBits() };
    case tp::QueryType::InputAssemblyVertices:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT };
    case tp::QueryType::InputAssemblyPrimitives:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT };
    case tp::QueryType::VertexShaderInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT };
    case tp::QueryType::GeometryShaderInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT };
    case tp::QueryType::GeometryShaderPrimitives:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT };
    case tp::QueryType::ClippingInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT };
    case tp::QueryType::ClippingPrimitives:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT };
    case tp::QueryType::FragmentShaderInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT };
    case tp::QueryType::TessellationControlShaderPatches:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                 VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT };
    case tp::QueryType::TessellationEvaluationShaderInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS,
                 VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT };
    case tp::QueryType::ComputeShaderInvocations:
        return { VK_QUERY_TYPE_PIPELINE_STATISTICS, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT };
    default:
        TEPHRA_ASSERTD(false, "Unexpected QueryType");
        return {};
    }
}

void QueryManager::createQueries(ArrayParameter<const QueryType> queryTypes, ArrayParameter<Query* const> queries) {
    TEPHRA_ASSERT(queryTypes.size() == queries.size());
    for (std::size_t i = 0; i < queryTypes.size(); i++) {
        QueryType type = queryTypes[i];
        auto [vkType, pipelineStatistics] = decodeVkQueryType(type);

        // Let's grab a pool for it right away, unless it's a pipeline statistics query, in which case we should
        // determine the pool at runtime
        uint32_t poolIndex = ~0;
        if (vkType != VK_QUERY_TYPE_PIPELINE_STATISTICS) {
            poolIndex = getOrCreatePool(vkType, pipelineStatistics);
        }

        QueryEntry* entry = entryPool.acquireExisting();
        if (entry == nullptr)
            entry = entryPool.acquireNew();

        entry->type = type;
        entry->result = {};
        entry->lastPoolIndex = poolIndex;

        *queries[i] = Query(this, reinterpret_cast<Query::Handle>(entry));
    }
}

QueryType QueryManager::getQueryType(Query::Handle handle) const {
    TEPHRA_ASSERT(handle != nullptr);
    return reinterpret_cast<QueryEntry*>(handle)->type;
}

QueryResult QueryManager::getQueryResult(Query::Handle handle) const {
    TEPHRA_ASSERT(handle != nullptr);
    return reinterpret_cast<QueryEntry*>(handle)->result;
}

void QueryManager::queueFreeQuery(Query::Handle handle) {
    TEPHRA_ASSERT(handle != nullptr);
    return entriesToFree.push_back(reinterpret_cast<QueryEntry*>(handle));
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

    queryPools.emplace_back(device, vkQueryType, pipelineStatistics);
    return static_cast<uint32_t>(queryPools.size() - 1);
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
        vkQueryPools.push_back(device->createQueryPool(vkQueryType, pipelineStatistics, QueriesInPool));
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
        device->getQueryResultsAndReset(
            vkQueryPools[vkPoolIndex], firstQueryInPool, queryCountInPool, tp::viewRange(data, 0, queryCountInPool));

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

}
