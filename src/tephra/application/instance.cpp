
#include "instance.hpp"
#include <tephra/physical_device.hpp>
#include <algorithm>

namespace tp {

constexpr const char* StandardValidationLayerName = "VK_LAYER_KHRONOS_validation";

std::vector<const char*> initPrepareExtensions(const ApplicationSetup& appSetup, const VulkanGlobals* vulkanGlobals) {
    // At least one of the platform specific extensions needs to be available for surface support
    static const char* platformExtensionNames[] = {
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_MIR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,   VK_KHR_XCB_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };

    std::vector<const char*> enabledExtensions{ appSetup.extensions.begin(), appSetup.extensions.end() };

    // Surface extension also needs at least one platform surface extension
    if (containsString(appSetup.extensions, ApplicationExtension::KHR_Surface)) {
        bool hasPlatformSurfaceSupport = false;
        for (const char* platformExtension : platformExtensionNames) {
            if (vulkanGlobals->isInstanceExtensionAvailable(platformExtension)) {
                enabledExtensions.push_back(platformExtension);
                hasPlatformSurfaceSupport = true;
            }
        }

        if (!hasPlatformSurfaceSupport) {
            std::string errorMessage = "No platform surface extension is available for " +
                std::string(ApplicationExtension::KHR_Surface) + " support.";
            throwRuntimeError(UnsupportedOperationError(ErrorType::ExtensionNotPresent, errorMessage));
        }
    }

    return enabledExtensions;
}

std::vector<const char*> initPrepareLayers(const ApplicationSetup& appSetup, const VulkanGlobals* vulkanGlobals) {
    std::vector<const char*> enabledLayers{ appSetup.instanceLayers.begin(), appSetup.instanceLayers.end() };

    if (appSetup.vulkanValidation.enable && !containsString(appSetup.instanceLayers, StandardValidationLayerName)) {
        if (vulkanGlobals->isInstanceLayerAvailable(StandardValidationLayerName)) {
            enabledLayers.push_back(StandardValidationLayerName);
        } else {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Vulkan validation was requested, but the standard Khronos validation layer isn't present.");
        }
    }

    return enabledLayers;
}

struct ValidationFeaturesSetup {
    VkValidationFeaturesEXT vkValidationFeatures;
    std::vector<VkValidationFeatureEnableEXT> enableSet;
    std::vector<VkValidationFeatureDisableEXT> disableSet;
};

void initPrepareValidationFeatures(const VulkanValidationSetup& vulkanValidation, ValidationFeaturesSetup* setup) {
    // Convert Tephra bitmasks to an array of features. They aren't directly convertible, but (for now) the indices of
    // the bits used are convertible (see definition of ValidationFeatureEnable)
    uint32_t mask = static_cast<uint32_t>(vulkanValidation.enabledFeatures);
    uint32_t bitIndex = 0;
    while (mask) {
        if ((mask & 1) == 1) {
            setup->enableSet.push_back(static_cast<VkValidationFeatureEnableEXT>(bitIndex));
        }
        mask >>= 1;
        bitIndex++;
    }

    mask = static_cast<uint32_t>(vulkanValidation.disabledFeatures);
    bitIndex = 0;
    while (mask) {
        if ((mask & 1) == 1) {
            setup->disableSet.push_back(static_cast<VkValidationFeatureDisableEXT>(bitIndex));
        }
        mask >>= 1;
        bitIndex++;
    }

    setup->vkValidationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    setup->vkValidationFeatures.pNext = nullptr;
    setup->vkValidationFeatures.enabledValidationFeatureCount = static_cast<uint32_t>(setup->enableSet.size());
    setup->vkValidationFeatures.pEnabledValidationFeatures = setup->enableSet.data();
    setup->vkValidationFeatures.disabledValidationFeatureCount = static_cast<uint32_t>(setup->disableSet.size());
    setup->vkValidationFeatures.pDisabledValidationFeatures = setup->disableSet.data();
}

VkInstanceHandle initCreateVulkanInstance(const VulkanGlobals* vulkanGlobals, const ApplicationSetup& appSetup) {
    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = appSetup.applicationIdentifier.applicationName;
    appInfo.applicationVersion = appSetup.applicationIdentifier.applicationVersion.pack();
    appInfo.pEngineName = appSetup.applicationIdentifier.engineName;
    appInfo.engineVersion = appSetup.applicationIdentifier.engineVersion.pack();
    appInfo.apiVersion = tp::max(appSetup.apiVersion.pack(), Version::getMaxUsedVulkanAPIVersion().pack());
    std::vector<const char*> extensions = initPrepareExtensions(appSetup, vulkanGlobals);
    std::vector<const char*> layers = initPrepareLayers(appSetup, vulkanGlobals);

    void* vkCreateInfoExtPtr = appSetup.vkCreateInfoExtPtr;

    ValidationFeaturesSetup validationFeaturesSetup;
    if (appSetup.vulkanValidation.enable &&
        (appSetup.vulkanValidation.enabledFeatures != ValidationFeatureEnableMask::None() ||
         appSetup.vulkanValidation.disabledFeatures != ValidationFeatureDisableMask::None())) {
        // Need to enable the validation feature extension and set it up through the extension pointer
        if (vulkanGlobals->queryLayerExtension(
                StandardValidationLayerName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
            extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

            initPrepareValidationFeatures(appSetup.vulkanValidation, &validationFeaturesSetup);
            validationFeaturesSetup.vkValidationFeatures.pNext = appSetup.vkCreateInfoExtPtr;
            vkCreateInfoExtPtr = &validationFeaturesSetup.vkValidationFeatures;
        } else {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Specific validation features were requested, but Vulkan instance does not support "
                "the " VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME " extension.");
        }
    }

    return vulkanGlobals->createVulkanInstance(appInfo, view(extensions), view(layers), vkCreateInfoExtPtr);
}

Instance::Instance(const ApplicationSetup& appSetup)
    : vkInstanceHandle(initCreateVulkanInstance(VulkanGlobals::get(), appSetup)),
      isHandleOwning(true),
      vkiInstance(VulkanGlobals::get()->loadInstanceInterface<VulkanInstanceInterface>(vkInstanceHandle)),
      vkiPhysicalDevice(VulkanGlobals::get()->loadInstanceInterface<VulkanPhysicalDeviceInterface>(vkInstanceHandle)),
      vkiSurface(
          VulkanGlobals::get()->loadInstanceInterface<VulkanPhysicalDeviceSurfaceInterfaceKHR>(vkInstanceHandle)) {
    listPhysicalDevices();
    if (containsString(appSetup.extensions, ApplicationExtension::EXT_DebugUtils))
        functionalityMask |= InstanceFunctionality::DebugUtilsEXT;
}

Instance::Instance(VkInstanceHandle vkInstanceHandle)
    : vkInstanceHandle(vkInstanceHandle),
      isHandleOwning(false),
      vkiInstance(VulkanGlobals::get()->loadInstanceInterface<VulkanInstanceInterface>(vkInstanceHandle)),
      vkiPhysicalDevice(VulkanGlobals::get()->loadInstanceInterface<VulkanPhysicalDeviceInterface>(vkInstanceHandle)),
      vkiSurface(
          VulkanGlobals::get()->loadInstanceInterface<VulkanPhysicalDeviceSurfaceInterfaceKHR>(vkInstanceHandle)) {
    listPhysicalDevices();
}

VkDeviceHandle Instance::createVulkanDevice(
    VkPhysicalDeviceHandle vkPhysicalDevice,
    const VulkanDeviceCreateInfo& createInfo) {
    // Create an array of queue priorities, setting all to 1.
    // TODO: Add functionality to customize priorities later. They don't really affect anything on most platforms
    // anyway.
    uint32_t maxQueues = 0;
    for (uint32_t queueFamilyCount : createInfo.queueFamilyCounts) {
        maxQueues = max(maxQueues, queueFamilyCount);
    }
    ScratchVector<float> queuePriorities;
    queuePriorities.resize(maxQueues, 1.0f);

    ScratchVector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < createInfo.queueFamilyCounts.size(); queueFamilyIndex++) {
        uint32_t queueFamilyCount = createInfo.queueFamilyCounts[queueFamilyIndex];
        if (queueFamilyCount == 0)
            continue;
        VkDeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.flags = 0;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = queueFamilyCount;
        queueCreateInfo.pQueuePriorities = queuePriorities.data();
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo deviceInfo;
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = createInfo.vkCreateInfoExtPtr;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();

    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(createInfo.extensions.size());
    deviceInfo.ppEnabledExtensionNames = createInfo.extensions.data();

    // Device layers are deprecated
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;

    // VkPhysicalDeviceFeatures2 structure is used instead
    deviceInfo.pEnabledFeatures = nullptr;

    VkDevice vkDeviceHandle;
    throwRetcodeErrors(vkiInstance.createDevice(vkPhysicalDevice, &deviceInfo, nullptr, &vkDeviceHandle));

    return VkDeviceHandle(vkDeviceHandle);
}

void* Instance::loadDeviceProcedure(VkDeviceHandle vkDeviceHandle, const char* procName) const {
    return vkiInstance.loadDeviceProcedure(vkDeviceHandle, procName);
}

Instance::~Instance() {
    if (!vkInstanceHandle.isNull() && isHandleOwning) {
        vkiInstance.destroyInstance(vkInstanceHandle, nullptr);
        vkInstanceHandle = {};
    }
}

void Instance::listPhysicalDevices() {
    ScratchVector<VkPhysicalDeviceHandle> physicalDeviceHandles;
    uint32_t count;
    throwRetcodeErrors(vkiInstance.enumeratePhysicalDevices(vkInstanceHandle, &count, nullptr));
    physicalDeviceHandles.resize(count);
    throwRetcodeErrors(vkiInstance.enumeratePhysicalDevices(
        vkInstanceHandle, &count, vkCastTypedHandlePtr(physicalDeviceHandles.data())));
    physicalDeviceHandles.resize(count);

    physicalDevices.reserve(count);
    for (VkPhysicalDeviceHandle vkPhysicalDeviceHandle : physicalDeviceHandles) {
        auto device = PhysicalDevice(vkiPhysicalDevice, vkiSurface, vkPhysicalDeviceHandle);

        // Check if physical device is supported by Tephra
        if (device.apiVersion < Version::getMinSupportedVulkanDeviceVersion()) {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Physical device '",
                device.name,
                "' is not available because its Vulkan API version (",
                device.apiVersion.toString(),
                ") is outdated. Minimum required: ",
                Version::getMinSupportedVulkanDeviceVersion().toString());
            continue;
        }

        const auto& vk12Features = device.vkQueryFeatures<VkPhysicalDeviceVulkan12Features>();
        if (vk12Features.timelineSemaphore == VK_FALSE) {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Physical device '",
                device.name,
                "' is not available because it does not support the required timeline semaphore feature.");
            continue;
        }

        physicalDevices.push_back(std::move(device));
    }
}

}
