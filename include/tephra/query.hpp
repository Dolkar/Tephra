#pragma once

#include <tephra/common.hpp>
#include <tephra/semaphore.hpp>

namespace tp {

enum class QueryType {
    Timestamp,
    Render,
};

enum class RenderQueryType {
    // Occlusion queries
    Occlusion,
    // Occlusion queries with PRECISE bit
    // Requires occlusionQueryPrecise feature
    OcclusionPrecise,
    // Pipeline statistics queries
    // Requires pipelineStatisticsQuery feature
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
};

struct QueryResult {
    JobSemaphore jobSemaphore;
    uint64_t value = 0;

    bool isNull() const {
        return jobSemaphore.timestamp == 0;
    }
};

struct QueryEntry;
struct JobSemaphore;
class QueryManager;

class BaseQuery {
    friend class QueryManager;

public:
    using Handle = QueryEntry*;

    bool isNull() const {
        return parentManager == nullptr;
    }

    void setMaxHistorySize(uint32_t size);

    QueryResult getLastResult() const;

    QueryResult getJobResult(const JobSemaphore& jobSemaphore) const;

    TEPHRA_MAKE_NONCOPYABLE(BaseQuery);
    TEPHRA_MAKE_MOVABLE(BaseQuery);

protected:
    BaseQuery() : parentManager(nullptr), handle(nullptr) {}
    BaseQuery(QueryManager* parentManager, Handle handle);
    ~BaseQuery() noexcept;

    QueryManager* parentManager;
    Handle handle;
};

class TimestampQuery : public BaseQuery {
public:
    TimestampQuery() : BaseQuery() {}
    TimestampQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

class RenderQuery : public BaseQuery {
public:
    RenderQuery() : BaseQuery() {}
    RenderQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

}
