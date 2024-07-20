#pragma once

#include "../vulkan/interface.hpp"
#include "../application/instance.hpp"
#include "../common_impl.hpp"
#include <tephra/physical_device.hpp>
#include <tephra/swapchain.hpp>
#include <tephra/device.hpp>

namespace tp {

class Instance;
class QueueMap;

// External access to whether important optional functionality (extensions, features, etc) is available
enum class Functionality {
    DebugUtilsEXT = 1 << 0,
    MemoryBudgetEXT = 1 << 1,
    BufferDeviceAddress = 1 << 2,
};
TEPHRA_MAKE_ENUM_BIT_MASK(FunctionalityMask, Functionality)

struct SubmitBatch {
    struct SubmitEntry {
        uint32_t waitSemaphoreOffset;
        uint32_t waitSemaphoreCount;
        uint32_t signalSemaphoreOffset;
        uint32_t signalSemaphoreCount;
        uint32_t commandBufferOffset;
        uint32_t commandBufferCount;
    };

    ScratchVector<VkSemaphoreHandle> vkWaitSemaphores;
    ScratchVector<VkPipelineStageFlags> vkWaitStageFlags;
    ScratchVector<uint64_t> waitSemaphoreValues;

    ScratchVector<VkSemaphoreHandle> vkSignalSemaphores;
    ScratchVector<uint64_t> signalSemaphoreValues;

    ScratchVector<VkCommandBufferHandle> vkCommandBuffers;

    ScratchVector<SubmitEntry> submitEntries;
};

class LogicalDevice {
public:
    LogicalDevice(Instance* instance, QueueMap* queueMap, const DeviceSetup& setup);

    template <typename TypedHandle>
    void setObjectDebugName(TypedHandle vkHandle, const char* name) const {
        setObjectDebugName(TypedHandle::VkHandleId, reinterpret_cast<uint64_t>(vkHandle.vkRawHandle), name);
    }
    void setObjectDebugName(VkObjectType vkHandleId, uint64_t vkHandle, const char* name) const;

    VkShaderModuleHandle createShaderModule(ArrayParameter<const uint32_t> shaderCode);

    void destroyShaderModule(VkShaderModuleHandle vkShaderModuleHandle) noexcept;

    VkDescriptorSetLayoutHandle createDescriptorSetLayout(ArrayParameter<const DescriptorBinding> descriptorBindings);

    void destroyDescriptorSetLayout(VkDescriptorSetLayoutHandle vkDescriptorSetLayoutHandle) noexcept;

    VkDescriptorUpdateTemplateHandle createDescriptorSetUpdateTemplate(
        VkDescriptorSetLayoutHandle vkDescriptorSetLayoutHandle,
        ArrayParameter<const VkDescriptorUpdateTemplateEntry> entries);

    void destroyDescriptorUpdateTemplate(VkDescriptorUpdateTemplateHandle vkDescriptorUpdateTemplateHandle) noexcept;

    VkDescriptorPoolHandle createDescriptorPool(
        uint32_t maxSets,
        ArrayParameter<const VkDescriptorPoolSize> poolSizes,
        bool updateAfterBind);

    void allocateDescriptorSets(
        VkDescriptorPoolHandle vkDescriptorPoolHandle,
        ArrayParameter<const VkDescriptorSetLayoutHandle> vkDescriptorSetLayoutHandles,
        ArrayView<VkDescriptorSetHandle> vkDescriptorSetHandles);

    void updateDescriptorSet(
        VkDescriptorSetHandle vkDescriptorSetHandle,
        ArrayView<const DescriptorBinding> bindings,
        ArrayParameter<const Descriptor> descriptors);

    void updateDescriptorSetWithTemplate(
        VkDescriptorSetHandle vkDescriptorSetHandle,
        VkDescriptorUpdateTemplateHandle vkDescriptorUpdateTemplateHandle,
        ArrayParameter<const Descriptor> descriptors);

    void resetDescriptorPool(VkDescriptorPoolHandle vkDescriptorPoolHandle);

    void destroyDescriptorPool(VkDescriptorPoolHandle vkDescriptorPoolHandle) noexcept;

    VkPipelineLayoutHandle createPipelineLayout(
        ArrayParameter<const DescriptorSetLayout* const> descriptorSetLayouts,
        ArrayParameter<const PushConstantRange> pushConstantRanges);

    void destroyPipelineLayout(VkPipelineLayoutHandle vkPipelineLayoutHandle) noexcept;

    VkPipelineCacheHandle createPipelineCache(ArrayParameter<const std::byte> data);

    void mergePipelineCaches(
        ArrayParameter<const VkPipelineCacheHandle> vkSrcPipelineCacheHandles,
        VkPipelineCacheHandle vkDstPipelineCacheHandle);

    std::size_t getPipelineCacheData(VkPipelineCacheHandle vkPipelineCacheHandle, ArrayView<std::byte> data) const;

    void destroyPipelineCache(VkPipelineCacheHandle vkPipelineCacheHandle) noexcept;

    void createComputePipelines(
        VkPipelineCacheHandle vkPipelineCacheHandle,
        ArrayParameter<const VkComputePipelineCreateInfo> createInfos,
        ArrayView<VkPipelineHandle> vkPipelineHandles);

    void createGraphicsPipelines(
        VkPipelineCacheHandle vkPipelineCacheHandle,
        ArrayParameter<const VkGraphicsPipelineCreateInfo> createInfos,
        ArrayView<VkPipelineHandle> vkPipelineHandles);

    void destroyPipeline(VkPipelineHandle vkPipelineHandle) noexcept;

    // Memory allocator is responsible for creating buffers, use that instead
    VkBufferHandle createBuffer(const VkBufferCreateInfo& vkCreateInfo);

    void destroyBuffer(VkBufferHandle vkBufferHandle) noexcept;

    VkBufferViewHandle createBufferView(VkBufferHandle vkBufferHandle, uint64_t offset, uint64_t size, Format format);

    void destroyBufferView(VkBufferViewHandle vkBufferViewHandle) noexcept;

    VkDeviceAddress getBufferDeviceAddress(VkBufferHandle vkBufferHandle) noexcept;

    // Memory allocator is responsible for creating images, use that instead
    VkImageHandle createImage(const VkImageCreateInfo& vkCreateInfo);

    void destroyImage(VkImageHandle vkImageHandle) noexcept;

    VkImageViewHandle createImageView(VkImageHandle vkImageHandle, const ImageViewSetup& setup);

    void destroyImageView(VkImageViewHandle vkImageViewHandle) noexcept;

    VkSamplerHandle createSampler(const SamplerSetup& setup);

    void destroySampler(VkSamplerHandle vkSamplerHandle) noexcept;

    VkCommandPoolHandle createCommandPool(QueueType queueType, VkCommandPoolCreateFlagBits flags);

    void destroyCommandPool(VkCommandPoolHandle vkCommandPoolHandle) noexcept;

    VkSemaphoreHandle createSemaphore(bool timelineSemaphore);

    void destroySemaphore(VkSemaphoreHandle vkSemaphoreHandle) noexcept;

    uint64_t getSemaphoreCounterValue(VkSemaphoreHandle vkSemaphoreHandle) const;

    bool waitForSemaphores(
        ArrayParameter<const VkSemaphoreHandle> vkSemaphoreHandles,
        ArrayParameter<const uint64_t> values,
        bool waitAll,
        Timeout timeout) const;

    void signalSemaphore(VkSemaphoreHandle vkSemaphoreHandle, uint64_t value);

    void queueSubmit(uint32_t queueIndex, const SubmitBatch& submitBatch);

    VkQueryPoolHandle createQueryPool(
        VkQueryType queryType,
        VkQueryPipelineStatisticFlagBits pipelineStatistics,
        uint32_t queryCount);

    void destroyQueryPool(VkQueryPoolHandle vkQueryPoolHandle) noexcept;

    VkSwapchainHandleKHR createSwapchainKHR(
        const SwapchainSetup& setup,
        VkSwapchainHandleKHR vkOldSwapchainHandle,
        ScratchVector<VkImageHandle>* vkSwapchainImageHandles);

    void waitForDeviceIdle() const;

    void destroySwapchainKHR(VkSwapchainHandleKHR vkSwapchainHandle) noexcept;

    VkResult acquireNextImageKHR(
        VkSwapchainHandleKHR vkSwapchainHandle,
        Timeout timeout,
        VkSemaphoreHandle vkSemaphoreHandle,
        uint32_t* imageIndex);

    void queuePresentKHR(
        uint32_t queueIndex,
        ArrayParameter<const VkSwapchainHandleKHR> vkSwapchainHandles,
        ArrayParameter<const uint32_t> swapchainImageIndices,
        ArrayParameter<const VkSemaphoreHandle> vkSemaphoreHandles,
        ArrayView<VkResult> vkResults);

    template <typename Interface>
    Interface loadDeviceInterface() const {
        return instance->loadDeviceInterface<Interface>(vkDeviceHandle);
    }

    bool isFunctionalityAvailable(Functionality fun) const {
        return functionalityMask.contains(fun);
    }

    VkDeviceHandle vkGetDeviceHandle() const {
        return vkDeviceHandle;
    }

    TEPHRA_MAKE_NONCOPYABLE(LogicalDevice);
    TEPHRA_MAKE_NONMOVABLE(LogicalDevice);
    ~LogicalDevice();

private:
    VkDeviceHandle vkDeviceHandle;
    VulkanDeviceInterface vkiDevice;
    const Instance* instance;
    const PhysicalDevice* physicalDevice;
    const QueueMap* queueMap;
    FunctionalityMask functionalityMask;

    VulkanSwapchainInterfaceKHR vkiSwapchainKHR;
};

}
