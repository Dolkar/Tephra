
#include "instance.hpp"
#include <tephra/physical_device.hpp>
#include <algorithm>

namespace tp {

constexpr const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

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

    if (appSetup.vulkanValidation.enable && !containsString(appSetup.instanceLayers, ValidationLayerName)) {
        if (vulkanGlobals->isInstanceLayerAvailable(ValidationLayerName)) {
            enabledLayers.push_back(ValidationLayerName);
        } else {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Vulkan validation was requested, but the standard Khronos validation layer isn't present.");
        }
    }

    return enabledLayers;
}

struct LayerSettingsEXT {
    std::vector<VkLayerSettingEXT> vkSettings;
    VkLayerSettingsCreateInfoEXT vkCreateInfo;
};

void initPrepareValidationFeatures(const VulkanValidationSetup& setup, LayerSettingsEXT* settings) {
    static const VkBool32 falseValue = VK_TRUE;
    static const VkBool32 trueValue = VK_TRUE;
    static const char* gpuBasedNone = "GPU_BASED_NONE";
    static const char* gpuBasedDebugPrintf = "GPU_BASED_DEBUG_PRINTF";
    static const char* gpuBasedAssisted = "GPU_BASED_GPU_ASSISTED";

    auto setBool = [settings](const char* name, bool value) {
        const VkBool32* valuePtr = value ? &trueValue : &falseValue;
        settings->vkSettings.push_back({ ValidationLayerName, name, VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, valuePtr });
    };

    auto setString = [settings](const char* name, const char* value) {
        settings->vkSettings.push_back({ ValidationLayerName, name, VK_LAYER_SETTING_TYPE_STRING_EXT, 1, value });
    };

    // Set bool toggles
    setBool("validate_core", setup.features.contains(ValidationFeature::Core));
    setBool("object_lifetime", setup.features.contains(ValidationFeature::ObjectLifetime));
    setBool("stateless_param", setup.features.contains(ValidationFeature::StatelessParameter));
    setBool("thread_safety", setup.features.contains(ValidationFeature::ThreadSafety));
    setBool("validate_sync", setup.features.contains(ValidationFeature::Synchronization));
    setBool("sync_queue_submit", setup.features.contains(ValidationFeature::QueueSubmitSynchronization));
    setBool("validate_best_practices", setup.features.contains(ValidationFeature::BestPractices));
    setBool("validate_best_practices_nvidia", setup.features.contains(ValidationFeature::BestPracticesNvidia));
    setBool("validate_best_practices_amd", setup.features.contains(ValidationFeature::BestPracticesAMD));
    setBool("validate_best_practices_arm", setup.features.contains(ValidationFeature::BestPracticesARM));
    setBool("validate_best_practices_img", setup.features.contains(ValidationFeature::BestPracticesIMG));

    // Set gpu based enum
    const char* gpuBased = gpuBasedNone;
    if (setup.features.contains(ValidationFeature::DebugPrintf))
        gpuBased = gpuBasedDebugPrintf;
    else if (setup.features.contains(ValidationFeature::GPUAssisted))
        gpuBased = gpuBasedAssisted;
    setString("validate_gpu_based", gpuBased);

    settings->vkCreateInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    settings->vkCreateInfo.pNext = nullptr;
    settings->vkCreateInfo.settingCount = static_cast<uint32_t>(settings->vkSettings.size());
    settings->vkCreateInfo.pSettings = settings->vkSettings.data();
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

    LayerSettingsEXT layerSettings;
    if (appSetup.vulkanValidation.enable) {
        // Need to enable the layer settings extension and set it up through the extension pointer
        if (vulkanGlobals->queryLayerExtension(ValidationLayerName, VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
            extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);

            initPrepareValidationFeatures(appSetup.vulkanValidation, &layerSettings);
            layerSettings.vkCreateInfo.pNext = appSetup.vkCreateInfoExtPtr;
            vkCreateInfoExtPtr = &layerSettings.vkCreateInfo;
        } else {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::General,
                "Vulkan instance does not support the " VK_EXT_LAYER_SETTINGS_EXTENSION_NAME
                " extension. Configuration of validation features is disabled and the defaults will be used.");
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

PFN_vkVoidFunction Instance::loadDeviceProcedure(VkDeviceHandle vkDeviceHandle, const char* procName) const {
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

        physicalDevices.push_back(std::move(device));
    }
}

}
