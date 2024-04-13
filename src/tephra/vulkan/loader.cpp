#include "loader.hpp"
#include "../common_impl.hpp"

#if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
    #define TEPHRA_OS_WINDOWS
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
    #define TEPHRA_OS_UNIX
#endif

#if !defined(TEPHRA_OS_WINDOWS) && !defined(TEPHRA_OS_UNIX)
    #define TEPHRA_OS_UNSUPPORTED
#endif

#if defined(TEPHRA_OS_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #define VULKAN_LOADER_LIBRARY_NAME TEXT("vulkan-1.dll")
#elif defined(TEPHRA_OS_UNIX)
    #include <dlfcn.h>
    #define VULKAN_LOADER_LIBRARY_NAME "libvulkan.so.1"
#endif

namespace tp {

std::string getLastError() {
#ifdef TEPHRA_OS_WINDOWS
    LPVOID msgBuffer;
    DWORD errorCode = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&msgBuffer),
        0,
        NULL);
    std::string errorMessage;
    if (msgBuffer) {
    #ifdef _UNICODE
        // Convert UTF-16 to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, static_cast<LPTSTR>(msgBuffer), -1, nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8data(len);
        WideCharToMultiByte(CP_UTF8, 0, static_cast<LPCTSTR>(msgBuffer), -1, utf8data.data(), len, nullptr, nullptr);

        // Make sure result is always null terminated
        utf8data[len - 1] = 0;

        errorMessage = utf8data.data();
    #else
        errorMessage = static_cast<LPCTSTR>(msgBuffer);
    #endif
    } else {
        errorMessage = "Unknown error.";
    }

    LocalFree(msgBuffer);
    return errorMessage;
#elif defined(TEPHRA_OS_UNIX)
    const char* msgBuffer = dlerror();
    return msgBuffer ? std::string(msgBuffer) : "Unknown error.";
#endif
}

VulkanLoader::VulkanLoader() {
#if defined(TEPHRA_OS_WINDOWS)
    libHandle = LoadLibrary(VULKAN_LOADER_LIBRARY_NAME);
#elif defined(TEPHRA_OS_UNIX)
    libHandle = dlopen(VULKAN_LOADER_LIBRARY_NAME, RTLD_NOW | RTLD_LOCAL);
#elif TEPHRA_OS_UNSUPPORTED
    throwRuntimeError(RuntimeError(ErrorType::InitializationFailed, "Unsupported operating system."));
#endif
    if (libHandle == nullptr) {
        std::string errorMessage = "Failed to load loader library - " + getLastError();
        throwRuntimeError(RuntimeError(ErrorType::InitializationFailed, errorMessage));
    }
}

PFN_vkVoidFunction VulkanLoader::loadExportedProcedure(const char* procName) const {
#if defined(TEPHRA_OS_WINDOWS)
    void* procPtr = GetProcAddress(static_cast<HMODULE>(libHandle), procName);
#elif defined(TEPHRA_OS_UNIX)
    void* procPtr = dlsym(libHandle, procName);
#endif
    return reinterpret_cast<PFN_vkVoidFunction>(procPtr);
}

VulkanLoader::~VulkanLoader() {
#if defined(TEPHRA_OS_WINDOWS)
    FreeLibrary(static_cast<HMODULE>(libHandle));
#elif defined(TEPHRA_OS_UNIX)
    dlclose(libHandle);
#endif
    libHandle = nullptr;
}

}
