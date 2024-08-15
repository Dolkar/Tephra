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

class QueryManager;

class BaseQuery {
    friend class QueryManager;

public:
    using Handle = void*;

    bool isNull() const {
        return parentManager == nullptr;
    }

    const tp::JobSemaphore& getResultJobSemaphore() const;

    TEPHRA_MAKE_NONCOPYABLE(BaseQuery);
    TEPHRA_MAKE_MOVABLE(BaseQuery);

protected:
    BaseQuery() : parentManager(nullptr), handle(nullptr) {}
    BaseQuery(QueryManager* parentManager, Handle handle) : parentManager(parentManager), handle(handle) {}
    ~BaseQuery() noexcept;

    QueryManager* parentManager;
    Handle handle;
};

class TimestampQuery : public BaseQuery {
public:
    TimestampQuery() : BaseQuery() {}
    TimestampQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}

    uint64_t getResult() const;
    double getResultSeconds() const;
};

class ScopedQuery : public BaseQuery {
public:
    ScopedQuery() : BaseQuery() {}
    ScopedQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}

    uint64_t getResult() const;
};

}
