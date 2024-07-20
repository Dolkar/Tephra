
#include "query_manager.hpp"

namespace tp {

QueryType Query::getType() const {
    return parentManager->getQueryType(handle);
}

QueryResult Query::getResult() const {
    return parentManager->getQueryResult(handle);
}

Query::Query(Query&& other) noexcept {}

Query& Query::operator=(Query&& other) noexcept {}

Query::~Query() noexcept {}

void QueryManager::createQueries(ArrayParameter<const QueryType> queryTypes, ArrayParameter<Query* const> queries) {}

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

}
