#pragma once

#include <tephra/debug_handler.hpp>
#include <tephra/errors.hpp>
#include <memory>

#ifdef TEPHRA_ENABLE_DEBUG
    // Enable reporting of Tephra debug messages to the debug handler
    #define TEPHRA_ENABLE_DEBUG_REPORTING
    // Enable Tephra validation
    #define TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION
    // Enable internal asserts
    #define TEPHRA_ENABLE_DEBUG_ASSERTS
    // Enable the management and reporting of contexts for debug messages
    #define TEPHRA_ENABLE_DEBUG_CONTEXTS
    // Enable debug name storage
    #define TEPHRA_ENABLE_DEBUG_NAMES
    // Enable reporting statistic events
    #define TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
#endif

// Also enable reporting if any of the options that depend on it are enabled
#if defined(TEPHRA_ENABLE_DEBUG_ASSERTS) || defined(TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION) || \
    defined(TEPHRA_ENABLE_DEBUG_CONTEXTS)
    #define TEPHRA_ENABLE_DEBUG_REPORTING
#endif

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    #define TEPHRA_ENABLE_DEBUG_NAMES
    #include <sstream>
#endif

#ifdef TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
constexpr bool StatisticEventsEnabled = true;
#else
constexpr bool StatisticEventsEnabled = false;
#endif

#ifdef TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION
constexpr bool TephraValidationEnabled = true;
#else
constexpr bool TephraValidationEnabled = false;
#endif

namespace tp {

class DebugReporter;

// Exists during the lifetime of Tephra objects, storing data needed for the reporting of
// debug information and debug contexts.
// If TEPHRA_ENABLE_DEBUG_REPORTING is defined, it simply stores a pointer to the debug
// reporter, so that messages can be reported to the handler provided by the user.
// If TEPHRA_ENABLE_DEBUG_CONTEXTS is defined, it additionally stores identifying information
// about the object's name and type to be used in debug message contexts.
class DebugTarget;

// Exists during the lifetime of one scope, be it a function call or destruction of an object.
// It provides an interface for reporting messages and errors that happen within the scope
// to the handler defined by the user, through a DebugTarget present in the containing object.
class DebugContext;

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
class DebugContext {
public:
    DebugContext(const DebugTarget* debugTarget, const char* methodName, const char* parameter);

    void reportMessage(
        DebugMessageSeverity severity,
        DebugMessageType type,
        const char* message,
        const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData) const noexcept;

    void reportRuntimeError(const RuntimeError& error) const noexcept;

    void reportStatisticEvent(StatisticEventType type, uint64_t counter, const char* objectName) const;

    TEPHRA_MAKE_NONCOPYABLE(DebugContext);
    TEPHRA_MAKE_NONMOVABLE(DebugContext);
    ~DebugContext();

    static const DebugContext* getCurrentContext() {
        return currentContext;
    }

private:
    static thread_local const DebugContext* currentContext;

    const DebugTarget* debugTarget;
    const DebugContext* previousContext;
    #ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS
    const char* methodName;
    const char* parameter;
    #endif

    DebugMessageContext formMessageContext() const noexcept;
};
#else
class DebugContext {
public:
    static const DebugContext* getCurrentContext() {
        return nullptr;
    }
};
#endif

#ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS
class DebugTarget {
public:
    DebugTarget(const DebugTarget* parentTarget, const char* typeName, const char* objectName)
        : parentTarget(parentTarget),
          typeName(typeName),
          objectName(objectName ? objectName : std::string()),
          debugReporter(parentTarget->debugReporter) {}
    DebugTarget(const DebugReporter* debugReporter, const char* typeName, const char* objectName)
        : parentTarget(nullptr),
          typeName(typeName),
          objectName(objectName ? objectName : std::string()),
          debugReporter(debugReporter) {}

    const DebugTarget* getParentTarget() const {
        return parentTarget;
    }

    const char* getObjectName() const {
        return objectName.empty() ? nullptr : objectName.c_str();
    }

    const char* getTypeName() const {
        return typeName;
    }

    const bool isSilent() const {
        return debugReporter == nullptr;
    }

    DebugContext* setDestructorContext();

    TEPHRA_MAKE_MOVABLE_DEFAULT(DebugTarget);
    DebugTarget(const DebugTarget& other) noexcept;
    DebugTarget& operator=(const DebugTarget& other) noexcept;

    // Silent DebugTarget won't invoke contexts. Used for internal use of interface classes
    static DebugTarget makeSilent() {
        return DebugTarget(static_cast<DebugReporter*>(nullptr), nullptr, nullptr);
    }

private:
    friend class DebugContext;

    const DebugReporter* debugReporter;
    const DebugTarget* parentTarget;
    const char* typeName;
    std::string objectName;

    // This context wraps around the destruction of the containing object
    std::unique_ptr<DebugContext> destructorContext;
};

#else
class DebugTarget {
public:
    DebugTarget() {}
    DebugTarget(const DebugTarget* parentTarget, const char* typeName, const char* objectName)
    #ifdef TEPHRA_ENABLE_DEBUG_NAMES
        : objectName(objectName ? objectName : std::string())
    #endif
    #ifdef TEPHRA_ENABLE_DEBUG_REPORTING
          ,
          debugReporter(parentTarget->debugReporter)
    #endif
    {
    }
    DebugTarget(const DebugReporter* debugReporter, const char* typeName, const char* objectName)
    #ifdef TEPHRA_ENABLE_DEBUG_NAMES
        : objectName(objectName ? objectName : std::string())
    #endif
    #ifdef TEPHRA_ENABLE_DEBUG_REPORTING
          ,
          debugReporter(debugReporter)
    #endif
    {
    }

    const DebugTarget* getParentTarget() const {
        return nullptr;
    }

    const char* getObjectName() const {
    #ifdef TEPHRA_ENABLE_DEBUG_NAMES
        return objectName.empty() ? nullptr : objectName.c_str();
    #else
        return nullptr;
    #endif
    }

    const char* getTypeName() const {
        return nullptr;
    }

    const bool isSilent() const {
    #ifdef TEPHRA_ENABLE_DEBUG_REPORTING
        return debugReporter == nullptr;
    #else
        return true;
    #endif
    }

    DebugContext* setDestructorContext() {
        return nullptr;
    }

    // Silent DebugTarget won't invoke contexts. Used for internal use of interface classes
    static DebugTarget makeSilent() {
        return DebugTarget(static_cast<DebugReporter*>(nullptr), nullptr, nullptr);
    }

private:
    friend class DebugContext;

    #ifdef TEPHRA_ENABLE_DEBUG_NAMES
    std::string objectName;
    #endif
    #ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    const DebugReporter* debugReporter;
    #endif
};

#endif

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    #define TEPHRA_DEBUG_SET_CONTEXT(debugTarget, methodName, parameter) \
        DebugContext debugContext((debugTarget), (methodName), (parameter))
    #define TEPHRA_DEBUG_SET_CONTEXT_TEMP(parentDebugTarget, typeName, methodName, parameter) \
        DebugTarget tempDebugTarget((parentDebugTarget), (typeName), nullptr); \
        DebugContext debugContext(&tempDebugTarget, (methodName), (parameter))
    #define TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(debugTarget) (debugTarget)->setDestructorContext()
#else
    #define TEPHRA_DEBUG_SET_CONTEXT(debugTarget, methodName, parameter)
    #define TEPHRA_DEBUG_SET_CONTEXT_TEMP(parentDebugTarget, typeName, methodName, parameter)
    #define TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(debugTarget)
#endif

#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
void reportDebugMessageRaw(
    DebugMessageSeverity severity,
    DebugMessageType type,
    const char* message,
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData) noexcept;

template <typename... TArgs>
void reportDebugMessage(DebugMessageSeverity severity, DebugMessageType type, const TArgs&... args) noexcept {
    std::ostringstream stream;
    (stream << ... << args);
    reportDebugMessageRaw(severity, type, stream.str().c_str(), nullptr);
}

template <>
inline void reportDebugMessage<const char*>(
    DebugMessageSeverity severity,
    DebugMessageType type,
    const char* const& message) noexcept {
    reportDebugMessageRaw(severity, type, message, nullptr);
}

#else
inline void reportDebugMessageRaw(
    DebugMessageSeverity severity,
    DebugMessageType type,
    const char* message,
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData) noexcept {}

template <typename... TArgs>
void reportDebugMessage(DebugMessageSeverity severity, DebugMessageType type, const TArgs&... args) noexcept {}
#endif

#ifdef TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
void reportStatisticEvent(StatisticEventType type, uint64_t counter, const char* objectName = nullptr);
#else
inline void reportStatisticEvent(StatisticEventType type, uint64_t counter, const char* objectName = nullptr) {}
#endif

}
