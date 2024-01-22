#pragma once

#include <tephra/common.hpp>
#include <stdexcept>

namespace tp {

/// The type of an error returned by either Vulkan or Tephra
/// @see @vksymbol{VkResult}
enum class ErrorType : int32_t {
    /// Unknown runtime error type. Maps to tp::RuntimeError.
    UnknownError = 0,
    /// Internal Tephra assertion failed. Maps to tp::AssertionError.
    AssertionFailed = 1,
    /// Platform or device does not support this operation. Maps to tp::UnsupportedOperationError.
    UnsupportedOperation = 2,
    /// A host memory allocation has failed. Maps to tp::OutOfMemoryError.
    OutOfHostMemory = VK_ERROR_OUT_OF_HOST_MEMORY,
    /// A device memory allocation has failed. Maps to tp::OutOfMemoryError.
    OutOfDeviceMemory = VK_ERROR_OUT_OF_DEVICE_MEMORY,
    /// A descriptor pool creation has failed due to fragmentation. Maps to tp::OutOfMemoryError.
    Fragmentation = VK_ERROR_FRAGMENTATION,
    /// Initialization of an object could not be completed. Maps to tp::RuntimeError.
    InitializationFailed = VK_ERROR_INITIALIZATION_FAILED,
    /// The logical or physical device has been lost. Maps to tp::DeviceLostError.
    DeviceLost = VK_ERROR_DEVICE_LOST,
    /// Mapping of a memory object has failed. Maps to tp::RuntimeError.
    MemoryMapFailed = VK_ERROR_MEMORY_MAP_FAILED,
    /// A requested layer is not present or could not be loaded. Maps to tp::UnsupportedOperationError.
    LayerNotPresent = VK_ERROR_LAYER_NOT_PRESENT,
    /// A requested extension is not supported. Maps to tp::UnsupportedOperationError.
    ExtensionNotPresent = VK_ERROR_EXTENSION_NOT_PRESENT,
    /// A requested feature is not supported. Maps to tp::UnsupportedOperationError.
    FeatureNotPresent = VK_ERROR_FEATURE_NOT_PRESENT,
    /// Too many objects of a type have already been created. Maps to tp::TooManyObjectsError.
    TooManyObjects = VK_ERROR_TOO_MANY_OBJECTS,
    /// A requested format is not supported on this device. Maps to tp::UnsupportedOperationError.
    FormatNotSupported = VK_ERROR_FORMAT_NOT_SUPPORTED,
    /// An external handle is not a valid handle of the specified type. Maps to tp::UnsupportedOperationError.
    InvalidExternalHandle = VK_ERROR_INVALID_EXTERNAL_HANDLE,
    /// A surface is no longer available. Maps to tp::SurfaceLostError.
    SurfaceLostKHR = VK_ERROR_SURFACE_LOST_KHR,
    /// The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API.
    /// Maps to tp::RuntimeError.
    NativeWindowInUseKHR = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
    /// A surface has changed in such a way that it is no longer compatible with the swapchain, and further
    /// presentation requests using the swapchain will fail. Maps to tp::OutOfDateError.
    OutOfDateKHR = VK_ERROR_OUT_OF_DATE_KHR,

    // Vulkan errors that should not propagate to Tephra user:
    // VK_ERROR_FRAGMENTED_POOL, VK_ERROR_OUT_OF_POOL_MEMORY,
    // Vulkan errors from unsupported features:
    // VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, VK_PIPELINE_COMPILE_REQUIRED
};

/// A generic Tephra runtime error.
class RuntimeError : public std::runtime_error {
public:
    RuntimeError(ErrorType type, const std::string& what) : runtime_error(what), type(type) {}
    RuntimeError(ErrorType type, const char* what) : runtime_error(what == nullptr ? "" : what), type(type) {}

    /// Returns the type of the error.
    ErrorType getErrorType() const noexcept {
        return type;
    }

    /// Throws this error as an exception.
    [[noreturn]] virtual void raise() const {
        throw *this;
    }

    /// Formats the error type name and description as a string.
    static std::string getErrorTypeDescription(ErrorType type);

protected:
    ErrorType type;
};

/// Internal Tephra error violating an assertion. May be caused by a previous invalid usage.
class AssertionError : public RuntimeError {
public:
    explicit AssertionError(const std::string& what) : RuntimeError(ErrorType::AssertionFailed, what) {}
    explicit AssertionError(const char* what) : RuntimeError(ErrorType::AssertionFailed, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// Operation failed because the device was lost. Subsequent device operations will fail, too.
class DeviceLostError : public RuntimeError {
public:
    explicit DeviceLostError(const std::string& what) : RuntimeError(ErrorType::DeviceLost, what) {}
    explicit DeviceLostError(const char* what) : RuntimeError(ErrorType::DeviceLost, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// Operation failed because the surface was lost. Subsequent surface operations will fail, too.
class SurfaceLostError : public RuntimeError {
public:
    explicit SurfaceLostError(const std::string& what) : RuntimeError(ErrorType::SurfaceLostKHR, what) {}
    explicit SurfaceLostError(const char* what) : RuntimeError(ErrorType::SurfaceLostKHR, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// The operation could not be completed because it is not supported by the platform or device.
class UnsupportedOperationError : public RuntimeError {
public:
    explicit UnsupportedOperationError(const std::string& what) : RuntimeError(ErrorType::UnsupportedOperation, what) {}
    explicit UnsupportedOperationError(const char* what) : RuntimeError(ErrorType::UnsupportedOperation, what) {}

    UnsupportedOperationError(ErrorType type, const std::string& what) : RuntimeError(type, what) {}
    UnsupportedOperationError(ErrorType type, const char* what) : RuntimeError(type, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// Allocation failed due to overcommitting of memory.
class OutOfMemoryError : public RuntimeError {
public:
    OutOfMemoryError(ErrorType type, const std::string& what) : RuntimeError(type, what) {}
    OutOfMemoryError(ErrorType type, const char* what) : RuntimeError(type, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// Too many objects of the same type have already been created.
class TooManyObjectsError : public RuntimeError {
public:
    explicit TooManyObjectsError(const std::string& what) : RuntimeError(ErrorType::TooManyObjects, what) {}
    explicit TooManyObjectsError(const char* what) : RuntimeError(ErrorType::TooManyObjects, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

/// A surface has changed in such a way that it is no longer compatible with the swapchain,
/// and further presentation requests using the swapchain will fail.
class OutOfDateError : public RuntimeError {
public:
    explicit OutOfDateError(const std::string& what) : RuntimeError(ErrorType::OutOfDateKHR, what) {}
    explicit OutOfDateError(const char* what) : RuntimeError(ErrorType::OutOfDateKHR, what) {}

    [[noreturn]] virtual void raise() const override {
        throw *this;
    }
};

}
