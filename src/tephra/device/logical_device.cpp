
#include "logical_device.hpp"
#include "queue_map.hpp"
#include "../descriptor_pool_impl.hpp"
#include <tephra/descriptor.hpp>

namespace tp {

// Translates user-supplied extensions and features to ones that will actually be used to create the device
inline FunctionalityMask processExtensions(
    Instance* instance,
    VkFeatureMap& vkFeatureMap,
    ScratchVector<const char*>& vkExtensions) {
    // Add own required features - these are guaranteed to be supported
    auto& vk12Features = vkFeatureMap.get<VkPhysicalDeviceVulkan12Features>();
    vk12Features.timelineSemaphore = VK_TRUE;
    auto& vk13Features = vkFeatureMap.get<VkPhysicalDeviceVulkan13Features>();
    vk13Features.dynamicRendering = VK_TRUE;

    // Add implied features and extensions
    if (containsString(view(vkExtensions), DeviceExtension::KHR_AccelerationStructure)) {
        vkFeatureMap.get<VkPhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure = true;
    }

    if (containsString(view(vkExtensions), DeviceExtension::KHR_RayTracingPipeline) &&
        !containsString(view(vkExtensions), VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME))
        vkExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);

    // Store functionality availability for easy access
    FunctionalityMask functionalityMask = {};
    if (instance->isFunctionalityAvailable(InstanceFunctionality::DebugUtilsEXT))
        functionalityMask |= Functionality::DebugUtilsEXT;
    if (containsString(view(vkExtensions), DeviceExtension::EXT_MemoryBudget))
        functionalityMask |= Functionality::MemoryBudgetEXT;
    if (vkFeatureMap.get<VkPhysicalDeviceVulkan12Features>().bufferDeviceAddress)
        functionalityMask |= Functionality::BufferDeviceAddress;

    return functionalityMask;
}

LogicalDevice::LogicalDevice(Instance* instance, QueueMap* queueMap, const DeviceSetup& setup)
    : instance(instance), physicalDevice(setup.physicalDevice), queueMap(queueMap) {
    // Make a copy of feature map and extensiosn so we can make changes to them
    VkFeatureMap vkFeatureMap;
    if (setup.vkFeatureMap != nullptr)
        vkFeatureMap = *setup.vkFeatureMap;

    ScratchVector<const char*> vkExtensions;
    vkExtensions.insert(vkExtensions.begin(), setup.extensions.begin(), setup.extensions.end());
    auto& vk13Features = vkFeatureMap.get<VkPhysicalDeviceVulkan13Features>();
    vk13Features.dynamicRendering = VK_TRUE;

    functionalityMask = processExtensions(instance, vkFeatureMap, vkExtensions);

    // Chain feature structures to extended structure pointer
    void* vkCreateInfoExtPtr = setup.vkCreateInfoExtPtr;
    if (!vkFeatureMap.empty()) {
        vkFeatureMap.back().pNext = vkCreateInfoExtPtr;
        vkCreateInfoExtPtr = &vkFeatureMap.front();
    }

    // Create the logical device
    VulkanDeviceCreateInfo createInfo;
    createInfo.extensions = view(vkExtensions);
    createInfo.queueFamilyCounts = queueMap->getQueueFamilyCounts();
    createInfo.vkCreateInfoExtPtr = vkCreateInfoExtPtr;
    vkDeviceHandle = instance->createVulkanDevice(physicalDevice->vkGetPhysicalDeviceHandle(), createInfo);

    // Load device interfaces
    vkiDevice = instance->loadDeviceInterface<VulkanDeviceInterface>(vkDeviceHandle);

    // Assign Vulkan queue handles
    ScratchVector<VkQueueHandle> vkQueueHandles;
    for (const QueueInfo& queueInfo : queueMap->getQueueInfos()) {
        vkQueueHandles.emplace_back();

        vkiDevice.getDeviceQueue(
            vkDeviceHandle,
            queueInfo.queueFamilyIndex,
            queueInfo.queueIndexInFamily,
            vkCastTypedHandlePtr(&vkQueueHandles.back()));
    }
    queueMap->assignVkQueueHandles(this, view(vkQueueHandles));
}

void LogicalDevice::setObjectDebugName(VkObjectType vkHandleId, uint64_t vkHandle, const char* name) const {
    if (isFunctionalityAvailable(Functionality::DebugUtilsEXT)) {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo;
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.pNext = nullptr;
        objectNameInfo.objectType = vkHandleId;
        objectNameInfo.objectHandle = vkHandle;
        objectNameInfo.pObjectName = name;
        throwRetcodeErrors(vkiDevice.setDebugUtilsObjectNameEXT(vkDeviceHandle, &objectNameInfo));
    }
}

VkShaderModuleHandle LogicalDevice::createShaderModule(ArrayParameter<const uint32_t> shaderCode) {
    VkShaderModuleCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = shaderCode.size() * sizeof(uint32_t);
    createInfo.pCode = shaderCode.data();

    VkShaderModule vkShaderModuleHandle;
    throwRetcodeErrors(vkiDevice.createShaderModule(vkDeviceHandle, &createInfo, nullptr, &vkShaderModuleHandle));
    return VkShaderModuleHandle(vkShaderModuleHandle);
}

void LogicalDevice::destroyShaderModule(VkShaderModuleHandle vkShaderModuleHandle) noexcept {
    vkiDevice.destroyShaderModule(vkDeviceHandle, vkShaderModuleHandle, nullptr);
}

VkDescriptorSetLayoutHandle LogicalDevice::createDescriptorSetLayout(
    ArrayParameter<const DescriptorBinding> descriptorBindings) {
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo;
    bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pNext = nullptr;

    VkDescriptorSetLayoutCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = &bindingFlagsCreateInfo;
    createInfo.flags = 0;

    // Count immutable samplers
    uint64_t immutableSamplerCount = 0;
    for (const DescriptorBinding& binding : descriptorBindings) {
        if (binding.descriptorType != IgnoredDescriptorType) {
            immutableSamplerCount += binding.immutableSamplers.size();
        }
    }

    ScratchVector<VkSamplerHandle> vkSamplerHandles(immutableSamplerCount);

    // Initialize bindings
    ScratchVector<VkDescriptorSetLayoutBinding> vkBindings;
    ScratchVector<VkDescriptorBindingFlags> vkBindingFlags;
    vkBindings.reserve(descriptorBindings.size());

    uint64_t nextImmutableSamplerOffset = 0;
    for (const DescriptorBinding& binding : descriptorBindings) {
        if (binding.descriptorType != IgnoredDescriptorType) {
            VkDescriptorSetLayoutBinding vkBinding;
            vkBinding.binding = binding.bindingNumber;
            vkBinding.descriptorType = vkCastConvertibleEnum(binding.descriptorType);
            vkBinding.stageFlags = static_cast<uint32_t>(binding.stageMask);
            vkBinding.descriptorCount = binding.arraySize;
            vkBinding.pImmutableSamplers = nullptr;

            if (!binding.immutableSamplers.empty()) {
                vkBinding.pImmutableSamplers = vkCastTypedHandlePtr(&vkSamplerHandles[nextImmutableSamplerOffset]);
                for (const Sampler* sampler : binding.immutableSamplers) {
                    vkSamplerHandles[nextImmutableSamplerOffset++] = sampler->vkGetSamplerHandle();
                }
            }

            vkBindings.push_back(vkBinding);

            // also set up flags
            VkDescriptorBindingFlags flags = 0;
            if (binding.flags.contains(tp::DescriptorBindingFlag::PartiallyBound)) {
                flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
            if (binding.flags.contains(tp::DescriptorBindingFlag::VariableDescriptorCount)) {
                flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
            if (binding.flags.contains(tp::DescriptorBindingFlag::UpdateAfterBind)) {
                flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
            }
            vkBindingFlags.push_back(flags);
        }
    }

    createInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    createInfo.pBindings = vkBindings.data();

    bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(vkBindingFlags.size());
    bindingFlagsCreateInfo.pBindingFlags = vkBindingFlags.data();

    VkDescriptorSetLayout vkDescriptorSetLayoutHandle;
    throwRetcodeErrors(
        vkiDevice.createDescriptorSetLayout(vkDeviceHandle, &createInfo, nullptr, &vkDescriptorSetLayoutHandle));
    return VkDescriptorSetLayoutHandle(vkDescriptorSetLayoutHandle);
}

void LogicalDevice::destroyDescriptorSetLayout(VkDescriptorSetLayoutHandle vkDescriptorSetLayoutHandle) noexcept {
    vkiDevice.destroyDescriptorSetLayout(vkDeviceHandle, vkDescriptorSetLayoutHandle, nullptr);
}

VkDescriptorUpdateTemplateHandle LogicalDevice::createDescriptorSetUpdateTemplate(
    VkDescriptorSetLayoutHandle vkDescriptorSetLayoutHandle,
    ArrayParameter<const VkDescriptorUpdateTemplateEntry> entries) {
    VkDescriptorUpdateTemplateCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
    createInfo.pDescriptorUpdateEntries = entries.data();
    createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    createInfo.descriptorSetLayout = vkDescriptorSetLayoutHandle;
    createInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM; // Unused
    createInfo.pipelineLayout = VK_NULL_HANDLE; // Unused
    createInfo.set = ~0; // Unused

    VkDescriptorUpdateTemplate vkDescriptorUpdateTemplateHandle;
    throwRetcodeErrors(vkiDevice.createDescriptorUpdateTemplate(
        vkDeviceHandle, &createInfo, nullptr, &vkDescriptorUpdateTemplateHandle));
    return VkDescriptorUpdateTemplateHandle(vkDescriptorUpdateTemplateHandle);
}

void LogicalDevice::destroyDescriptorUpdateTemplate(
    VkDescriptorUpdateTemplateHandle vkDescriptorUpdateTemplateHandle) noexcept {
    vkiDevice.destroyDescriptorUpdateTemplate(vkDeviceHandle, vkDescriptorUpdateTemplateHandle, nullptr);
}

VkDescriptorPoolHandle LogicalDevice::createDescriptorPool(
    uint32_t maxSets,
    ArrayParameter<const VkDescriptorPoolSize> poolSizes) {
    VkDescriptorPoolCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.maxSets = maxSets;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool vkDescriptorPoolHandle;
    throwRetcodeErrors(vkiDevice.createDescriptorPool(vkDeviceHandle, &createInfo, nullptr, &vkDescriptorPoolHandle));
    return VkDescriptorPoolHandle(vkDescriptorPoolHandle);
}

void LogicalDevice::allocateDescriptorSets(
    VkDescriptorPoolHandle vkDescriptorPoolHandle,
    ArrayParameter<const VkDescriptorSetLayoutHandle> vkDescriptorSetLayoutHandles,
    ArrayView<VkDescriptorSetHandle> vkDescriptorSetHandles) {
    TEPHRA_ASSERT(vkDescriptorSetLayoutHandles.size() == vkDescriptorSetHandles.size());

    VkDescriptorSetAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.descriptorPool = vkDescriptorPoolHandle;
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(vkDescriptorSetLayoutHandles.size());
    allocateInfo.pSetLayouts = vkCastTypedHandlePtr(vkDescriptorSetLayoutHandles.data());

    throwRetcodeErrors(vkiDevice.allocateDescriptorSets(
        vkDeviceHandle, &allocateInfo, vkCastTypedHandlePtr(vkDescriptorSetHandles.data())));
}

void LogicalDevice::updateDescriptorSet(
    VkDescriptorSetHandle vkDescriptorSetHandle,
    ArrayView<const DescriptorBinding> bindings,
    ArrayParameter<const Descriptor> descriptors) {
    ScratchVector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(descriptors.size());

    int descriptorIndex = 0;
    for (const DescriptorBinding& binding : bindings) {
        VkWriteDescriptorSet descWrite;
        descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite.pNext = nullptr;
        descWrite.dstSet = vkDescriptorSetHandle;
        descWrite.dstBinding = binding.bindingNumber;

        // TODO: Currently one descriptor per write - is this any slower?
        descWrite.descriptorCount = 1;
        descWrite.descriptorType = vkCastConvertibleEnum(binding.descriptorType);

        TEPHRA_ASSERT(descriptors.size() >= descriptorIndex + binding.arraySize);
        for (uint32_t i = 0; i < binding.arraySize; i++) {
            descWrite.dstArrayElement = i;
            const Descriptor& descriptor = descriptors[descriptorIndex++];

            if (!descriptor.isNull()) {
                descWrite.pImageInfo = descriptor.vkResolveDescriptorImageInfo();
                descWrite.pBufferInfo = descriptor.vkResolveDescriptorBufferInfo();
                descWrite.pTexelBufferView = descriptor.vkResolveDescriptorBufferViewHandle();
                descriptorWrites.push_back(descWrite);
            }
        }
    }

    vkiDevice.updateDescriptorSets(
        vkDeviceHandle, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void LogicalDevice::updateDescriptorSetWithTemplate(
    VkDescriptorSetHandle vkDescriptorSetHandle,
    VkDescriptorUpdateTemplateHandle vkDescriptorUpdateTemplateHandle,
    ArrayParameter<const Descriptor> descriptors) {
    const void* descriptorData = static_cast<const void*>(descriptors.data());
    vkiDevice.updateDescriptorSetWithTemplate(
        vkDeviceHandle, vkDescriptorSetHandle, vkDescriptorUpdateTemplateHandle, descriptorData);
}

void LogicalDevice::resetDescriptorPool(VkDescriptorPoolHandle vkDescriptorPoolHandle) {
    throwRetcodeErrors(vkiDevice.resetDescriptorPool(vkDeviceHandle, vkDescriptorPoolHandle, 0));
}

void LogicalDevice::destroyDescriptorPool(VkDescriptorPoolHandle vkDescriptorPoolHandle) noexcept {
    vkiDevice.destroyDescriptorPool(vkDeviceHandle, vkDescriptorPoolHandle, nullptr);
}

VkPipelineLayoutHandle LogicalDevice::createPipelineLayout(
    ArrayParameter<const DescriptorSetLayout* const> descriptorSetLayouts,
    ArrayParameter<const PushConstantRange> pushConstantRanges) {
    VkPipelineLayoutCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;

    ScratchVector<VkDescriptorSetLayoutHandle> layouts(descriptorSetLayouts.size());
    for (std::size_t i = 0; i < descriptorSetLayouts.size(); i++) {
        layouts[i] = descriptorSetLayouts[i]->vkGetDescriptorSetLayoutHandle();
    }
    createInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    createInfo.pSetLayouts = vkCastTypedHandlePtr(layouts.data());
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges = static_cast<const VkPushConstantRange*>(pushConstantRanges.data());

    VkPipelineLayout vkPipelineLayoutHandle;
    throwRetcodeErrors(vkiDevice.createPipelineLayout(vkDeviceHandle, &createInfo, nullptr, &vkPipelineLayoutHandle));
    return VkPipelineLayoutHandle(vkPipelineLayoutHandle);
}

void LogicalDevice::destroyPipelineLayout(VkPipelineLayoutHandle vkPipelineLayoutHandle) noexcept {
    vkiDevice.destroyPipelineLayout(vkDeviceHandle, vkPipelineLayoutHandle, nullptr);
}

VkPipelineCacheHandle LogicalDevice::createPipelineCache(ArrayParameter<const std::byte> data) {
    VkPipelineCacheCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.initialDataSize = static_cast<uint32_t>(data.size());
    createInfo.pInitialData = data.data();

    VkPipelineCache vkPipelineCacheHandle;
    throwRetcodeErrors(vkiDevice.createPipelineCache(vkDeviceHandle, &createInfo, nullptr, &vkPipelineCacheHandle));
    return VkPipelineCacheHandle(vkPipelineCacheHandle);
}

void LogicalDevice::mergePipelineCaches(
    ArrayParameter<const VkPipelineCacheHandle> vkSrcPipelineCacheHandles,
    VkPipelineCacheHandle vkDstPipelineCacheHandle) {
    throwRetcodeErrors(vkiDevice.mergePipelineCaches(
        vkDeviceHandle,
        vkDstPipelineCacheHandle,
        static_cast<uint32_t>(vkSrcPipelineCacheHandles.size()),
        vkCastTypedHandlePtr(vkSrcPipelineCacheHandles.data())));
}

std::size_t LogicalDevice::getPipelineCacheData(VkPipelineCacheHandle vkPipelineCacheHandle, ArrayView<std::byte> data)
    const {
    std::size_t dataSize = data.size();
    throwRetcodeErrors(vkiDevice.getPipelineCacheData(vkDeviceHandle, vkPipelineCacheHandle, &dataSize, data.data()));
    return dataSize;
}

void LogicalDevice::destroyPipelineCache(VkPipelineCacheHandle vkPipelineCacheHandle) noexcept {
    vkiDevice.destroyPipelineCache(vkDeviceHandle, vkPipelineCacheHandle, nullptr);
}

void LogicalDevice::createComputePipelines(
    VkPipelineCacheHandle vkPipelineCacheHandle,
    ArrayParameter<const VkComputePipelineCreateInfo> createInfos,
    ArrayView<VkPipelineHandle> vkPipelineHandles) {
    throwRetcodeErrors(vkiDevice.createComputePipelines(
        vkDeviceHandle,
        vkPipelineCacheHandle,
        static_cast<uint32_t>(createInfos.size()),
        createInfos.data(),
        nullptr,
        vkCastTypedHandlePtr(vkPipelineHandles.data())));
}

void LogicalDevice::createGraphicsPipelines(
    VkPipelineCacheHandle vkPipelineCacheHandle,
    ArrayParameter<const VkGraphicsPipelineCreateInfo> createInfos,
    ArrayView<VkPipelineHandle> vkPipelineHandles) {
    throwRetcodeErrors(vkiDevice.createGraphicsPipelines(
        vkDeviceHandle,
        vkPipelineCacheHandle,
        static_cast<uint32_t>(createInfos.size()),
        createInfos.data(),
        nullptr,
        vkCastTypedHandlePtr(vkPipelineHandles.data())));
}

void LogicalDevice::destroyPipeline(VkPipelineHandle vkPipelineHandle) noexcept {
    vkiDevice.destroyPipeline(vkDeviceHandle, vkPipelineHandle, nullptr);
}

VkBufferHandle LogicalDevice::createBuffer(const VkBufferCreateInfo& vkCreateInfo) {
    VkBuffer vkBufferHandle;
    throwRetcodeErrors(vkiDevice.createBuffer(vkDeviceHandle, &vkCreateInfo, nullptr, &vkBufferHandle));
    return VkBufferHandle(vkBufferHandle);
}

void LogicalDevice::destroyBuffer(VkBufferHandle vkBufferHandle) noexcept {
    vkiDevice.destroyBuffer(vkDeviceHandle, vkBufferHandle, nullptr);
}

VkBufferViewHandle LogicalDevice::createBufferView(
    VkBufferHandle vkBufferHandle,
    uint64_t offset,
    uint64_t size,
    Format format) {
    VkBufferViewCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.buffer = vkBufferHandle;
    createInfo.format = vkCastConvertibleEnum(format);
    createInfo.offset = offset;
    createInfo.range = size;

    VkBufferView vkBufferViewHandle;
    throwRetcodeErrors(vkiDevice.createBufferView(vkDeviceHandle, &createInfo, nullptr, &vkBufferViewHandle));
    return VkBufferViewHandle(vkBufferViewHandle);
}

void LogicalDevice::destroyBufferView(VkBufferViewHandle vkBufferViewHandle) noexcept {
    vkiDevice.destroyBufferView(vkDeviceHandle, vkBufferViewHandle, nullptr);
}

DeviceAddress LogicalDevice::getBufferDeviceAddress(VkBufferHandle vkBufferHandle) noexcept {
    VkBufferDeviceAddressInfo addressInfo;
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.pNext = nullptr;
    addressInfo.buffer = vkBufferHandle;
    return vkiDevice.getBufferDeviceAddress(vkDeviceHandle, &addressInfo);
}

VkImageHandle LogicalDevice::createImage(const VkImageCreateInfo& vkCreateInfo) {
    VkImage vkImageHandle;
    throwRetcodeErrors(vkiDevice.createImage(vkDeviceHandle, &vkCreateInfo, nullptr, &vkImageHandle));
    return VkImageHandle(vkImageHandle);
}

void LogicalDevice::destroyImage(VkImageHandle vkImageHandle) noexcept {
    vkiDevice.destroyImage(vkDeviceHandle, vkImageHandle, nullptr);
}

VkImageViewHandle LogicalDevice::createImageView(VkImageHandle vkImageHandle, const ImageViewSetup& setup) {
    VkImageViewCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.image = vkImageHandle;
    createInfo.viewType = vkCastConvertibleEnum(setup.viewType);
    createInfo.format = vkCastConvertibleEnum(setup.format);
    createInfo.components = vkCastConvertibleStruct(setup.componentMapping);
    createInfo.subresourceRange = vkCastConvertibleStruct(setup.subresourceRange);

    VkImageView vkImageViewHandle;
    throwRetcodeErrors(vkiDevice.createImageView(vkDeviceHandle, &createInfo, nullptr, &vkImageViewHandle));
    return VkImageViewHandle(vkImageViewHandle);
}

void LogicalDevice::destroyImageView(VkImageViewHandle vkImageViewHandle) noexcept {
    vkiDevice.destroyImageView(vkDeviceHandle, vkImageViewHandle, nullptr);
}

VkSamplerHandle LogicalDevice::createSampler(const SamplerSetup& setup) {
    VkSamplerCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.magFilter = vkCastConvertibleEnum(setup.filtering.magFilter);
    createInfo.minFilter = vkCastConvertibleEnum(setup.filtering.minFilter);
    createInfo.mipmapMode = setup.filtering.mipmapFilter == Filter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST :
                                                                              VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.addressModeU = vkCastConvertibleEnum(setup.addressing.addressModeU);
    createInfo.addressModeV = vkCastConvertibleEnum(setup.addressing.addressModeV);
    createInfo.addressModeW = vkCastConvertibleEnum(setup.addressing.addressModeW);
    createInfo.mipLodBias = setup.mipLodBias;
    createInfo.anisotropyEnable = setup.maxAnisotropy > 1.0f;
    createInfo.maxAnisotropy = setup.maxAnisotropy;
    createInfo.compareEnable = setup.compareEnable;
    createInfo.compareOp = vkCastConvertibleEnum(setup.compareOp);
    createInfo.minLod = setup.minMipLod;
    createInfo.maxLod = setup.maxMipLod;
    createInfo.borderColor = vkCastConvertibleEnum(setup.addressing.borderColor);
    createInfo.unnormalizedCoordinates = setup.unnormalizedCoordinates;

    VkSampler vkSamplerHandle;
    throwRetcodeErrors(vkiDevice.createSampler(vkDeviceHandle, &createInfo, nullptr, &vkSamplerHandle));
    return VkSamplerHandle(vkSamplerHandle);
}

void LogicalDevice::destroySampler(VkSamplerHandle vkSamplerHandle) noexcept {
    vkiDevice.destroySampler(vkDeviceHandle, vkSamplerHandle, nullptr);
}

VkCommandPoolHandle LogicalDevice::createCommandPool(QueueType queueType, VkCommandPoolCreateFlagBits flags) {
    VkCommandPoolCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = flags;
    createInfo.queueFamilyIndex = queueMap->getQueueInfo(queueType).queueFamilyIndex;

    VkCommandPool vkCommandPoolHandle;
    throwRetcodeErrors(vkiDevice.createCommandPool(vkDeviceHandle, &createInfo, nullptr, &vkCommandPoolHandle));
    return VkCommandPoolHandle(vkCommandPoolHandle);
}

void LogicalDevice::destroyCommandPool(VkCommandPoolHandle vkCommandPoolHandle) noexcept {
    vkiDevice.destroyCommandPool(vkDeviceHandle, vkCommandPoolHandle, nullptr);
}

VkSemaphoreHandle LogicalDevice::createSemaphore(bool timelineSemaphore) {
    VkSemaphoreTypeCreateInfo typeInfo;
    typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    typeInfo.pNext = nullptr;
    typeInfo.semaphoreType = timelineSemaphore ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY;
    typeInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &typeInfo;
    createInfo.flags = 0;

    VkSemaphore vkSemaphoreHandle;
    throwRetcodeErrors(vkiDevice.createSemaphore(vkDeviceHandle, &createInfo, nullptr, &vkSemaphoreHandle));
    return VkSemaphoreHandle(vkSemaphoreHandle);
}

void LogicalDevice::destroySemaphore(VkSemaphoreHandle vkSemaphoreHandle) noexcept {
    vkiDevice.destroySemaphore(vkDeviceHandle, vkSemaphoreHandle, nullptr);
}

uint64_t LogicalDevice::getSemaphoreCounterValue(VkSemaphoreHandle vkSemaphoreHandle) const {
    uint64_t value;
    throwRetcodeErrors(vkiDevice.getSemaphoreCounterValue(vkDeviceHandle, vkSemaphoreHandle, &value));

    return value;
}

bool LogicalDevice::waitForSemaphores(
    ArrayParameter<const VkSemaphoreHandle> vkSemaphoreHandles,
    ArrayParameter<const uint64_t> values,
    bool waitAll,
    Timeout timeout) const {
    VkSemaphoreWaitInfo waitInfo;
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.pNext = nullptr;
    waitInfo.flags = waitAll ? 0 : VK_SEMAPHORE_WAIT_ANY_BIT;
    waitInfo.semaphoreCount = static_cast<uint32_t>(vkSemaphoreHandles.size());
    waitInfo.pSemaphores = vkCastTypedHandlePtr(vkSemaphoreHandles.data());
    waitInfo.pValues = values.data();

    VkResult result = throwRetcodeErrors(vkiDevice.waitSemaphores(vkDeviceHandle, &waitInfo, timeout.nanoseconds));
    return result != VK_TIMEOUT;
}

void LogicalDevice::signalSemaphore(VkSemaphoreHandle vkSemaphoreHandle, uint64_t value) {
    VkSemaphoreSignalInfo signalInfo;
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.pNext = nullptr;
    signalInfo.semaphore = vkSemaphoreHandle;
    signalInfo.value = value;

    throwRetcodeErrors(vkiDevice.signalSemaphore(vkDeviceHandle, &signalInfo));
}

void LogicalDevice::queueSubmit(uint32_t queueIndex, const SubmitBatch& submitBatch) {
    ScratchVector<VkTimelineSemaphoreSubmitInfo> vkSemaphoreSubmitInfos;
    vkSemaphoreSubmitInfos.reserve(submitBatch.submitEntries.size());
    ScratchVector<VkSubmitInfo> vkSubmitInfos;
    vkSubmitInfos.reserve(submitBatch.submitEntries.size());

    for (const SubmitBatch::SubmitEntry& entry : submitBatch.submitEntries) {
        vkSemaphoreSubmitInfos.emplace_back();
        VkTimelineSemaphoreSubmitInfo& semaphoreSubmitInfo = vkSemaphoreSubmitInfos.back();

        semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfo.pNext = nullptr;
        semaphoreSubmitInfo.waitSemaphoreValueCount = entry.waitSemaphoreCount;
        semaphoreSubmitInfo.pWaitSemaphoreValues = submitBatch.waitSemaphoreValues.data() + entry.waitSemaphoreOffset;
        semaphoreSubmitInfo.signalSemaphoreValueCount = entry.signalSemaphoreCount;
        semaphoreSubmitInfo.pSignalSemaphoreValues = submitBatch.signalSemaphoreValues.data() +
            entry.signalSemaphoreOffset;

        vkSubmitInfos.emplace_back();
        VkSubmitInfo& submitInfo = vkSubmitInfos.back();

        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &semaphoreSubmitInfo;
        submitInfo.waitSemaphoreCount = entry.waitSemaphoreCount;
        submitInfo.pWaitSemaphores = vkCastTypedHandlePtr(
            submitBatch.vkWaitSemaphores.data() + entry.waitSemaphoreOffset);
        submitInfo.pWaitDstStageMask = submitBatch.vkWaitStageFlags.data() + entry.waitSemaphoreOffset;
        submitInfo.commandBufferCount = entry.commandBufferCount;
        submitInfo.pCommandBuffers = vkCastTypedHandlePtr(
            submitBatch.vkCommandBuffers.data() + entry.commandBufferOffset);
        submitInfo.signalSemaphoreCount = entry.signalSemaphoreCount;
        submitInfo.pSignalSemaphores = vkCastTypedHandlePtr(
            submitBatch.vkSignalSemaphores.data() + entry.signalSemaphoreOffset);
    }

    // Lock Vulkan queue, if shared with other Tephra queues
    TEPHRA_ASSERT(queueIndex != ~0);
    const QueueInfo& queueInfo = queueMap->getQueueInfos()[queueIndex];
    std::unique_lock<Mutex> lock;
    if (queueInfo.queueHandleMutex != nullptr)
        lock = std::unique_lock<Mutex>(*queueInfo.queueHandleMutex);

    throwRetcodeErrors(vkiDevice.queueSubmit(
        queueInfo.vkQueueHandle, static_cast<uint32_t>(vkSubmitInfos.size()), vkSubmitInfos.data(), VK_NULL_HANDLE));
}

VkSwapchainHandleKHR LogicalDevice::createSwapchainKHR(
    const SwapchainSetup& setup,
    VkSwapchainHandleKHR vkOldSwapchainHandle,
    ScratchVector<VkImageHandle>* vkSwapchainImageHandles) {
    if (vkiDevice.createSwapchainKHR == nullptr) {
        throw UnsupportedOperationError(
            "Functionality of the KHR_Swapchain extension is being used, but its interface could not be loaded. Has it "
            "been enabled?");
    }

    // Cast the pre-transform. This is generally unsafe because we use an extra value, but
    // here we take care of it specially
    VkSurfaceTransformFlagBitsKHR vkSurfaceTransform;
    if (setup.preTransform == SurfaceTransform::UseCurrentTransform) {
        SurfaceCapabilities capabilities = physicalDevice->querySurfaceCapabilitiesKHR(setup.vkSurfaceHandle);
        vkSurfaceTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(capabilities.currentTransform);
    } else {
        vkSurfaceTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(setup.preTransform);
    }

    VkSwapchainCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = setup.vkCreateInfoExtPtr;
    createInfo.flags = vkCastConvertibleEnumMask(setup.flags);
    createInfo.surface = setup.vkSurfaceHandle;
    createInfo.minImageCount = setup.minImageCount;
    createInfo.imageFormat = vkCastConvertibleEnum(setup.imageFormat);
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = setup.imageExtent;
    createInfo.imageArrayLayers = setup.imageArrayLayerCount;
    createInfo.imageUsage = vkCastConvertibleEnumMask(setup.imageUsage);
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = vkSurfaceTransform;
    createInfo.compositeAlpha = vkCastConvertibleEnum(setup.compositeAlpha);
    createInfo.presentMode = vkCastConvertibleEnum(setup.presentMode);
    createInfo.clipped = setup.clipped;
    createInfo.oldSwapchain = vkOldSwapchainHandle;

    const void** nextExtPtr = &createInfo.pNext;

    // Add additional view compatible formats
    ScratchVector<VkFormat> compatibleFormats;
    VkImageFormatListCreateInfo formatListCreateInfo;
    if (!setup.imageCompatibleFormatsKHR.empty()) {
        compatibleFormats.reserve(setup.imageCompatibleFormatsKHR.size() + 1);

        bool hadImageFormat = false;
        for (Format format : setup.imageCompatibleFormatsKHR) {
            compatibleFormats.push_back(vkCastConvertibleEnum(format));
            if (format == setup.imageFormat)
                hadImageFormat = true;
        }

        // Make sure the image's own format is in the compatible list
        if (!hadImageFormat)
            compatibleFormats.push_back(createInfo.imageFormat);

        formatListCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
        formatListCreateInfo.pNext = nullptr;
        formatListCreateInfo.viewFormatCount = static_cast<uint32_t>(compatibleFormats.size());
        formatListCreateInfo.pViewFormats = compatibleFormats.data();
        *nextExtPtr = &formatListCreateInfo;
        nextExtPtr = &formatListCreateInfo.pNext;
        createInfo.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;
    }

    VkSwapchainKHR vkSwapchainHandle;
    throwRetcodeErrors(vkiDevice.createSwapchainKHR(vkDeviceHandle, &createInfo, nullptr, &vkSwapchainHandle));

    // Get swapchain image handles
    uint32_t count;
    throwRetcodeErrors(vkiDevice.getSwapchainImagesKHR(vkDeviceHandle, vkSwapchainHandle, &count, nullptr));
    vkSwapchainImageHandles->resize(count);
    throwRetcodeErrors(vkiDevice.getSwapchainImagesKHR(
        vkDeviceHandle, vkSwapchainHandle, &count, vkCastTypedHandlePtr(vkSwapchainImageHandles->data())));
    vkSwapchainImageHandles->resize(count);

    return VkSwapchainHandleKHR(vkSwapchainHandle);
}

void LogicalDevice::waitForDeviceIdle() const {
    throwRetcodeErrors(vkiDevice.deviceWaitIdle(vkDeviceHandle));
}

void LogicalDevice::destroySwapchainKHR(VkSwapchainHandleKHR vkSwapchainHandle) noexcept {
    vkiDevice.destroySwapchainKHR(vkDeviceHandle, vkSwapchainHandle, nullptr);
}

VkResult LogicalDevice::acquireNextImageKHR(
    VkSwapchainHandleKHR vkSwapchainHandle,
    Timeout timeout,
    VkSemaphoreHandle vkSemaphoreHandle,
    uint32_t* imageIndex) {
    return throwRetcodeErrors(vkiDevice.acquireNextImageKHR(
        vkDeviceHandle, vkSwapchainHandle, timeout.nanoseconds, vkSemaphoreHandle, VK_NULL_HANDLE, imageIndex));
}

void LogicalDevice::queuePresentKHR(
    uint32_t queueIndex,
    ArrayParameter<const VkSwapchainHandleKHR> vkSwapchainHandles,
    ArrayParameter<const uint32_t> swapchainImageIndices,
    ArrayParameter<const VkSemaphoreHandle> vkSemaphoreHandles,
    ArrayView<VkResult> vkResults) {
    TEPHRA_ASSERT(vkSwapchainHandles.size() == swapchainImageIndices.size());
    TEPHRA_ASSERT(vkSwapchainHandles.size() == vkResults.size());

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vkSemaphoreHandles.size());
    presentInfo.pWaitSemaphores = vkCastTypedHandlePtr(vkSemaphoreHandles.data());
    presentInfo.swapchainCount = static_cast<uint32_t>(vkSwapchainHandles.size());
    presentInfo.pSwapchains = vkCastTypedHandlePtr(vkSwapchainHandles.data());
    presentInfo.pImageIndices = swapchainImageIndices.data();
    presentInfo.pResults = vkResults.data();

    // Lock Vulkan queue, if shared with other Tephra queues
    TEPHRA_ASSERT(queueIndex != ~0);
    const QueueInfo& queueInfo = queueMap->getQueueInfos()[queueIndex];
    std::unique_lock<Mutex> lock;
    if (queueInfo.queueHandleMutex != nullptr)
        lock = std::unique_lock<Mutex>(*queueInfo.queueHandleMutex);

    throwRetcodeErrors(vkiDevice.queuePresentKHR(queueInfo.vkQueueHandle, &presentInfo));
}

VkAccelerationStructureKHR LogicalDevice::createAccelerationStructureKHR(
    const BufferView& buffer,
    AccelerationStructureType type) {
    if (vkiDevice.createAccelerationStructureKHR == nullptr) {
        throw UnsupportedOperationError(
            "Functionality of the KHR_AccelerationStructure extension is being used, but its interface could not be "
            "loaded. Has it been enabled?");
    }

    VkAccelerationStructureCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.createFlags = 0;
    createInfo.buffer = buffer.vkResolveBufferHandle(&createInfo.offset);
    createInfo.size = buffer.getSize();
    createInfo.type = vkCastConvertibleEnum(type);
    createInfo.deviceAddress = 0;

    VkAccelerationStructureKHR vkAccelerationHandle;
    throwRetcodeErrors(
        vkiDevice.createAccelerationStructureKHR(vkDeviceHandle, &createInfo, nullptr, &vkAccelerationHandle));
    return vkAccelerationHandle;
}

void LogicalDevice::destroyAccelerationStructureKHR(VkAccelerationStructureKHR vkAccelerationStructureHandle) noexcept {
    vkiDevice.destroyAccelerationStructureKHR(vkDeviceHandle, vkAccelerationStructureHandle, nullptr);
}

VkAccelerationStructureBuildSizesInfoKHR LogicalDevice::getAccelerationStructureBuildSizes(
    const VkAccelerationStructureBuildGeometryInfoKHR& vkBuildInfo,
    const uint32_t* pMaxPrimitiveCounts) {
    // Unlike other functions, it is more convenient to create the structure elsewhere, so here we just wrap
    // Our AS are always device-only
    VkAccelerationStructureBuildTypeKHR buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
    vkiDevice.getAccelerationStructureBuildSizesKHR(
        vkDeviceHandle, buildType, &vkBuildInfo, pMaxPrimitiveCounts, &sizeInfo);
    return sizeInfo;
}

LogicalDevice::~LogicalDevice() {
    if (!vkDeviceHandle.isNull()) {
        throwRetcodeErrors(vkiDevice.deviceWaitIdle(vkDeviceHandle));
        vkiDevice.destroyDevice(vkDeviceHandle, nullptr);
        vkDeviceHandle = {};
    }
}

}
