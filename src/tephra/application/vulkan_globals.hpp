#pragma once

#include "../vulkan/interface.hpp"
#include "../common_impl.hpp"
#include <tephra/version.hpp>

namespace tp {

class VulkanGlobals {
public:
    Version getInstanceApiVersion() const {
        return instanceApiVersion;
    }
    bool isInstanceExtensionAvailable(const char* extName) const;
    bool isInstanceLayerAvailable(const char* layerName) const;
    bool queryLayerExtension(const char* layerName, const char* extName) const;

    VkInstanceHandle createVulkanInstance(
        const VkApplicationInfo& applicationInfo,
        ArrayParameter<const char* const> extensions,
        ArrayParameter<const char* const> layers,
        void* vkCreateInfoExtPtr) const;

    template <typename Interface>
    Interface loadInstanceInterface(VkInstanceHandle vkInstanceHandle) const {
        return Interface(vkiGlobal, vkInstanceHandle);
    }

    void* loadInstanceProcedure(VkInstanceHandle vkInstanceHandle, const char* procName) const;

    static const VulkanGlobals* get();

    TEPHRA_MAKE_NONCOPYABLE(VulkanGlobals);
    TEPHRA_MAKE_NONMOVABLE(VulkanGlobals);

private:
    VulkanGlobalInterface vkiGlobal;
    Version instanceApiVersion;
    std::vector<VkExtensionProperties> instanceExtensions;
    std::vector<VkLayerProperties> instanceLayers;

    VulkanGlobals();
    ~VulkanGlobals() {}
};

}
