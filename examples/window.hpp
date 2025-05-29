#pragma once

// Based on the Vulkan cube.cpp demo that comes with the Vulkan SDK.
// The platform dependent windowing and surface creation functionality has been extracted here.
//
// Original license statement as included with cube.cpp:
/*
 * Copyright (c) 2015-2019 The Khronos Group Inc.
 * Copyright (c) 2015-2019 Valve Corporation
 * Copyright (c) 2015-2019 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jeremy Hayes <jeremy@lunarg.com>
 */

#include "examples_common.hpp"

#include <cinttypes>
#include <vulkan/vulkan.h>

#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
    #include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    #include <linux/input.h>
#endif

class WindowManager {
public:
    WindowManager();

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    void init(HINSTANCE hInstance, Example* example);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    void init(void* caMetalLayer, Example* example);
#else
    void init(Example* example);
#endif

    void run(uint32_t frameCount = UINT32_MAX);

    void cleanup();

private:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HINSTANCE connection; // hInstance - Windows Instance
    HWND window; // hWnd - window handle
    POINT minsize; // minimum window size
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    Window xlib_window;
    Atom xlib_wm_delete_window;
    Display* display;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_window_t xcb_window;
    xcb_screen_t* screen;
    xcb_connection_t* connection;
    xcb_intern_atom_reply_t* atom_wm_delete_window;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    wl_display* display;
    wl_registry* registry;
    wl_compositor* compositor;
    wl_surface* window;
    wl_shell* shell;
    wl_shell_surface* shell_surface;
    wl_seat* seat;
    wl_pointer* pointer;
    wl_keyboard* keyboard;
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    IDirectFB* dfb;
    IDirectFBSurface* window;
    IDirectFBEventBuffer* event_buffer;
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    void* caMetalLayer;
#endif

    Example* example;
    VkSurfaceKHR surface;
    uint32_t width;
    uint32_t height;
    bool has_resized;
    bool pause;
    bool quit;

    void create_window();

    void create_surface();

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    friend LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    void handle_xlib_event(const XEvent*);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    void handle_xcb_event(const xcb_generic_event_t*);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    void handle_directfb_event(const DFBInputEvent*);
#endif
};
