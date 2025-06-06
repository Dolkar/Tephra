#pragma once

#include <tephra/common.hpp>
#include <tephra/semaphore.hpp>

namespace tp {

/// Specifies the type of the measured quantity by a tp::RenderQuery.
/// @see @vksymbol{VkQueryType}
/// @see @vksymbol{VkQueryPipelineStatisticFlagBits}
/// @remarks
///     All pipeline statistics queries require the @vksymbol{VkPhysicalDeviceVulkanFeatures}::`pipelineStatisticsQuery`
///     feature to be enabled.
enum class RenderQueryType {
    /// Measures the number of samples that pass the per-fragment tests. The result can be approximate for values
    /// greater than 0.
    Occlusion,
    /// Measures the number of samples that pass the per-fragment tests.
    /// The use of this type requires the @vksymbol{VkPhysicalDeviceVulkanFeatures}::`occlusionQueryPrecise` feature to
    /// be enabled.
    OcclusionPrecise,
    /// Pipeline statistics query counting the number of vertices processed by the input assembly stage.
    InputAssemblyVertices,
    /// Pipeline statistics query counting the number of primitives processed by the input assembly stage.
    InputAssemblyPrimitives,
    /// Pipeline statistics query counting the number of vertex shader invocations.
    VertexShaderInvocations,
    /// Pipeline statistics query counting number of geometry shader invocations.
    GeometryShaderInvocations,
    /// Pipeline statistics query counting the number of primitives generated by geometry shader invocations.
    GeometryShaderPrimitives,
    /// Pipeline statistics query counting the number of primitives processed by the "primitive clipping" stage of the
    /// pipeline.
    ClippingInvocations,
    /// Pipeline statistics query counting the number of primitives output by the "primitive clipping" stage of the
    /// pipeline.
    ClippingPrimitives,
    /// Pipeline statistics query counting the number of fragment shader invocations.
    FragmentShaderInvocations,
    /// Pipeline statistics query counting the number of patches processed by the tessellation control shader.
    TessellationControlShaderPatches,
    /// Pipeline statistics query counting the number of invocations of the tessellation evaluation shader.
    TessellationEvaluationShaderInvocations,
};

/// Represents the result of a query that was written as part of a completed job.
/// @see tp::BaseQuery::getLastResult
/// @see tp::BaseQuery::getJobResult
struct QueryResult {
    /// The semaphore signalled by the job during which this result value was written.
    JobSemaphore jobSemaphore;
    /// The value of the result.
    uint64_t value = 0;

    /// Returns `true` if the result is null and not valid.
    bool isNull() const {
        return jobSemaphore.timestamp == 0;
    }
};

struct QueryRecord;
class QueryManager;

/// Base class implementing the shared functionality of device queries.
class BaseQuery {
    friend class QueryRecorder;

public:
    /// Returns `true` if the query is null and not valid for use.
    bool isNull() const {
        return parentManager == nullptr;
    }

    /// Returns the last (largest timestamp) result written for this query, or a null result if none are available.
    QueryResult getLastResult() const;

    /// Returns the result written during the execution of the given job, or a null result if not available.
    /// By default, any result older than the last result may be forgotten at any point. Use
    /// tp::BaseQuery::setMaxHistorySize to set the number of past results that should be stored and retrievable through
    /// this method.
    /// @param jobSemaphore
    ///     The semaphore that identifies the job for which a result should be retrieved.
    QueryResult getJobResult(const JobSemaphore& jobSemaphore) const;

    /// Sets the maximum number of past stored results that can be retrieved with tp::BaseQuery::getJobResult.
    /// @remarks
    ///     This function may not be called while the query is in use (pending a result).
    void setMaxHistorySize(uint32_t size);

    /// Throws away all previous results.
    /// @remarks
    ///     This function may not be called while the query is in use (pending a result).
    void clear();

    TEPHRA_MAKE_NONCOPYABLE(BaseQuery);
    TEPHRA_MAKE_MOVABLE(BaseQuery);

protected:
    using Handle = QueryRecord*;

    BaseQuery() : parentManager(nullptr), handle(nullptr) {}
    BaseQuery(QueryManager* parentManager, Handle handle);
    ~BaseQuery() noexcept;

    QueryManager* parentManager;
    Handle handle;
};

/// Query for measuring execution time on the device timeline.
/// @remarks
///     The returned value is the number of "ticks" that has passed since some undefined point in the past. Two
///     timestamp query values can be subtracted from each other and converted to a duration in nanoseconds by
///     multiplying them with
///     @see @vksymbol{VkPhysicalDeviceLimits}::`timestampPeriod`.
/// @remarks
///     By default, it is only valid to compare or subtract timstamp query results that were written as part of the same
///     queue submission. Only if the @see @vksymbol{VK_KHR_calibrated_timestamps} extension is enabled, the timestamps
///     become comparable across submissions and queues. The extension also allows comparing the values against
///     system clock.
/// @see tp::Device::createTimestampQueries
/// @see tp::Job::cmdWriteTimestamp
/// @see tp::RenderList::cmdWriteTimestamp
/// @see tp::ComputeList::cmdWriteTimestamp
class TimestampQuery : public BaseQuery {
public:
    /// Creates a null timestamp query
    TimestampQuery() : BaseQuery() {}

    TimestampQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

/// Query for measuring some statistic over a range of render commands.
/// @remarks
///     The measured range must be fully contained inside a single tp::RenderList.
/// @see tp::Device::createRenderQueries
/// @see tp::RenderList::cmdBeginQueries
/// @see tp::RenderList::cmdEndQueries
class RenderQuery : public BaseQuery {
public:
    /// Creates a null render query
    RenderQuery() : BaseQuery() {}

    RenderQuery(QueryManager* parentManager, Handle handle) : BaseQuery(parentManager, handle) {}
};

}
