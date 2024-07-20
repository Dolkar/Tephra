#pragma once

#include <tephra/vulkan/handles.hpp>
#include <tephra/vulkan/header.hpp>
#include <string>

namespace tp {

class VulkanLoader;

class VulkanGlobalInterface {
public:
    VulkanGlobalInterface();
    PFN_vkVoidFunction loadInstanceProcedure(VkInstanceHandle vkInstanceHandle, const char* procName) const;

    PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties = nullptr;
    PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties = nullptr;
    PFN_vkEnumerateInstanceVersion enumerateInstanceVersion = nullptr;
    PFN_vkCreateInstance createInstance = nullptr;

private:
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
};

class VulkanInstanceInterface {
public:
    VulkanInstanceInterface() {}
    VulkanInstanceInterface(const VulkanGlobalInterface& vkiGlobal, VkInstanceHandle vkInstanceHandle);
    PFN_vkVoidFunction loadDeviceProcedure(VkDeviceHandle vkDeviceHandle, const char* procName) const;

    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = nullptr;
    PFN_vkCreateDevice createDevice = nullptr;

private:
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
};

class VulkanPhysicalDeviceInterface {
public:
    VulkanPhysicalDeviceInterface() {}
    VulkanPhysicalDeviceInterface(const VulkanGlobalInterface& vkiGlobal, VkInstanceHandle vkInstanceHandle);

    PFN_vkGetPhysicalDeviceFeatures2 getPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceProperties2 getPhysicalDeviceProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties2 getPhysicalDeviceMemoryProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties getPhysicalDeviceFormatProperties = nullptr;
    PFN_vkGetPhysicalDeviceImageFormatProperties getPhysicalDeviceImageFormatProperties = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties = nullptr;

    // Old equivalents of functions loaded above needed for backwards compatibility with VMA
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr;
};

class VulkanPhysicalDeviceSurfaceInterfaceKHR {
public:
    VulkanPhysicalDeviceSurfaceInterfaceKHR() {}
    VulkanPhysicalDeviceSurfaceInterfaceKHR(const VulkanGlobalInterface& vkiGlobal, VkInstanceHandle vkInstanceHandle);

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModesKHR = nullptr;

    bool isLoaded() const;
};

class VulkanDebugUtilsMessengerInterfaceEXT {
public:
    VulkanDebugUtilsMessengerInterfaceEXT() {}
    VulkanDebugUtilsMessengerInterfaceEXT(const VulkanGlobalInterface& vkiGlobal, VkInstanceHandle vkInstanceHandle);

    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;

    bool isLoaded() const;
};

class VulkanDeviceInterface {
public:
    VulkanDeviceInterface() {}
    VulkanDeviceInterface(const VulkanInstanceInterface& vkiInstance, VkDeviceHandle vkDeviceHandle);

    PFN_vkDestroyDevice destroyDevice = nullptr;
    PFN_vkGetDeviceQueue getDeviceQueue = nullptr;
    PFN_vkQueueSubmit queueSubmit = nullptr;
    PFN_vkQueueWaitIdle queueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle deviceWaitIdle = nullptr;
    PFN_vkCreateShaderModule createShaderModule = nullptr;
    PFN_vkDestroyShaderModule destroyShaderModule = nullptr;
    PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout = nullptr;
    PFN_vkUpdateDescriptorSets updateDescriptorSets = nullptr;
    PFN_vkCreateDescriptorUpdateTemplate createDescriptorUpdateTemplate = nullptr;
    PFN_vkUpdateDescriptorSetWithTemplate updateDescriptorSetWithTemplate = nullptr;
    PFN_vkDestroyDescriptorUpdateTemplate destroyDescriptorUpdateTemplate = nullptr;
    PFN_vkCreateDescriptorPool createDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets allocateDescriptorSets = nullptr;
    PFN_vkResetDescriptorPool resetDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool destroyDescriptorPool = nullptr;
    PFN_vkCreatePipelineLayout createPipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipelineLayout = nullptr;
    PFN_vkCreatePipelineCache createPipelineCache = nullptr;
    PFN_vkMergePipelineCaches mergePipelineCaches = nullptr;
    PFN_vkGetPipelineCacheData getPipelineCacheData = nullptr;
    PFN_vkDestroyPipelineCache destroyPipelineCache = nullptr;
    PFN_vkCreateComputePipelines createComputePipelines = nullptr;
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkCreateBuffer createBuffer = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkCreateBufferView createBufferView = nullptr;
    PFN_vkDestroyBufferView destroyBufferView = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;
    PFN_vkCreateSampler createSampler = nullptr;
    PFN_vkDestroySampler destroySampler = nullptr;
    PFN_vkCreateCommandPool createCommandPool = nullptr;
    PFN_vkDestroyCommandPool destroyCommandPool = nullptr;
    PFN_vkCreateSemaphore createSemaphore = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkGetSemaphoreCounterValue getSemaphoreCounterValue = nullptr;
    PFN_vkWaitSemaphores waitSemaphores = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    PFN_vkGetBufferDeviceAddress getBufferDeviceAddress = nullptr;
    PFN_vkCreateQueryPool createQueryPool = nullptr;
    PFN_vkDestroyQueryPool destroyQueryPool = nullptr;
    PFN_vkGetQueryPoolResults getQueryPoolResults = nullptr;

    PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT setDebugUtilsObjectTagEXT = nullptr;
};

class VulkanDeviceMemoryInterface {
public:
    VulkanDeviceMemoryInterface() {}
    VulkanDeviceMemoryInterface(const VulkanInstanceInterface& vkiInstance, VkDeviceHandle vkDeviceHandle);

    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkMapMemory mapMemory = nullptr;
    PFN_vkUnmapMemory unmapMemory = nullptr;
    PFN_vkFlushMappedMemoryRanges flushMappedMemoryRanges = nullptr;
    PFN_vkInvalidateMappedMemoryRanges invalidateMappedMemoryRanges = nullptr;
    PFN_vkBindBufferMemory bindBufferMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkGetBufferMemoryRequirements2 getBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageMemoryRequirements2 getImageMemoryRequirements2 = nullptr;
    PFN_vkBindBufferMemory2 bindBufferMemory2 = nullptr;
    PFN_vkBindImageMemory2 bindImageMemory2 = nullptr;

    // Additional functions defined elsewhere, but used by vma
    PFN_vkCreateBuffer createBuffer = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkCmdCopyBuffer cmdCopyBuffer = nullptr;
};

class VulkanCommandInterface {
public:
    VulkanCommandInterface() {}
    VulkanCommandInterface(const VulkanInstanceInterface& vkiInstance, VkDeviceHandle vkDeviceHandle);

    PFN_vkResetCommandPool resetCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers allocateCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer beginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer endCommandBuffer = nullptr;

    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdSetViewport cmdSetViewport = nullptr;
    PFN_vkCmdSetScissor cmdSetScissor = nullptr;
    PFN_vkCmdSetLineWidth cmdSetLineWidth = nullptr;
    PFN_vkCmdSetDepthBias cmdSetDepthBias = nullptr;
    PFN_vkCmdSetBlendConstants cmdSetBlendConstants = nullptr;
    PFN_vkCmdSetDepthBounds cmdSetDepthBounds = nullptr;
    PFN_vkCmdSetStencilCompareMask cmdSetStencilCompareMask = nullptr;
    PFN_vkCmdSetStencilWriteMask cmdSetStencilWriteMask = nullptr;
    PFN_vkCmdSetStencilReference cmdSetStencilReference = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets = nullptr;
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdDraw cmdDraw = nullptr;
    PFN_vkCmdDrawIndexed cmdDrawIndexed = nullptr;
    PFN_vkCmdDrawIndirect cmdDrawIndirect = nullptr;
    PFN_vkCmdDrawIndexedIndirect cmdDrawIndexedIndirect = nullptr;
    PFN_vkCmdDrawIndirectCount cmdDrawIndirectCount = nullptr;
    PFN_vkCmdDrawIndexedIndirectCount cmdDrawIndexedIndirectCount = nullptr;
    PFN_vkCmdDispatch cmdDispatch = nullptr;
    PFN_vkCmdDispatchIndirect cmdDispatchIndirect = nullptr;
    PFN_vkCmdCopyBuffer cmdCopyBuffer = nullptr;
    PFN_vkCmdCopyImage cmdCopyImage = nullptr;
    PFN_vkCmdBlitImage cmdBlitImage = nullptr;
    PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer cmdCopyImageToBuffer = nullptr;
    PFN_vkCmdUpdateBuffer cmdUpdateBuffer = nullptr;
    PFN_vkCmdFillBuffer cmdFillBuffer = nullptr;
    PFN_vkCmdClearColorImage cmdClearColorImage = nullptr;
    PFN_vkCmdClearDepthStencilImage cmdClearDepthStencilImage = nullptr;
    PFN_vkCmdClearAttachments cmdClearAttachments = nullptr;
    PFN_vkCmdResolveImage cmdResolveImage = nullptr;
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier = nullptr;
    PFN_vkCmdBeginQuery cmdBeginQuery = nullptr;
    PFN_vkCmdEndQuery cmdEndQuery = nullptr;
    PFN_vkCmdResetQueryPool cmdResetQueryPool = nullptr;
    PFN_vkCmdWriteTimestamp cmdWriteTimestamp = nullptr;
    PFN_vkCmdCopyQueryPoolResults cmdCopyQueryPoolResults = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdExecuteCommands cmdExecuteCommands = nullptr;
    PFN_vkCmdBeginRendering cmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering cmdEndRendering = nullptr;
    PFN_vkCmdBeginQuery cmdBeginQuery = nullptr;
    PFN_vkCmdEndQuery cmdEndQuery = nullptr;
    PFN_vkCmdWriteTimestamp cmdWriteTimestamp = nullptr;

    PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT cmdInsertDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmdEndDebugUtilsLabelEXT = nullptr;
};

class VulkanSwapchainInterfaceKHR {
public:
    VulkanSwapchainInterfaceKHR() {}
    VulkanSwapchainInterfaceKHR(const VulkanInstanceInterface& vkiInstance, VkDeviceHandle vkDeviceHandle);

    PFN_vkCreateSwapchainKHR createSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR destroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR getSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR acquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR queuePresentKHR = nullptr;

    bool isLoaded() const;
};

}
