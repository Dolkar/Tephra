#pragma once

#include "logical_device.hpp"
#include "../utils/object_pool.hpp"
#include "../common_impl.hpp"

#include <tephra/device.hpp>
#include <tephra/query.hpp>

namespace tp {

class QueryPool {
public:
    QueryPool(LogicalDevice* device, VkQueryPoolCreateInfo createInfo) : device(device), createInfo(createInfo) {}

private:
    struct SubPool {};
    VkQueryPoolCreateInfo createInfo;
    LogicalDevice* device;
};

class QueryManager {
public:
    explicit QueryManager(LogicalDevice* device) : device(device) {}

    void createQueries(ArrayParameter<const QueryType> queryTypes, ArrayParameter<Query* const> queries);

    QueryType getQueryType(Query::Handle handle) const;
    QueryResult getQueryResult(Query::Handle handle) const;

    void queueFreeQuery(Query::Handle handle);

private:
    struct QueryEntry {
        QueryType type;
        QueryResult result;
        uint32_t poolIndex;
    };

    LogicalDevice* device;
    ObjectPool<QueryEntry> entryPool;
    std::vector<QueryEntry*> entriesToFree;
};

}
