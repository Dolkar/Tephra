// Include this first, since it defines the required macros
#include "common_impl.hpp"

#include "debugging.hpp"
#include "debug_reporter.hpp"
#include "device/device_container.hpp"

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    #include <cassert>
#endif

namespace tp {

#if defined(TEPHRA_ENABLE_DEBUG_REPORTING) || defined(TEPHRA_ENABLE_DEBUG_NAMES)
DebugTargetPtr::DebugTargetPtr(DebugTarget debugTarget) : ptr(std::make_unique<DebugTarget>(std::move(debugTarget))) {}

DebugTarget* DebugTargetPtr::get() {
    return ptr.get();
}
DebugTarget* DebugTargetPtr::operator->() {
    return ptr.get();
}
const DebugTarget* DebugTargetPtr::get() const {
    return ptr.get();
}
const DebugTarget* DebugTargetPtr::operator->() const {
    return ptr.get();
}
#else
DebugTargetPtr::DebugTargetPtr(DebugTarget debugTarget) {}

DebugTarget dummyDebugTarget;
DebugTarget* DebugTargetPtr::get() {
    return &dummyDebugTarget;
}
DebugTarget* DebugTargetPtr::operator->() {
    return &dummyDebugTarget;
}
const DebugTarget* DebugTargetPtr::get() const {
    return &dummyDebugTarget;
}
const DebugTarget* DebugTargetPtr::operator->() const {
    return &dummyDebugTarget;
}
#endif

DebugTargetPtr::~DebugTargetPtr() = default;
DebugTargetPtr::DebugTargetPtr(DebugTargetPtr&&) noexcept = default;
DebugTargetPtr& DebugTargetPtr::operator=(DebugTargetPtr&&) noexcept = default;

#ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS
DebugContext* DebugTarget::setDestructorContext() {
    TEPHRA_ASSERT(destructorContext == nullptr);
    if (debugReporter != nullptr) {
        destructorContext = std::make_unique<DebugContext>(this, "destructor", nullptr);
        return destructorContext.get();
    } else {
        return nullptr;
    }
}
#endif

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
const thread_local DebugContext* DebugContext::currentContext = nullptr;

DebugContext::DebugContext(const DebugTarget* debugTarget, const char* methodName, const char* parameter)
    : debugTarget(debugTarget) {
    TEPHRA_ASSERT(debugTarget != nullptr);

    if (debugTarget->isSilent()) {
        this->debugTarget = nullptr;
        previousContext = nullptr;
        return;
    }

    previousContext = currentContext;
    currentContext = this;

    #ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS
    this->methodName = methodName;
    this->parameter = parameter;
    reportMessage(DebugMessageSeverity::Verbose, DebugMessageType::General, "Entered context", nullptr);
    #endif
}

void DebugContext::reportMessage(
    DebugMessageSeverity severity,
    DebugMessageType type,
    const char* message,
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData) const noexcept {
    TEPHRA_ASSERT_NOEXCEPT(debugTarget != nullptr);

    DebugMessage debugMessage;
    debugMessage.severity = severity;
    debugMessage.type = type;
    debugMessage.context = formMessageContext();
    debugMessage.message = message;
    debugMessage.vkCallbackData = vkCallbackData;

    debugTarget->debugReporter->reportMessage(debugMessage);
}

void DebugContext::reportRuntimeError(const RuntimeError& error) const noexcept {
    TEPHRA_ASSERT_NOEXCEPT(debugTarget != nullptr);
    debugTarget->debugReporter->reportRuntimeError(formMessageContext(), error);
}

void DebugContext::reportStatisticEvent(StatisticEventType type, uint64_t counter, const char* objectName) const {
    TEPHRA_ASSERT_NOEXCEPT(debugTarget != nullptr);

    StatisticEventInfo eventInfo;
    eventInfo.type = type;
    eventInfo.counter = counter;
    eventInfo.context = formMessageContext();
    eventInfo.objectName = objectName;

    debugTarget->debugReporter->reportStatisticEvent(eventInfo);
}

    #ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS

DebugContext::~DebugContext() {
    if (debugTarget == nullptr)
        return;

    reportMessage(DebugMessageSeverity::Verbose, DebugMessageType::General, "Left context", nullptr);

    // Ensure we didn't leak scratch space
    if (previousContext == nullptr)
        TEPHRA_ASSERT(ScratchAllocatorState::get()->isEmpty());

    currentContext = previousContext;
}

DebugMessageContext DebugContext::formMessageContext() const noexcept {
    DebugMessageContext msgContext;
    msgContext.parentObjectName = debugTarget->getParentTarget() != nullptr ?
        debugTarget->getParentTarget()->getObjectName() :
        nullptr;
    msgContext.typeName = debugTarget->typeName;
    msgContext.objectName = debugTarget->getObjectName();
    msgContext.methodName = methodName;
    msgContext.parameter = parameter;

    return msgContext;
}
    #else
DebugContext::DebugContext(DebugContext&& other) noexcept
    : debugTarget(other.debugTarget), previousContext(other.previousContext) {
    other.debugTarget = nullptr;
};

DebugContext& DebugContext::operator=(DebugContext&& other) noexcept {
    debugTarget = other.debugTarget;
    previousContext = other.previousContext;
    other.debugTarget = nullptr;
}

DebugContext::~DebugContext() {
    if (debugTarget != nullptr)
        currentContext = previousContext;
}

DebugMessageContext DebugContext::formMessageContext() const noexcept {
    return DebugMessageContext();
}
    #endif
#endif

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
void reportDebugMessageRaw(
    DebugMessageSeverity severity,
    DebugMessageType type,
    const char* message,
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData) noexcept {
    auto context = DebugContext::getCurrentContext();
    if (context != nullptr) {
        context->reportMessage(severity, type, message, vkCallbackData);
    } else if (severity == DebugMessageSeverity::Error) {
        // Context is missing and we can't throw exceptions here, use C assert as last resort
        assert(("Error encountered outside of debug context.", 0));
    }
}
#endif

#ifdef TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
void reportStatisticEvent(StatisticEventType type, uint64_t counter, const char* objectName) {
    auto context = DebugContext::getCurrentContext();
    if (context != nullptr) {
        context->reportStatisticEvent(type, counter, objectName);
    }
}
#endif

}
