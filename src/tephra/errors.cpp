#include <tephra/errors.hpp>
#include "common_impl.hpp"

namespace tp {

std::string RuntimeError::getErrorTypeDescription(ErrorType type) {
    const std::size_t msgBufferSize = 64;
    char msgBuffer[msgBufferSize];

    switch (type) {
    case ErrorType::AssertionFailed:
        return "Internal Tephra assertion failed";
    case ErrorType::UnsupportedOperation:
        return "Platform or device does not support this operation";
    case ErrorType::OutOfHostMemory:
        return "VK_ERROR_OUT_OF_HOST_MEMORY: A host memory allocation has failed";
    case ErrorType::OutOfDeviceMemory:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY: A device memory allocation has failed";
    case ErrorType::Fragmentation:
        return "VK_ERROR_FRAGMENTATION: A descriptor pool creation has failed due to fragmentation";
    case ErrorType::InitializationFailed:
        return "VK_ERROR_INITIALIZATION_FAILED: Initialization of an object could not be completed";
    case ErrorType::DeviceLost:
        return "VK_ERROR_DEVICE_LOST: The logical or physical device has been lost";
    case ErrorType::MemoryMapFailed:
        return "VK_ERROR_MEMORY_MAP_FAILED: Mapping of a memory object has failed";
    case ErrorType::LayerNotPresent:
        return "VK_ERROR_LAYER_NOT_PRESENT: A requested layer is not present or could not be loaded";
    case ErrorType::ExtensionNotPresent:
        return "VK_ERROR_EXTENSION_NOT_PRESENT: A requested extension is not supported";
    case ErrorType::FeatureNotPresent:
        return "VK_ERROR_FEATURE_NOT_PRESENT: A requested feature is not supported";
    case ErrorType::TooManyObjects:
        return "VK_ERROR_TOO_MANY_OBJECTS: Too many objects of a type have already been created";
    case ErrorType::FormatNotSupported:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED: A requested format is not supported on this device";
    case ErrorType::InvalidExternalHandle:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE: An external handle is not a valid handle of the specified type";
    case ErrorType::SurfaceLostKHR:
        return "VK_ERROR_SURFACE_LOST_KHR: A surface is no longer available";
    case ErrorType::NativeWindowInUseKHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: The requested window is already connected to a VkSurfaceKHR, "
               "or to some other non-Vulkan API";
    case ErrorType::OutOfDateKHR:
        return "VK_ERROR_OUT_OF_DATE_KHR: A surface has changed in such a way that it is no longer compatible "
               "with the swapchain, and further presentation requests using the swapchain will fail";
    default:
        TEPHRA_ASSERT(static_cast<int32_t>(type) < 0);
        snprintf(msgBuffer, msgBufferSize, "Unknown error code: %d", static_cast<int32_t>(type));
        return std::string(msgBuffer);
    }
}

}
