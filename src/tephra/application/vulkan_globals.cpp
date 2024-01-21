#include "vulkan_globals.hpp"
#include "../vulkan/interface.hpp"

namespace tp {

VulkanGlobals::VulkanGlobals() {
    static_assert(
        Version(VK_HEADER_VERSION_COMPLETE) >= Version::getMaxUsedVulkanAPIVersion(),
        "Vulkan header version is out of date. Please update the Vulkan SDK.");

    uint32_t packedVersion;
    throwRetcodeErrors(vkiGlobal.enumerateInstanceVersion(&packedVersion));
    instanceApiVersion = Version(packedVersion);

    if (instanceApiVersion < Version::getMinSupportedVulkanInstanceVersion()) {
        std::string msg = "The Vulkan runtime library is out of date. The version of instance-level functionality is " +
            instanceApiVersion.toString() + ", but the minimum required version is " +
            Version::getMinSupportedVulkanInstanceVersion().toString();
        throwRuntimeError(RuntimeError(ErrorType::InitializationFailed, msg));
    }

    uint32_t count;
    throwRetcodeErrors(vkiGlobal.enumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    instanceExtensions.resize(count);
    throwRetcodeErrors(vkiGlobal.enumerateInstanceExtensionProperties(nullptr, &count, instanceExtensions.data()));
    instanceExtensions.resize(count);

    throwRetcodeErrors(vkiGlobal.enumerateInstanceLayerProperties(&count, nullptr));
    instanceLayers.resize(count);
    throwRetcodeErrors(vkiGlobal.enumerateInstanceLayerProperties(&count, instanceLayers.data()));
    instanceLayers.resize(count);
}

bool VulkanGlobals::isInstanceExtensionAvailable(const char* extName) const {
    for (const VkExtensionProperties& extInfo : instanceExtensions) {
        if (strcmp(extName, extInfo.extensionName) == 0) {
            return true;
        }
    }
    return false;
}

bool VulkanGlobals::isInstanceLayerAvailable(const char* layerName) const {
    for (const VkLayerProperties& layerInfo : instanceLayers) {
        if (strcmp(layerName, layerInfo.layerName) == 0) {
            return true;
        }
    }
    return false;
}

bool VulkanGlobals::queryLayerExtension(const char* layerName, const char* extName) const {
    ScratchVector<VkExtensionProperties> layerExtensions;

    uint32_t count;
    throwRetcodeErrors(vkiGlobal.enumerateInstanceExtensionProperties(layerName, &count, nullptr));
    layerExtensions.resize(count);
    throwRetcodeErrors(vkiGlobal.enumerateInstanceExtensionProperties(layerName, &count, layerExtensions.data()));
    layerExtensions.resize(count);

    for (const VkExtensionProperties& extInfo : layerExtensions) {
        if (strcmp(extName, extInfo.extensionName) == 0) {
            return true;
        }
    }
    return false;
}

VkInstanceHandle VulkanGlobals::createVulkanInstance(
    const VkApplicationInfo& applicationInfo,
    ArrayParameter<const char* const> extensions,
    ArrayParameter<const char* const> layers,
    void* vkCreateInfoExtPtr) const {
    VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = vkCreateInfoExtPtr;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = &applicationInfo;

    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();

    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();

    VkInstance vkInstanceHandle;
    throwRetcodeErrors(vkiGlobal.createInstance(&instanceInfo, nullptr, &vkInstanceHandle));
    return VkInstanceHandle(vkInstanceHandle);
}

PFN_vkVoidFunction VulkanGlobals::loadInstanceProcedure(VkInstanceHandle vkInstanceHandle, const char* procName) const {
    return vkiGlobal.loadInstanceProcedure(vkInstanceHandle, procName);
}

const VulkanGlobals* VulkanGlobals::get() {
    static VulkanGlobals globalInstance;
    return &globalInstance;
}

}
