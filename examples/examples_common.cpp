#include "examples_common.hpp"

#include <string>
#include <cstdlib>

void showErrorAndExit(std::string errorType, std::string errorDetail) {
    std::string errorBody = errorDetail + "\nSee \"examples_log.txt\" for additional details.";
    std::cerr << errorType << ": " << errorBody << std::endl;

#ifdef VK_USE_PLATFORM_WIN32_KHR
    // On Windows also show a message box because stderr gets hidden
    // Just assume our errors have only single byte characters
    std::wstring wErrorType = std::wstring(errorType.begin(), errorType.end());
    std::wstring wErrorBody = std::wstring(errorBody.begin(), errorBody.end());
    MessageBox(nullptr, wErrorBody.data(), wErrorType.data(), MB_OK);
#endif

    std::abort();
}

void Example::resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    this->surface = surface;
    this->windowWidth = width;
    this->windowHeight = height;
}

void Example::prepareSwapchain(
    const tp::PhysicalDevice* physicalDevice,
    tp::Device* device,
    tp::DeviceQueue presentQueue) {
    tp::SurfaceCapabilities capabilities = physicalDevice->querySurfaceCapabilitiesKHR(surface);

    bool supportsQueue = false;
    for (tp::QueueType queueType : capabilities.supportedQueueTypes) {
        if (presentQueue.type == queueType)
            supportsQueue = true;
    }
    if (!supportsQueue) {
        showErrorAndExit("Swapchain creation failed", "Surface not supported on this device and queue.");
    }

    // Prefer the extent specified by the surface over what's provided by the windowing system
    if (capabilities.currentExtent.width != ~0) {
        windowWidth = capabilities.currentExtent.width;
        windowHeight = capabilities.currentExtent.height;
    }

    // Prefer triple buffering
    uint32_t minImageCount = 3;
    if (capabilities.maxImageCount != 0 && capabilities.maxImageCount < minImageCount)
        minImageCount = capabilities.maxImageCount;

    // Prefer RelaxedFIFO if available, otherwise fallback to FIFO, which is always supported
    auto presentMode = tp::PresentMode::FIFO;
    for (tp::PresentMode m : capabilities.supportedPresentModes) {
        if (m == tp::PresentMode::RelaxedFIFO) {
            presentMode = tp::PresentMode::RelaxedFIFO;
            break;
        }
    }

    // Check if the swapchain supports the format we used to build the pipelines
    bool supportsRequiredFormat = false;
    for (tp::Format format : capabilities.supportedFormatsSRGB) {
        if (format == swapchainFormat)
            supportsRequiredFormat = true;
    }
    if (!supportsRequiredFormat) {
        showErrorAndExit("Swapchain creation failed", "Surface doesn't support the required format.");
    }

    auto swapchainSetup = tp::SwapchainSetup(
        surface,
        presentMode,
        minImageCount,
        tp::ImageUsage::ColorAttachment,
        swapchainFormat,
        { windowWidth, windowHeight });

    // Reuse old swapchain
    swapchain = device->createSwapchainKHR(swapchainSetup, swapchain.get());
}

const tp::DebugMessageSeverityMask Example::debugSeverity = tp::DebugMessageSeverity::Verbose |
    tp::DebugMessageSeverity::Information | tp::DebugMessageSeverity::Warning | tp::DebugMessageSeverity::Error;
