#include "interface.hpp"
#include "loader.hpp"
#include "../common_impl.hpp"

namespace tp {

PFN_vkVoidFunction checkLoadedProc(PFN_vkVoidFunction procPtr, const char* procName, const char* scope) {
    if (procPtr == nullptr) {
        std::string errorMessage = "Unable to load " + std::string(scope) + " Vulkan procedure '" +
            std::string(procName) + "'";
        throwRuntimeError(RuntimeError(ErrorType::InitializationFailed, errorMessage));
    }
    return procPtr;
}

#define LOAD_EXPORTED_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>(checkLoadedProc(vulkanLoader.loadExportedProcedure(#fun), #fun, "exported"))
#define LOAD_GLOBAL_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>(checkLoadedProc(loadInstanceProcedure(VkInstanceHandle(), #fun), #fun, "global"))
#define LOAD_INSTANCE_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>( \
        checkLoadedProc(vkiGlobal.loadInstanceProcedure(vkInstanceHandle, #fun), #fun, "instance"))
#define LOAD_INSTANCE_EXT_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>(vkiGlobal.loadInstanceProcedure(vkInstanceHandle, #fun))
#define LOAD_DEVICE_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>(checkLoadedProc(vkiInstance.loadDeviceProcedure(vkDeviceHandle, #fun), #fun, "device"))
#define LOAD_DEVICE_EXT_PROCEDURE(fun) \
    reinterpret_cast<PFN_##fun>(vkiInstance.loadDeviceProcedure(vkDeviceHandle, #fun))

VulkanGlobalInterface::VulkanGlobalInterface() {
    static VulkanLoader vulkanLoader;
    getInstanceProcAddr = LOAD_EXPORTED_PROCEDURE(vkGetInstanceProcAddr);

    enumerateInstanceExtensionProperties = LOAD_GLOBAL_PROCEDURE(vkEnumerateInstanceExtensionProperties);
    enumerateInstanceLayerProperties = LOAD_GLOBAL_PROCEDURE(vkEnumerateInstanceLayerProperties);
    enumerateInstanceVersion = LOAD_GLOBAL_PROCEDURE(vkEnumerateInstanceVersion);
    createInstance = LOAD_GLOBAL_PROCEDURE(vkCreateInstance);
}

PFN_vkVoidFunction VulkanGlobalInterface::loadInstanceProcedure(VkInstanceHandle vkInstanceHandle, const char* procName)
    const {
    return getInstanceProcAddr(vkInstanceHandle, procName);
}

VulkanInstanceInterface::VulkanInstanceInterface(
    const VulkanGlobalInterface& vkiGlobal,
    VkInstanceHandle vkInstanceHandle) {
    destroyInstance = LOAD_INSTANCE_PROCEDURE(vkDestroyInstance);
    enumeratePhysicalDevices = LOAD_INSTANCE_PROCEDURE(vkEnumeratePhysicalDevices);
    createDevice = LOAD_INSTANCE_PROCEDURE(vkCreateDevice);
    getDeviceProcAddr = LOAD_INSTANCE_PROCEDURE(vkGetDeviceProcAddr);
}

PFN_vkVoidFunction VulkanInstanceInterface::loadDeviceProcedure(VkDeviceHandle vkDeviceHandle, const char* procName)
    const {
    return getDeviceProcAddr(vkDeviceHandle, procName);
}

VulkanPhysicalDeviceInterface::VulkanPhysicalDeviceInterface(
    const VulkanGlobalInterface& vkiGlobal,
    VkInstanceHandle vkInstanceHandle) {
    getPhysicalDeviceFeatures2 = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceFeatures2);
    getPhysicalDeviceProperties2 = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceProperties2);
    getPhysicalDeviceMemoryProperties2 = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceMemoryProperties2);
    getPhysicalDeviceQueueFamilyProperties = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceQueueFamilyProperties);
    getPhysicalDeviceFormatProperties = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceFormatProperties);
    getPhysicalDeviceImageFormatProperties = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceImageFormatProperties);
    enumerateDeviceExtensionProperties = LOAD_INSTANCE_PROCEDURE(vkEnumerateDeviceExtensionProperties);

    getPhysicalDeviceProperties = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceProperties);
    getPhysicalDeviceMemoryProperties = LOAD_INSTANCE_PROCEDURE(vkGetPhysicalDeviceMemoryProperties);
}

VulkanPhysicalDeviceSurfaceInterfaceKHR::VulkanPhysicalDeviceSurfaceInterfaceKHR(
    const VulkanGlobalInterface& vkiGlobal,
    VkInstanceHandle vkInstanceHandle) {
    getPhysicalDeviceSurfaceSupportKHR = LOAD_INSTANCE_EXT_PROCEDURE(vkGetPhysicalDeviceSurfaceSupportKHR);
    getPhysicalDeviceSurfaceCapabilitiesKHR = LOAD_INSTANCE_EXT_PROCEDURE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    getPhysicalDeviceSurfaceFormatsKHR = LOAD_INSTANCE_EXT_PROCEDURE(vkGetPhysicalDeviceSurfaceFormatsKHR);
    getPhysicalDeviceSurfacePresentModesKHR = LOAD_INSTANCE_EXT_PROCEDURE(vkGetPhysicalDeviceSurfacePresentModesKHR);
}

bool VulkanPhysicalDeviceSurfaceInterfaceKHR::isLoaded() const {
    return getPhysicalDeviceSurfaceSupportKHR != nullptr;
}

VulkanDebugUtilsMessengerInterfaceEXT::VulkanDebugUtilsMessengerInterfaceEXT(
    const VulkanGlobalInterface& vkiGlobal,
    VkInstanceHandle vkInstanceHandle) {
    createDebugUtilsMessengerEXT = LOAD_INSTANCE_EXT_PROCEDURE(vkCreateDebugUtilsMessengerEXT);
    destroyDebugUtilsMessengerEXT = LOAD_INSTANCE_EXT_PROCEDURE(vkDestroyDebugUtilsMessengerEXT);
}

bool VulkanDebugUtilsMessengerInterfaceEXT::isLoaded() const {
    return createDebugUtilsMessengerEXT != nullptr;
}

VulkanDeviceInterface::VulkanDeviceInterface(const VulkanInstanceInterface& vkiInstance, VkDeviceHandle vkDeviceHandle) {
    destroyDevice = LOAD_DEVICE_PROCEDURE(vkDestroyDevice);
    getDeviceQueue = LOAD_DEVICE_PROCEDURE(vkGetDeviceQueue);
    queueSubmit = LOAD_DEVICE_PROCEDURE(vkQueueSubmit);
    queueWaitIdle = LOAD_DEVICE_PROCEDURE(vkQueueWaitIdle);
    deviceWaitIdle = LOAD_DEVICE_PROCEDURE(vkDeviceWaitIdle);
    createShaderModule = LOAD_DEVICE_PROCEDURE(vkCreateShaderModule);
    destroyShaderModule = LOAD_DEVICE_PROCEDURE(vkDestroyShaderModule);
    createDescriptorSetLayout = LOAD_DEVICE_PROCEDURE(vkCreateDescriptorSetLayout);
    destroyDescriptorSetLayout = LOAD_DEVICE_PROCEDURE(vkDestroyDescriptorSetLayout);
    updateDescriptorSets = LOAD_DEVICE_PROCEDURE(vkUpdateDescriptorSets);
    createDescriptorUpdateTemplate = LOAD_DEVICE_PROCEDURE(vkCreateDescriptorUpdateTemplate);
    updateDescriptorSetWithTemplate = LOAD_DEVICE_PROCEDURE(vkUpdateDescriptorSetWithTemplate);
    destroyDescriptorUpdateTemplate = LOAD_DEVICE_PROCEDURE(vkDestroyDescriptorUpdateTemplate);
    createDescriptorPool = LOAD_DEVICE_PROCEDURE(vkCreateDescriptorPool);
    allocateDescriptorSets = LOAD_DEVICE_PROCEDURE(vkAllocateDescriptorSets);
    resetDescriptorPool = LOAD_DEVICE_PROCEDURE(vkResetDescriptorPool);
    destroyDescriptorPool = LOAD_DEVICE_PROCEDURE(vkDestroyDescriptorPool);
    createPipelineLayout = LOAD_DEVICE_PROCEDURE(vkCreatePipelineLayout);
    destroyPipelineLayout = LOAD_DEVICE_PROCEDURE(vkDestroyPipelineLayout);
    createPipelineCache = LOAD_DEVICE_PROCEDURE(vkCreatePipelineCache);
    mergePipelineCaches = LOAD_DEVICE_PROCEDURE(vkMergePipelineCaches);
    getPipelineCacheData = LOAD_DEVICE_PROCEDURE(vkGetPipelineCacheData);
    destroyPipelineCache = LOAD_DEVICE_PROCEDURE(vkDestroyPipelineCache);
    createComputePipelines = LOAD_DEVICE_PROCEDURE(vkCreateComputePipelines);
    createGraphicsPipelines = LOAD_DEVICE_PROCEDURE(vkCreateGraphicsPipelines);
    destroyPipeline = LOAD_DEVICE_PROCEDURE(vkDestroyPipeline);
    createBuffer = LOAD_DEVICE_PROCEDURE(vkCreateBuffer);
    destroyBuffer = LOAD_DEVICE_PROCEDURE(vkDestroyBuffer);
    createImage = LOAD_DEVICE_PROCEDURE(vkCreateImage);
    destroyImage = LOAD_DEVICE_PROCEDURE(vkDestroyImage);
    createBufferView = LOAD_DEVICE_PROCEDURE(vkCreateBufferView);
    destroyBufferView = LOAD_DEVICE_PROCEDURE(vkDestroyBufferView);
    createImageView = LOAD_DEVICE_PROCEDURE(vkCreateImageView);
    destroyImageView = LOAD_DEVICE_PROCEDURE(vkDestroyImageView);
    createSampler = LOAD_DEVICE_PROCEDURE(vkCreateSampler);
    destroySampler = LOAD_DEVICE_PROCEDURE(vkDestroySampler);
    createCommandPool = LOAD_DEVICE_PROCEDURE(vkCreateCommandPool);
    destroyCommandPool = LOAD_DEVICE_PROCEDURE(vkDestroyCommandPool);
    createSemaphore = LOAD_DEVICE_PROCEDURE(vkCreateSemaphore);
    destroySemaphore = LOAD_DEVICE_PROCEDURE(vkDestroySemaphore);
    getSemaphoreCounterValue = LOAD_DEVICE_PROCEDURE(vkGetSemaphoreCounterValue);
    waitSemaphores = LOAD_DEVICE_PROCEDURE(vkWaitSemaphores);
    signalSemaphore = LOAD_DEVICE_PROCEDURE(vkSignalSemaphore);
    getBufferDeviceAddress = LOAD_DEVICE_PROCEDURE(vkGetBufferDeviceAddress);
    createQueryPool = LOAD_DEVICE_PROCEDURE(vkCreateQueryPool);
    destroyQueryPool = LOAD_DEVICE_PROCEDURE(vkDestroyQueryPool);
    getQueryPoolResults = LOAD_DEVICE_PROCEDURE(vkGetQueryPoolResults);
    resetQueryPool = LOAD_DEVICE_PROCEDURE(vkResetQueryPool);

    createSwapchainKHR = LOAD_DEVICE_EXT_PROCEDURE(vkCreateSwapchainKHR);
    destroySwapchainKHR = LOAD_DEVICE_EXT_PROCEDURE(vkDestroySwapchainKHR);
    getSwapchainImagesKHR = LOAD_DEVICE_EXT_PROCEDURE(vkGetSwapchainImagesKHR);
    acquireNextImageKHR = LOAD_DEVICE_EXT_PROCEDURE(vkAcquireNextImageKHR);
    queuePresentKHR = LOAD_DEVICE_EXT_PROCEDURE(vkQueuePresentKHR);

    createAccelerationStructureKHR = LOAD_DEVICE_EXT_PROCEDURE(vkCreateAccelerationStructureKHR);
    destroyAccelerationStructureKHR = LOAD_DEVICE_EXT_PROCEDURE(vkDestroyAccelerationStructureKHR);
    getAccelerationStructureBuildSizesKHR = LOAD_DEVICE_EXT_PROCEDURE(vkGetAccelerationStructureBuildSizesKHR);
    getAccelerationStructureDeviceAddressKHR = LOAD_DEVICE_EXT_PROCEDURE(vkGetAccelerationStructureDeviceAddressKHR);

    setDebugUtilsObjectNameEXT = LOAD_DEVICE_EXT_PROCEDURE(vkSetDebugUtilsObjectNameEXT);
    setDebugUtilsObjectTagEXT = LOAD_DEVICE_EXT_PROCEDURE(vkSetDebugUtilsObjectTagEXT);
}

VulkanDeviceMemoryInterface::VulkanDeviceMemoryInterface(
    const VulkanInstanceInterface& vkiInstance,
    VkDeviceHandle vkDeviceHandle) {
    allocateMemory = LOAD_DEVICE_PROCEDURE(vkAllocateMemory);
    freeMemory = LOAD_DEVICE_PROCEDURE(vkFreeMemory);
    mapMemory = LOAD_DEVICE_PROCEDURE(vkMapMemory);
    unmapMemory = LOAD_DEVICE_PROCEDURE(vkUnmapMemory);
    flushMappedMemoryRanges = LOAD_DEVICE_PROCEDURE(vkFlushMappedMemoryRanges);
    invalidateMappedMemoryRanges = LOAD_DEVICE_PROCEDURE(vkInvalidateMappedMemoryRanges);
    bindBufferMemory = LOAD_DEVICE_PROCEDURE(vkBindBufferMemory);
    bindImageMemory = LOAD_DEVICE_PROCEDURE(vkBindImageMemory);
    getBufferMemoryRequirements = LOAD_DEVICE_PROCEDURE(vkGetBufferMemoryRequirements);
    getImageMemoryRequirements = LOAD_DEVICE_PROCEDURE(vkGetImageMemoryRequirements);
    getBufferMemoryRequirements2 = LOAD_DEVICE_PROCEDURE(vkGetBufferMemoryRequirements2);
    getImageMemoryRequirements2 = LOAD_DEVICE_PROCEDURE(vkGetImageMemoryRequirements2);
    bindBufferMemory2 = LOAD_DEVICE_PROCEDURE(vkBindBufferMemory2);
    bindImageMemory2 = LOAD_DEVICE_PROCEDURE(vkBindImageMemory2);

    createBuffer = LOAD_DEVICE_PROCEDURE(vkCreateBuffer);
    destroyBuffer = LOAD_DEVICE_PROCEDURE(vkDestroyBuffer);
    createImage = LOAD_DEVICE_PROCEDURE(vkCreateImage);
    destroyImage = LOAD_DEVICE_PROCEDURE(vkDestroyImage);
    cmdCopyBuffer = LOAD_DEVICE_PROCEDURE(vkCmdCopyBuffer);
}

VulkanCommandInterface::VulkanCommandInterface(
    const VulkanInstanceInterface& vkiInstance,
    VkDeviceHandle vkDeviceHandle) {
    resetCommandPool = LOAD_DEVICE_PROCEDURE(vkResetCommandPool);
    allocateCommandBuffers = LOAD_DEVICE_PROCEDURE(vkAllocateCommandBuffers);
    beginCommandBuffer = LOAD_DEVICE_PROCEDURE(vkBeginCommandBuffer);
    endCommandBuffer = LOAD_DEVICE_PROCEDURE(vkEndCommandBuffer);

    cmdBindPipeline = LOAD_DEVICE_PROCEDURE(vkCmdBindPipeline);
    cmdSetViewport = LOAD_DEVICE_PROCEDURE(vkCmdSetViewport);
    cmdSetScissor = LOAD_DEVICE_PROCEDURE(vkCmdSetScissor);
    cmdSetLineWidth = LOAD_DEVICE_PROCEDURE(vkCmdSetLineWidth);
    cmdSetDepthBias = LOAD_DEVICE_PROCEDURE(vkCmdSetDepthBias);
    cmdSetBlendConstants = LOAD_DEVICE_PROCEDURE(vkCmdSetBlendConstants);
    cmdSetDepthBounds = LOAD_DEVICE_PROCEDURE(vkCmdSetDepthBounds);
    cmdSetStencilCompareMask = LOAD_DEVICE_PROCEDURE(vkCmdSetStencilCompareMask);
    cmdSetStencilWriteMask = LOAD_DEVICE_PROCEDURE(vkCmdSetStencilWriteMask);
    cmdSetStencilReference = LOAD_DEVICE_PROCEDURE(vkCmdSetStencilReference);
    cmdBindDescriptorSets = LOAD_DEVICE_PROCEDURE(vkCmdBindDescriptorSets);
    cmdBindIndexBuffer = LOAD_DEVICE_PROCEDURE(vkCmdBindIndexBuffer);
    cmdBindVertexBuffers = LOAD_DEVICE_PROCEDURE(vkCmdBindVertexBuffers);
    cmdDraw = LOAD_DEVICE_PROCEDURE(vkCmdDraw);
    cmdDrawIndexed = LOAD_DEVICE_PROCEDURE(vkCmdDrawIndexed);
    cmdDrawIndirect = LOAD_DEVICE_PROCEDURE(vkCmdDrawIndirect);
    cmdDrawIndexedIndirect = LOAD_DEVICE_PROCEDURE(vkCmdDrawIndexedIndirect);
    cmdDrawIndirectCount = LOAD_DEVICE_PROCEDURE(vkCmdDrawIndirectCount);
    cmdDrawIndexedIndirectCount = LOAD_DEVICE_PROCEDURE(vkCmdDrawIndexedIndirectCount);
    cmdDispatch = LOAD_DEVICE_PROCEDURE(vkCmdDispatch);
    cmdDispatchIndirect = LOAD_DEVICE_PROCEDURE(vkCmdDispatchIndirect);
    cmdCopyBuffer = LOAD_DEVICE_PROCEDURE(vkCmdCopyBuffer);
    cmdCopyImage = LOAD_DEVICE_PROCEDURE(vkCmdCopyImage);
    cmdBlitImage = LOAD_DEVICE_PROCEDURE(vkCmdBlitImage);
    cmdCopyBufferToImage = LOAD_DEVICE_PROCEDURE(vkCmdCopyBufferToImage);
    cmdCopyImageToBuffer = LOAD_DEVICE_PROCEDURE(vkCmdCopyImageToBuffer);
    cmdUpdateBuffer = LOAD_DEVICE_PROCEDURE(vkCmdUpdateBuffer);
    cmdFillBuffer = LOAD_DEVICE_PROCEDURE(vkCmdFillBuffer);
    cmdClearColorImage = LOAD_DEVICE_PROCEDURE(vkCmdClearColorImage);
    cmdClearDepthStencilImage = LOAD_DEVICE_PROCEDURE(vkCmdClearDepthStencilImage);
    cmdClearAttachments = LOAD_DEVICE_PROCEDURE(vkCmdClearAttachments);
    cmdResolveImage = LOAD_DEVICE_PROCEDURE(vkCmdResolveImage);
    cmdPipelineBarrier = LOAD_DEVICE_PROCEDURE(vkCmdPipelineBarrier);
    cmdBeginQuery = LOAD_DEVICE_PROCEDURE(vkCmdBeginQuery);
    cmdEndQuery = LOAD_DEVICE_PROCEDURE(vkCmdEndQuery);
    cmdWriteTimestamp = LOAD_DEVICE_PROCEDURE(vkCmdWriteTimestamp);
    cmdCopyQueryPoolResults = LOAD_DEVICE_PROCEDURE(vkCmdCopyQueryPoolResults);
    cmdPushConstants = LOAD_DEVICE_PROCEDURE(vkCmdPushConstants);
    cmdExecuteCommands = LOAD_DEVICE_PROCEDURE(vkCmdExecuteCommands);
    cmdBeginRendering = LOAD_DEVICE_PROCEDURE(vkCmdBeginRendering);
    cmdEndRendering = LOAD_DEVICE_PROCEDURE(vkCmdEndRendering);

    cmdBuildAccelerationStructuresKHR = LOAD_DEVICE_EXT_PROCEDURE(vkCmdBuildAccelerationStructuresKHR);
    cmdCopyAccelerationStructureKHR = LOAD_DEVICE_EXT_PROCEDURE(vkCmdCopyAccelerationStructureKHR);
    cmdWriteAccelerationStructuresPropertiesKHR = LOAD_DEVICE_EXT_PROCEDURE(
        vkCmdWriteAccelerationStructuresPropertiesKHR);

    cmdBeginDebugUtilsLabelEXT = LOAD_DEVICE_EXT_PROCEDURE(vkCmdBeginDebugUtilsLabelEXT);
    cmdInsertDebugUtilsLabelEXT = LOAD_DEVICE_EXT_PROCEDURE(vkCmdInsertDebugUtilsLabelEXT);
    cmdEndDebugUtilsLabelEXT = LOAD_DEVICE_EXT_PROCEDURE(vkCmdEndDebugUtilsLabelEXT);
}

}
