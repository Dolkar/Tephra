#pragma once

#ifdef VK_USE_PLATFORM_WIN32_KHR
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#undef VK_NO_PROTOTYPES

#ifdef VK_USE_PLATFORM_XLIB_KHR
    #ifdef None
        #undef None
    #endif
    #ifdef Always
        #undef Always
    #endif
#endif

#ifndef VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    #define VK_KHR_XLIB_SURFACE_EXTENSION_NAME "VK_KHR_xlib_surface"
#endif
#ifndef VK_KHR_XCB_SURFACE_EXTENSION_NAME
    #define VK_KHR_XCB_SURFACE_EXTENSION_NAME "VK_KHR_xcb_surface"
#endif
#ifndef VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
    #define VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME "VK_KHR_wayland_surface"
#endif
#ifndef VK_KHR_MIR_SURFACE_EXTENSION_NAME
    #define VK_KHR_MIR_SURFACE_EXTENSION_NAME "VK_KHR_mir_surface"
#endif
#ifndef VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    #define VK_KHR_ANDROID_SURFACE_EXTENSION_NAME "VK_KHR_android_surface"
#endif
#ifndef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vma/vk_mem_alloc.h"
