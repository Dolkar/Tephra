#pragma once

#include "vulkan_globals.hpp"
#include "../vulkan/interface.hpp"
#include "../common_impl.hpp"
#include <tephra/application.hpp>
#include <tephra/physical_device.hpp>

namespace tp {

class ApplicationContainer;
class DebugReporter;

// External access to whether important optional instance functionality (extensions, features) has been enabled
enum class InstanceFunctionality {
    DebugUtilsEXT = 1 << 0,
};
TEPHRA_MAKE_ENUM_BIT_MASK(InstanceFunctionalityMask, InstanceFunctionality)

struct VulkanDeviceCreateInfo {
    ArrayView<const uint32_t> queueFamilyCounts;
    ArrayView<const char* const> extensions;
    void* vkCreateInfoExtPtr;
};

class Instance {
public:
    explicit Instance(const ApplicationSetup& appSetup);
    explicit Instance(VkInstanceHandle vkInstanceHandle);

    ArrayView<const PhysicalDevice> getPhysicalDevices() const {
        return view(physicalDevices);
    };

    VkDeviceHandle createVulkanDevice(VkPhysicalDeviceHandle vkPhysicalDevice, const VulkanDeviceCreateInfo& createInfo);

    template <typename Interface>
    Interface loadDeviceInterface(VkDeviceHandle vkDeviceHandle) const {
        return Interface(vkiInstance, vkDeviceHandle);
    }

    void* loadDeviceProcedure(VkDeviceHandle vkDeviceHandle, const char* procName) const;

    const VulkanPhysicalDeviceInterface& getPhysicalDeviceInterface() const {
        return vkiPhysicalDevice;
    }
    
    bool isFunctionalityAvailable(InstanceFunctionality fun) const {
        return functionalityMask.contains(fun);
    }

    VkInstanceHandle vkGetInstanceHandle() const {
        return vkInstanceHandle;
    }

    TEPHRA_MAKE_NONCOPYABLE(Instance);
    TEPHRA_MAKE_NONMOVABLE(Instance);
    ~Instance();

private:
    VkInstanceHandle vkInstanceHandle;
    bool isHandleOwning;
    VulkanInstanceInterface vkiInstance;
    const VulkanPhysicalDeviceInterface vkiPhysicalDevice;
    const VulkanPhysicalDeviceSurfaceInterfaceKHR vkiSurface;
    InstanceFunctionalityMask functionalityMask;

    std::vector<PhysicalDevice> physicalDevices;
    uint64_t devicesCreatedCount = 0;

    void listPhysicalDevices();
};

}
