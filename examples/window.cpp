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

#include "window.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <sstream>
#include <memory>

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Assume there's only one window instance

static WindowManager* globalWindow;
// MS-Windows event handling function:
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    assert(globalWindow != nullptr);

    switch (uMsg) {
    case WM_CLOSE: PostQuitMessage(0); break;
    case WM_PAINT:
        if (globalWindow->example != nullptr && !globalWindow->quit) {
            try {
                globalWindow->example->drawFrame();
            } catch (const tp::SurfaceLostError&) {
                globalWindow->example->releaseSurface();
                globalWindow->create_surface();
            } catch (const tp::RuntimeError& e) {
                globalWindow->quit = true;
                showErrorAndExit("Frame draw failed", e.what());
            }
        }
        break;
    case WM_GETMINMAXINFO: // set window's minimum size
        ((MINMAXINFO*)lParam)->ptMinTrackSize = globalWindow->minsize;
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE:
        // Resize the application to the new window size, except when
        // it was minimized. Vulkan doesn't support images or swapchains
        // with width=0 and height=0.
        if (wParam != SIZE_MINIMIZED) {
            globalWindow->width = lParam & 0xffff;
            globalWindow->height = (lParam & 0xffff0000) >> 16;
            globalWindow->has_resized = true;
        }
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE: PostQuitMessage(0); break;
        case VK_SPACE: globalWindow->pause = !globalWindow->pause; break;
        }
        return 0;
    default: break;
    }

    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
static void handle_ping(void* data, wl_shell_surface* shell_surface, uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
}

static void handle_configure(
    void* data,
    wl_shell_surface* shell_surface,
    uint32_t edges,
    int32_t width,
    int32_t height) {}

static void handle_popup_done(void* data, wl_shell_surface* shell_surface) {}

static const wl_shell_surface_listener shell_surface_listener = { handle_ping, handle_configure, handle_popup_done };

static void pointer_handle_enter(
    void* data,
    struct wl_pointer* pointer,
    uint32_t serial,
    struct wl_surface* surface,
    wl_fixed_t sx,
    wl_fixed_t sy) {}

static void pointer_handle_leave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {}

static void pointer_handle_motion(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
}

static void pointer_handle_button(
    void* data,
    struct wl_pointer* wl_pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state) {
    WindowManager* windowMgr = (WindowManager*)data;
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        wl_shell_surface_move(windowMgr->shell_surface, windowMgr->seat, serial);
    }
}

static void pointer_handle_axis(
    void* data,
    struct wl_pointer* wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value) {}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter, pointer_handle_leave, pointer_handle_motion, pointer_handle_button, pointer_handle_axis,
};

static void keyboard_handle_keymap(void* data, struct wl_keyboard* keyboard, uint32_t format, int fd, uint32_t size) {}

static void keyboard_handle_enter(
    void* data,
    struct wl_keyboard* keyboard,
    uint32_t serial,
    struct wl_surface* surface,
    struct wl_array* keys) {}

static void keyboard_handle_leave(
    void* data,
    struct wl_keyboard* keyboard,
    uint32_t serial,
    struct wl_surface* surface) {}

static void keyboard_handle_key(
    void* data,
    struct wl_keyboard* keyboard,
    uint32_t serial,
    uint32_t time,
    uint32_t key,
    uint32_t state) {
    if (state != WL_KEYBOARD_KEY_STATE_RELEASED)
        return;
    WindowManager* windowMgr = (WindowManager*)data;
    switch (key) {
    case KEY_ESC: // Escape
        windowMgr->quit = true;
        break;
    case KEY_SPACE: // space bar
        windowMgr->pause = !windowMgr->pause;
        break;
    }
}

static void keyboard_handle_modifiers(
    void* data,
    wl_keyboard* keyboard,
    uint32_t serial,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group) {}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap, keyboard_handle_enter,     keyboard_handle_leave,
    keyboard_handle_key,    keyboard_handle_modifiers,
};

static void seat_handle_capabilities(void* data, wl_seat* seat, uint32_t caps) {
    // Subscribe to pointer events
    WindowManager* windowMgr = (WindowManager*)data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !windowMgr->pointer) {
        windowMgr->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(windowMgr->pointer, &pointer_listener, windowMgr);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && windowMgr->pointer) {
        wl_pointer_destroy(windowMgr->pointer);
        windowMgr->pointer = NULL;
    }
    // Subscribe to keyboard events
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        windowMgr->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(windowMgr->keyboard, &keyboard_listener, windowMgr);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        wl_keyboard_destroy(windowMgr->keyboard);
        windowMgr->keyboard = NULL;
    }
}

static const wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

static void registry_handle_global(
    void* data,
    wl_registry* registry,
    uint32_t id,
    const char* interface,
    uint32_t version) {
    WindowManager* windowMgr = (WindowManager*)data;
    // pickup wayland objects when they appear
    if (strcmp(interface, "wl_compositor") == 0) {
        windowMgr->compositor = (wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        windowMgr->shell = (wl_shell*)wl_registry_bind(registry, id, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        windowMgr->seat = (wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(windowMgr->seat, &seat_listener, windowMgr);
    }
}

static void registry_handle_global_remove(void* data, wl_registry* registry, uint32_t name) {}

static const wl_registry_listener registry_listener = { registry_handle_global, registry_handle_global_remove };
#endif

WindowManager::WindowManager()
    :
#if defined(VK_USE_PLATFORM_WIN32_KHR)
      connection{ nullptr },
      window{ nullptr },
      minsize(POINT{ 0, 0 }), // Use explicit construction to avoid MSVC error C2797.
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
      xlib_window{ 0 },
      xlib_wm_delete_window{ 0 },
      display{ nullptr },
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      xcb_window{ 0 },
      screen{ nullptr },
      connection{ nullptr },
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      display{ nullptr },
      registry{ nullptr },
      compositor{ nullptr },
      window{ nullptr },
      shell{ nullptr },
      shell_surface{ nullptr },
      seat{ nullptr },
      pointer{ nullptr },
      keyboard{ nullptr },
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      dfb{ nullptr },
      window{ nullptr },
      event_buffer{ nullptr },
#endif
      example{ nullptr },
      surface{ VK_NULL_HANDLE },
      width{ 800 },
      height{ 600 },
      has_resized{ true },
      pause{ false },
      quit{ false } {
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)
void WindowManager::init(HINSTANCE hInstance) {
    globalWindow = this;
    connection = hInstance;
    create_window();
}
#elif defined(VK_USE_PLATFORM_METAL_EXT)
void WindowManager::init(void* caMetalLayer) {
    caMetalLayer = caMetalLayer;
}
#else
void WindowManager::init() {
    #if defined(VK_USE_PLATFORM_XCB_KHR)
    const xcb_setup_t* setup;
    xcb_screen_iterator_t iter;
    int scr;

    const char* display_envar = getenv("DISPLAY");
    if (display_envar == nullptr || display_envar[0] == '\0') {
        showErrorAndExit("Window initialization failed", "Environment variable DISPLAY requires a valid value.");
    }

    connection = xcb_connect(nullptr, &scr);
    if (xcb_connection_has_error(connection) > 0) {
        showErrorAndExit(
            "Window initialization failed",
            "Cannot find a compatible Vulkan installable client driver (ICD).");
    }

    setup = xcb_get_setup(connection);
    iter = xcb_setup_roots_iterator(setup);
    while (scr-- > 0)
        xcb_screen_next(&iter);

    screen = iter.data;
    #elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    display = wl_display_connect(nullptr);

    if (display == nullptr) {
        showErrorAndExit(
            "Window initialization failed",
            "Cannot find a compatible Vulkan installable client driver (ICD).");
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    #endif
    create_window();
}
#endif

void WindowManager::create_surface() {
    const tp::Application* application = example->getApplication();
    VkInstance instance = application->vkGetInstanceHandle();

    if (surface != VK_NULL_HANDLE) {
        auto destroySurfacePfn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            application->vkLoadInstanceProcedure("vkDestroySurfaceKHR"));
        destroySurfacePfn(application->vkGetInstanceHandle(), surface, nullptr);
    }

    VkResult err = VK_ERROR_UNKNOWN;

    // Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        application->vkLoadInstanceProcedure("vkCreateWin32SurfaceKHR"));

    VkWin32SurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.hinstance = connection;
    createInfo.hwnd = window;

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
        application->vkLoadInstanceProcedure("vkCreateWaylandSurfaceKHR"));

    VkWaylandSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.display = display;
    createInfo.surface = window;

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        application->vkLoadInstanceProcedure("vkCreateAndroidSurfaceKHR"));

    VkAndroidSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = (struct ANativeWindow*)(window);

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        application->vkLoadInstanceProcedure("vkCreateXlibSurfaceKHR"));

    VkXlibSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dpy = display;
    createInfo.window = xlib_window;

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
        application->vkLoadInstanceProcedure("vkCreateXcbSurfaceKHR"));

    VkXcbSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.connection = connection;
    createInfo.window = xcb_window;

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateDirectFBSurfaceEXT>(
        application->vkLoadInstanceProcedure("vkCreateDirectFBSurfaceEXT"));

    VkDirectFBSurfaceCreateInfoEXT createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DIRECTFB_SURFACE_CREATE_INFO_EXT;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dfb = dfb;
    createInfo.surface = window;

    err = createSurfacePfn(instance, &createInfo, NULL, &surface);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    auto createSurfacePfn = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
        application->vkLoadInstanceProcedure("vkCreateMetalSurfaceEXT"));

    VkMetalSurfaceCreateInfoEXT surface;
    surface.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    surface.pNext = NULL;
    surface.flags = 0;
    surface.pLayer = caMetalLayer;

    err = createSurfacePfn(instance, &surface, NULL, &surface);
#endif

    if (err < 0) {
        showErrorAndExit("Window initialization failed", "Failed to create Vulkan surface.");
    }
}

void WindowManager::run(Example* example, uint32_t frameCount) {
    this->example = example;

    create_surface();

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    this->example = example;
    MSG msg;
    // Ensure wParam is initialized.
    msg.wParam = 0;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_flush(connection);
#endif

    uint32_t curFrame = 0;

    while (!quit) {
        try {
            example->update();
        } catch (const tp::RuntimeError& e) {
            showErrorAndExit("Frame update failed", e.what());
        }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
        if (pause) {
            const BOOL succ = WaitMessage();

            if (!succ) {
                showErrorAndExit("Window operation failed", "WaitMessage() failed on paused demo.");
            }
        }

        PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (msg.message == WM_QUIT) // check for a quit message
        {
            quit = true; // if found, quit app
        } else {
            /* Translate and dispatch to event queue*/
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RedrawWindow(window, nullptr, nullptr, RDW_INTERNALPAINT);

#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        XEvent event;
        if (pause) {
            XNextEvent(display, &event);
            handle_xlib_event(&event);
        }
        while (XPending(display) > 0) {
            XNextEvent(display, &event);
            handle_xlib_event(&event);
        }
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        xcb_generic_event_t* event;
        if (pause) {
            event = xcb_wait_for_event(connection);
        } else {
            event = xcb_poll_for_event(connection);
        }
        while (event) {
            handle_xcb_event(event);
            free(event);
            event = xcb_poll_for_event(connection);
        }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        if (pause) {
            wl_display_dispatch(display);
        } else {
            wl_display_dispatch_pending(display);
            update_data_buffer();
        }
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
        DFBInputEvent event;
        if (pause) {
            event_buffer->WaitForEvent(event_buffer);
            if (!event_buffer->GetEvent(event_buffer, DFB_EVENT(&event)))
                handle_directfb_event(&event);
        } else {
            if (!event_buffer->GetEvent(event_buffer, DFB_EVENT(&event)))
                handle_directfb_event(&event);
        }
#endif

        if (!pause) {
            if (has_resized) {
                try {
                    example->resize(surface, width, height);
                } catch (const tp::RuntimeError& e) {
                    showErrorAndExit("Resize failed", e.what());
                }
                has_resized = false;
            }

#if !defined(VK_USE_PLATFORM_WIN32_KHR) // On windows its redrawn in the event handler already
            try {
                example->drawFrame();
            } catch (const tp::SurfaceLostError&) {
                example->releaseSurface();
                create_surface();
            } catch (const tp::RuntimeError& e) {
                showErrorAndExit("Frame draw failed", e.what());
            }
#endif

            curFrame++;
            if (frameCount != UINT32_MAX && curFrame == frameCount) {
                quit = true;
            }
        }
    }

    example->releaseSurface();
}

void WindowManager::cleanup() {
    if (surface != VK_NULL_HANDLE) {
        auto destroySurfacePfn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            example->getApplication()->vkLoadInstanceProcedure("vkDestroySurfaceKHR"));
        destroySurfacePfn(example->getApplication()->vkGetInstanceHandle(), surface, nullptr);
    }

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    XDestroyWindow(display, xlib_window);
    XCloseDisplay(display);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_destroy_window(connection, xcb_window);
    xcb_disconnect(connection);
    free(atom_wm_delete_window);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    wl_keyboard_destroy(keyboard);
    wl_pointer_destroy(pointer);
    wl_seat_destroy(seat);
    wl_shell_surface_destroy(shell_surface);
    wl_surface_destroy(window);
    wl_shell_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    event_buffer->Release(event_buffer);
    window->Release(window);
    dfb->Release(dfb);
#endif
}

void WindowManager::create_window() {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    WNDCLASSEX win_class;

    const wchar_t* name = L"Tephra Example";

    // Initialize the window class structure:
    win_class.cbSize = sizeof(WNDCLASSEX);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = WndProc;
    win_class.cbClsExtra = 0;
    win_class.cbWndExtra = 0;
    win_class.hInstance = connection; // hInstance
    win_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    win_class.lpszMenuName = nullptr;
    win_class.lpszClassName = name;
    win_class.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    // Register window class:
    if (!RegisterClassEx(&win_class)) {
        // It didn't work, so try to give a useful error:
        showErrorAndExit("Window initialization failed", "Unexpected error trying to start the application!");
    }

    // Create window with the registered class:
    RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    window = CreateWindowEx(
        0,
        name, // class name
        name, // app name
        WS_OVERLAPPEDWINDOW | // window style
            WS_VISIBLE | WS_SYSMENU,
        100,
        100, // x/y coords
        wr.right - wr.left, // width
        wr.bottom - wr.top, // height
        nullptr, // handle to parent
        nullptr, // handle to menu
        connection, // hInstance
        nullptr); // no extra parameters

    if (!window) {
        // It didn't work, so try to give a useful error:
        showErrorAndExit("Window initialization failed", "Cannot create a window in which to draw!");
    }

    // Window client area size must be at least 1 pixel high, to prevent
    // crash.
    minsize.x = GetSystemMetrics(SM_CXMINTRACK);
    minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;

#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    const char* display_envar = getenv("DISPLAY");
    if (display_envar == nullptr || display_envar[0] == '\0') {
        showErrorAndExit("Window initialization failed", "Environment variable DISPLAY requires a valid value.");
    }

    XInitThreads();
    display = XOpenDisplay(nullptr);
    long visualMask = VisualScreenMask;
    int numberOfVisuals;
    XVisualInfo vInfoTemplate = {};
    vInfoTemplate.screen = DefaultScreen(display);
    XVisualInfo* visualInfo = XGetVisualInfo(display, visualMask, &vInfoTemplate, &numberOfVisuals);

    Colormap colormap = XCreateColormap(
        display,
        RootWindow(display, vInfoTemplate.screen),
        visualInfo->visual,
        AllocNone);

    XSetWindowAttributes windowAttributes = {};
    windowAttributes.colormap = colormap;
    windowAttributes.background_pixel = 0xFFFFFFFF;
    windowAttributes.border_pixel = 0;
    windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

    xlib_window = XCreateWindow(
        display,
        RootWindow(display, vInfoTemplate.screen),
        0,
        0,
        width,
        height,
        0,
        visualInfo->depth,
        InputOutput,
        visualInfo->visual,
        CWBackPixel | CWBorderPixel | CWEventMask | CWColormap,
        &windowAttributes);

    XSelectInput(display, xlib_window, ExposureMask | KeyPressMask);
    XMapWindow(display, xlib_window);
    XFlush(display);
    xlib_wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);

#elif defined(VK_USE_PLATFORM_XCB_KHR)
    uint32_t value_mask, value_list[32];

    xcb_window = xcb_generate_id(connection);

    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    value_list[0] = screen->black_pixel;
    value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    xcb_create_window(
        connection,
        XCB_COPY_FROM_PARENT,
        xcb_window,
        screen->root,
        0,
        0,
        width,
        height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        value_mask,
        value_list);

    /* Magic code that will send notification when window is destroyed */
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, 0);

    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
    atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

    xcb_change_property(
        connection,
        XCB_PROP_MODE_REPLACE,
        xcb_window,
        (*reply).atom,
        4,
        32,
        1,
        &(*atom_wm_delete_window).atom);

    free(reply);

    xcb_map_window(connection, xcb_window);

    // Force the x/y coordinates to 100,100 results are identical in
    // consecutive
    // runs
    const uint32_t coords[] = { 100, 100 };
    xcb_configure_window(connection, xcb_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);

#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    window = wl_compositor_create_surface(compositor);
    if (!window) {
        showErrorAndExit("Window initialization failed", "Can not create wayland_surface from compositor!");
    }

    shell_surface = wl_shell_get_shell_surface(shell, window);
    if (!shell_surface) {
        showErrorAndExit("Window initialization failed", "Can not get shell_surface from wayland_surface!");
    }

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    wl_shell_surface_set_toplevel(shell_surface);
    wl_shell_surface_set_title(shell_surface, "TephraExamples");

#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    DFBResult ret;

    ret = DirectFBInit(NULL, NULL);
    if (ret) {
        showErrorAndExit("Window initialization failed", "DirectFBInit failed to initialize DirectFB!");
    }

    ret = DirectFBCreate(&dfb);
    if (ret) {
        showErrorAndExit("Window initialization failed", "DirectFBCreate failed to create main interface of DirectFB!");
    }

    DFBSurfaceDescription desc;
    desc.flags = (DFBSurfaceDescriptionFlags)(DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT);
    desc.caps = DSCAPS_PRIMARY;
    desc.width = width;
    desc.height = height;
    ret = dfb->CreateSurface(dfb, &desc, &window);
    if (ret) {
        showErrorAndExit("Window initialization failed", "CreateSurface failed to create DirectFB surface interface!");
    }

    ret = dfb->CreateInputEventBuffer(dfb, DICAPS_KEYS, DFB_FALSE, &event_buffer);
    if (ret) {
        showErrorAndExit(
            "Window initialization failed",
            "CreateInputEventBuffer failed to create DirectFB event buffer interface!");
    }
#endif
}

#if defined(VK_USE_PLATFORM_XLIB_KHR)
void WindowManager::handle_xlib_event(const XEvent* event) {
    switch (event->type) {
    case ClientMessage:
        if ((Atom)event->xclient.data.l[0] == xlib_wm_delete_window) {
            quit = true;
        }
        break;
    case KeyPress:
        switch (event->xkey.keycode) {
        case 0x9: // Escape
            quit = true;
            break;
        case 0x41: // space bar
            pause = !pause;
            break;
        }
        break;
    case ConfigureNotify:
        if (((int32_t)width != event->xconfigure.width) || ((int32_t)height != event->xconfigure.height)) {
            width = event->xconfigure.width;
            height = event->xconfigure.height;
            has_resized = true;
        }
        break;
    default: break;
    }
}
#elif defined(VK_USE_PLATFORM_XCB_KHR)

void WindowManager::handle_xcb_event(const xcb_generic_event_t* event) {
    uint8_t event_code = event->response_type & 0x7f;
    switch (event_code) {
    case XCB_EXPOSE:
        // TODO: Resize window
        break;
    case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*atom_wm_delete_window).atom) {
            quit = true;
        }
        break;
    case XCB_KEY_RELEASE: {
        const xcb_key_release_event_t* key = (const xcb_key_release_event_t*)event;

        switch (key->detail) {
        case 0x9: // Escape
            quit = true;
            break;
        case 0x41: // space bar
            pause = !pause;
            break;
        }
    } break;
    case XCB_CONFIGURE_NOTIFY: {
        const xcb_configure_notify_event_t* cfg = (const xcb_configure_notify_event_t*)event;
        if ((width != cfg->width) || (height != cfg->height)) {
            width = cfg->width;
            height = cfg->height;
            has_resized = true;
        }
    } break;
    default: break;
    }
}

#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)

void WindowManager::handle_directfb_event(const DFBInputEvent* event) {
    if (event->type != DIET_KEYPRESS)
        return;
    switch (event->key_symbol) {
    case DIKS_ESCAPE: // Escape
        quit = true;
        break;
    case DIKS_SPACE: // space bar
        pause = !pause;
        break;
    default: break;
    }
}

#endif
