#pragma once

#include "debugging.hpp"

namespace tp {

[[noreturn]] inline void throwRuntimeError(const RuntimeError& error) {
#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    auto context = DebugContext::getCurrentContext();
    if (context != nullptr) {
        context->reportRuntimeError(error);
    }
#endif

    error.raise();

#ifdef __GNUC__
    __builtin_unreachable();
#endif
}

[[noreturn]] inline void throwErrorFromType(ErrorType type, const char* message = nullptr) {
    switch (type) {
    case ErrorType::AssertionFailed:
        throwRuntimeError(AssertionError(message));
        break;
    case ErrorType::DeviceLost:
        throwRuntimeError(DeviceLostError(message));
        break;
    case ErrorType::SurfaceLostKHR:
        throwRuntimeError(SurfaceLostError(message));
        break;
    case ErrorType::OutOfDateKHR:
        throwRuntimeError(OutOfDateError(message));
        break;
    case ErrorType::LayerNotPresent:
    case ErrorType::ExtensionNotPresent:
    case ErrorType::FeatureNotPresent:
    case ErrorType::FormatNotSupported:
    case ErrorType::InvalidExternalHandle:
        throwRuntimeError(UnsupportedOperationError(type, message));
        break;
    case ErrorType::OutOfHostMemory:
    case ErrorType::OutOfDeviceMemory:
    case ErrorType::Fragmentation:
        throwRuntimeError(OutOfMemoryError(type, message));
        break;
    case ErrorType::TooManyObjects:
        throwRuntimeError(TooManyObjectsError(message));
        break;
    default:
        throwRuntimeError(RuntimeError(type, message));
        break;
    }
}

inline VkResult throwRetcodeErrors(VkResult retCode) {
    if (retCode < 0) {
        throwErrorFromType(static_cast<ErrorType>(retCode));
    }
    return retCode;
};

inline bool throwAssertError(const char* expr, const char* desc, const char* file, int line) {
    const std::size_t bufSize = 1024;
    thread_local char buf[bufSize];
    snprintf(buf, bufSize, "%s (%s:%d %s)", desc, file, line, expr);
    throwRuntimeError(AssertionError(buf));
    return true;
}

inline bool reportAssertError(const char* expr, const char* desc, const char* file, int line) noexcept {
#ifdef TEPHRA_ENABLE_DEBUG_REPORTING
    const std::size_t bufSize = 1024;
    thread_local char buf[bufSize];
    snprintf(buf, bufSize, "%s (%s:%d %s)", desc, file, line, expr);

    auto context = DebugContext::getCurrentContext();
    if (context != nullptr) {
        context->reportRuntimeError(AssertionError(buf));
    }
#endif
    return true;
}

#ifdef TEPHRA_ENABLE_DEBUG_ASSERTS
    #define TEPHRA_ASSERTD(expr, desc) ((void)(!(expr) && tp::throwAssertError(#expr, desc, __FILE__, __LINE__)))
    #define TEPHRA_ASSERTD_NOEXCEPT(expr, desc) \
        ((void)(!(expr) && tp::reportAssertError(#expr, desc, __FILE__, __LINE__)))
#else
    #define TEPHRA_ASSERTD(expr, desc) ((void)sizeof(expr))
    #define TEPHRA_ASSERTD_NOEXCEPT(expr, desc) ((void)sizeof(expr))
#endif
#define TEPHRA_ASSERT(expr) TEPHRA_ASSERTD(expr, "")
#define TEPHRA_ASSERT_NOEXCEPT(expr) TEPHRA_ASSERTD_NOEXCEPT(expr, "")

}
