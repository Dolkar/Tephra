#include "cornell.hpp"
#include "cornell_data.hpp"

CornellExample::CornellExample(std::ostream& debugStream, RenderingMethod method, bool debugMode)
    : debugHandler(debugStream, debugSeverity), method(method), mainQueue(tp::QueueType::Graphics) {
    // Initialize Tephra Application
    // Setup required and optional extensions
    std::vector<const char*> appExtensions = { tp::ApplicationExtension::KHR_Surface };

    if (debugMode && tp::Application::isExtensionAvailable(tp::ApplicationExtension::EXT_DebugUtils)) {
        appExtensions.push_back(tp::ApplicationExtension::EXT_DebugUtils);
    }

    // Also enable monitor layer if present. Validation layers are enabled through validation setup.
    std::vector<const char*> appLayers = {};
    if (tp::Application::isLayerAvailable(vkLayerLunargMonitorName)) {
        appLayers.push_back(vkLayerLunargMonitorName);
    }

    // Create the application
    auto appSetup = tp::ApplicationSetup(
        tp::ApplicationIdentifier("Cornell Path Tracing Demo"),
        tp::VulkanValidationSetup(debugMode),
        &debugHandler,
        tp::view(appExtensions),
        tp::view(appLayers));

    application = tp::Application::createApplication(appSetup);

    // Choose and initialize the rendering device
    std::vector<const char*> deviceExtensions = { tp::DeviceExtension::KHR_Swapchain,
                                                  tp::DeviceExtension::KHR_AccelerationStructure };
    switch (method) {
    case RenderingMethod::RayQuery:
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayQuery);
        break;
    case RenderingMethod::RayTracingPipeline:
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayTracingPipeline);
        break;
    }

    if (method == RenderingMethod::RayQuery)
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayQuery);
    else if (method == RenderingMethod::RayTracingPipeline)
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayTracingPipeline);

    for (const tp::PhysicalDevice& candidateDevice : application->getPhysicalDevices()) {
        for (const char* ext : deviceExtensions) {
            if (!candidateDevice.isExtensionAvailable(ext))
                continue;
        }

        physicalDevice = &candidateDevice;
        break;
    }

    if (physicalDevice == nullptr) {
        showErrorAndExit("Vulkan initialization failed", "No supported physical device has been found!");
    }

    auto deviceSetup = tp::DeviceSetup(
        physicalDevice,
        tp::viewOne(mainQueue), // Use one graphics queue for this example
        tp::view(deviceExtensions));

    device = application->createDevice(deviceSetup);

    // Also create a Job Resource Pool that temporary resources will be allocated from
    jobResourcePool = device->createJobResourcePool(tp::JobResourcePoolSetup(mainQueue));

    switch (method) {
    case RenderingMethod::RayQuery:
        prepareRayQueryPipeline();
        break;
    case RenderingMethod::RayTracingPipeline:
        prepareRayTracingPipeline();
        break;
    }
}

void CornellExample::update() {}

void CornellExample::drawFrame() {}

void CornellExample::resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    Example::resize(surface, width, height);

    // Recreate the swapchain
    prepareSwapchain(physicalDevice, device.get(), mainQueue);

    // Also trim the job resource pool to free temporary resources used for the previous resolution
    jobResourcePool->trim();
}

void CornellExample::releaseSurface() {
    device->waitForIdle();
    swapchain.reset();
}

void CornellExample::prepareBLAS() {
    std::vector<std::vector<Point>> geometry;

    auto triSetup = tp::TriangleGeometrySetup(
        triCount,
        tp::Format::COL96_R32G32B32_SFLOAT,
        vertCount,
        tp::IndexType::NoneKHR,
        false,
        tp::GeometryFlag::Opaque);
    auto blasSetup = tp::AccelerationStructureSetup::BottomLevel(
        tp::AccelerationStructureBuildFlag::PreferFastTrace, tp::viewOne(triSetup), {});

    device->allocateAccelerationStructureKHR
}
