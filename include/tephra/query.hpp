#pragma once

#include <tephra/common.hpp>

namespace tp {

enum class QueryType {
    Timestamp,
    Scoped,
};

enum class ScopedQueryType {
    // Occlusion queries
    Occlusion,
    // Occlusion queries with PRECISE bit
    OcclusionPrecise,
    // Pipeline statistics queries
    InputAssemblyVertices,
    InputAssemblyPrimitives,
    VertexShaderInvocations,
    GeometryShaderInvocations,
    GeometryShaderPrimitives,
    ClippingInvocations,
    ClippingPrimitives,
    FragmentShaderInvocations,
    TessellationControlShaderPatches,
    TessellationEvaluationShaderInvocations,
    ComputeShaderInvocations,
};

struct QueryResult {
    uint64_t value = 0;
    uint64_t jobSemaphoreTimestamp = 0;

    bool isNull() const {
        return jobSemaphoreTimestamp == 0;
    }
};

class QueryManager;

class BaseQuery {
    friend class QueryManager;

public:
    using Handle = void*;

    bool isNull() const {
        return parentManager == nullptr;
    }

    QueryResult getResult() const;

    TEPHRA_MAKE_NONCOPYABLE(BaseQuery);
    TEPHRA_MAKE_MOVABLE(BaseQuery);
    ~BaseQuery() noexcept;

protected:
    BaseQuery() : parentManager(nullptr), handle(nullptr) {}
    BaseQuery(QueryManager* parentManager, Handle handle) : parentManager(parentManager), handle(handle) {}

private:
    QueryManager* parentManager;
    Handle handle;
};

class TimestampQuery : public BaseQuery {
public:
    TimestampQuery() : BaseQuery() {}
    TimestampQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

class ScopedQuery : public BaseQuery {
public:
    ScopedQuery() : BaseQuery() {}
    ScopedQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

}
