#pragma once

#include <tephra/tephra.hpp>
#include <iostream>

// Helper function for presenting errors to the user
void showErrorAndExit(std::string errorType, std::string errorDetail);

// A base class for containing all the windowed example demos
class Example {
public:
    // Returns the Tephra application
    virtual const tp::Application* getApplication() const = 0;

    // Called in the main loop to let the implementation play out its animations, process input, etc.
    virtual void update() = 0;

    // Called when the frame is to be redrawn. The implementation should present an image to its swapchain.
    virtual void drawFrame() = 0;

    // Called when the window is created or resized. The swapchain should be prepared here.
    virtual void resize(VkSurfaceKHR surface, uint32_t width, uint32_t height);

    // Called when the surface last passed in resize() is about to be destroyed.
    virtual void releaseSurface() = 0;

protected:
    static constexpr const char* vkLayerLunargMonitorName = "VK_LAYER_LUNARG_monitor";
    static constexpr tp::Format swapchainFormat = tp::Format::COL32_B8G8R8A8_UNORM;
    static const tp::DebugMessageSeverityMask debugSeverity;

    VkSurfaceKHR surface;
    tp::OwningPtr<tp::Swapchain> swapchain;
    uint32_t windowWidth;
    uint32_t windowHeight;

    //! Helper method for preparing the swapchain
    void prepareSwapchain(const tp::PhysicalDevice* physicalDevice, tp::Device* device, tp::DeviceQueue presentQueue);
};
