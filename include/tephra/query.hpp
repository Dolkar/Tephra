#pragma once

#include <tephra/common.hpp>

namespace tp {

enum class QueryType {
    Timestamp,
    Duration,
    // Occlusion queries
    VisibleFragmentSamples,
    // Occlusion queries with PRECISE bit
    VisibleFragmentSamplesPrecise,
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
    ComputeShaderInvocations
};

struct QueryResult {
    uint64_t value = 0;
    uint64_t jobSemaphoreTimestamp = 0;

    bool isNull() const {
        return jobSemaphoreTimestamp == 0;
    }
};

class QueryManager;

class Query {
public:
    using Handle = void*;

    Query() : parentManager(nullptr), handle(nullptr) {}

    Query(QueryManager* parentManager, Handle handle) : parentManager(parentManager), handle(handle) {}

    bool isNull() const {
        return parentManager == nullptr;
    }

    QueryType getType() const;

    QueryResult getResult() const;

    TEPHRA_MAKE_NONCOPYABLE(Query);
    TEPHRA_MAKE_MOVABLE(Query);
    ~Query() noexcept;

private:
    QueryManager* parentManager;
    Handle handle;
};

}
