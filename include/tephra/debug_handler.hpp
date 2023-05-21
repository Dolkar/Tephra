#pragma once

#include <tephra/errors.hpp>
#include <tephra/common.hpp>
#include <string>

namespace tp {

/// Describes the context of where a debug message was triggered. This is useful because Tephra calls do
/// not always correspond 1:1 to Vulkan calls. Vulkan validation messages refer to only the Vulkan function that
/// triggered the message, but this structure provides information about the involved Tephra function as well.
struct DebugMessageContext {
    /// The debug name given to the parent object (Application to Device, Device to Image) or nullptr.
    const char* parentObjectName = nullptr;
    /// The name of the Tephra class type where the message was triggered or nullptr.
    const char* typeName = nullptr;
    /// The debug name given to the specific Tephra object that triggered the message or nullptr.
    const char* objectName = nullptr;
    /// The class method that triggered the message.
    const char* methodName = nullptr;
    /// The name of an important parameter passed to the method or nullptr.
    const char* parameter = nullptr;
};

/// Describes the details of a single debug message.
struct DebugMessage {
    /// The severity of the message.
    DebugMessageSeverity severity;
    /// The type of the message.
    DebugMessageType type;
    /// The context of where the message was triggered.
    DebugMessageContext context;
    /// The actual message string.
    const char* message;
    /// The Vulkan callback data if the message originates from Vulkan layers, otherwise nullptr.
    /// @see @vksymbol{VkDebugUtilsMessengerCallbackDataEXT}
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData;
};

/// Describes the type of a statistic event.
enum class StatisticEventType {
    /// On tp::Device::submitQueuedJobs, reports the number of primary Vulkan command buffers used by the job.
    JobPrimaryCommandBuffersUsed,
    /// On tp::Device::submitQueuedJobs, reports the number of Vulkan pipeline barriers inserted for synchronizing the
    /// commands of the job.
    JobPipelineBarriersInserted,
    /// On tp::Device::submitQueuedJobs, reports the number of Vulkan buffer memory barriers inserted for synchronizing
    /// the buffer accesses of the job.
    JobBufferMemoryBarriersInserted,
    /// On tp::Device::submitQueuedJobs, reports the number of Vulkan image memory barriers inserted for synchronizing
    /// the image accesses of the job.
    JobImageMemoryBarriersInserted,
    /// On tp::Device::enqueueJob, reports the number of bytes requested for job-local buffers for the job.
    JobLocalBufferRequestedBytes,
    /// On tp::Device::enqueueJob, reports the number of bytes actually committed to job-local buffers for the job.
    /// May be lower than tp::StatisticEventType::JobLocalBufferRequestedBytes thanks to resource aliasing and reuse.
    JobLocalBufferCommittedBytes,
    /// On tp::Device::enqueueJob, reports the number of bytes requested for preinitialized job-local buffers for the
    /// job.
    JobPreinitBufferRequestedBytes,
    /// On tp::Device::enqueueJob, reports the number of bytes requested for job-local images for the job.
    JobLocalImageRequestedBytes,
    /// On tp::Device::enqueueJob, reports the number of bytes actually committed to job-local images for the job.
    /// May be lower than tp::StatisticEventType::JobLocalImageRequestedBytes thanks to resource aliasing and reuse.
    JobLocalImageCommittedBytes,
};
TEPHRA_MAKE_CONTIGUOUS_ENUM_VIEW(StatisticEventTypeEnumView, StatisticEventType, JobLocalImageCommittedBytes);

/// Information about the report of a statistic event.
struct StatisticEventInfo {
    /// The type of the event, optionally assigning meaning to the reported counter value.
    StatisticEventType type;
    /// The reported counter value.
    uint64_t counter;
    /// The context of where the event was triggered.
    DebugMessageContext context;
    /// The name of the object the event relates to, or nullptr.
    const char* objectName;
};

/// A base class for debug report handlers, containing callbacks through which debug messages and runtime errors
/// are reported.
class DebugReportHandler {
public:
    /// Will be called on a debug message.
    /// @param message
    ///     The details of the message.
    virtual void callbackMessage(const DebugMessage& message) noexcept {}

    /// Will be called when a Tephra error is about to be thrown.
    /// @param context
    ///     The Tephra context of where the error was triggered.
    /// @param error
    ///     The runtime error thrown.
    virtual void callbackRuntimeError(const DebugMessageContext& context, const RuntimeError& error) noexcept {}

    /// Will be called with statistic event reports gathered during various stages of execution when such callbacks are
    /// enabled through the #TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS preprocessor define.
    /// @param eventInfo
    ///     The information about the event.
    virtual void callbackStatisticEvent(const StatisticEventInfo& eventInfo) {}

    /// Returns the message severity mask. Only messages with these levels will be passed to
    /// tp::DebugReportHandler::callbackMessage.
    virtual DebugMessageSeverityMask getSeverityMask() const noexcept {
        return DebugMessageSeverityMask::None();
    }

    /// Returns the message severity mask. Only messages of these types will be passed to
    /// tp::DebugReportHandler::callbackMessage.
    virtual DebugMessageTypeMask getTypeMask() const noexcept {
        return DebugMessageTypeMask::None();
    }

    virtual ~DebugReportHandler() = default;
};

}
