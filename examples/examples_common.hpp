#pragma once

#include <tephra/tephra.hpp>
#include <iostream>

// Helper function for presenting errors to the user
void showErrorAndExit(std::string errorType, std::string errorDetail);

// An abstract class for containing all the example demos
class Example {
public:
    // Returns the Tephra application
    virtual const tp::Application* getApplication() const = 0;

    // Called in the main loop to let the implementation play out its animations, process input, etc.
    virtual void update() = 0;

    // Called when the frame is to be redrawn. The implementation should present an image to its swapchain.
    virtual void drawFrame() = 0;

    // Called when the window is created or resized. The swapchain should be created here.
    virtual void resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) = 0;

    // Called when the surface last passed in resize() is about to be destroyed.
    virtual void releaseSurface() = 0;
};
