
#include "device_container.hpp"
#include "../application/application_container.hpp"
#include "../job/resource_pool_container.hpp"
#include "../job/render_pass.hpp"
#include "../buffer_impl.hpp"
#include "../image_impl.hpp"
#include "../acceleration_structure_impl.hpp"
#include "../pipeline_builder.hpp"
#include "../descriptor_pool_impl.hpp"
#include "../swapchain_impl.hpp"
#include "../common_impl.hpp"
#include <tephra/device.hpp>
#include <tephra/application.hpp>

namespace tp {

constexpr const char* DeviceTypeName = "Device";
constexpr const char* BufferTypeName = "Buffer";
constexpr const char* ImageTypeName = "Image";
constexpr const char* AccelerationStructureTypeName = "AccelerationStructure";
constexpr const char* DescriptorPoolTypeName = "DescriptorPool";
constexpr const char* JobResourcePoolTypeName = "JobResourcePool";
constexpr const char* SwapchainTypeName = "Swapchain";

MemoryAllocatorSetup::MemoryAllocatorSetup(
    uint64_t preferredLargeHeapBlockSize,
    VmaDeviceMemoryCallbacks* vmaDeviceMemoryCallbacks)
    : preferredLargeHeapBlockSize(preferredLargeHeapBlockSize), vmaDeviceMemoryCallbacks(vmaDeviceMemoryCallbacks) {}

DeviceSetup::DeviceSetup(
    const PhysicalDevice* physicalDevice,
    ArrayView<const DeviceQueue> queues,
    ArrayView<const char* const> extensions,
    const VkFeatureMap* vkFeatureMap,
    MemoryAllocatorSetup memoryAllocatorSetup,
    void* vkCreateInfoExtPtr)
    : physicalDevice(physicalDevice),
      queues(queues),
      extensions(extensions),
      vkFeatureMap(vkFeatureMap),
      memoryAllocatorSetup(memoryAllocatorSetup),
      vkCreateInfoExtPtr(vkCreateInfoExtPtr) {}

void validateRequestedDeviceQueues(const DeviceSetup& deviceSetup) {
    // Validate queue support
    uint32_t queueCounts[QueueTypeEnumView::size()] = { 0 };
    for (DeviceQueue queue : deviceSetup.queues) {
        queueCounts[static_cast<uint32_t>(queue.type)]++;

        if (queue.isNull()) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "Requested queue cannot be null.");
        }
        if (queue.type == QueueType::External) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "Requested queue type cannot be QueueType::External.");
        }
    }

    if (queueCounts[static_cast<uint32_t>(QueueType::Graphics)] != 0) {
        QueueTypeInfo queueTypeInfo = deviceSetup.physicalDevice->getQueueTypeInfo(QueueType::Graphics);
        if (queueTypeInfo.queueCount == 0) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "Physical device does not support graphics queues, but one was requested.");
        }
    }

    for (QueueType queueType : QueueTypeEnumView()) {
        uint32_t requestedCount = queueCounts[static_cast<uint32_t>(queueType)];
        uint32_t exposedCount = deviceSetup.physicalDevice->getQueueTypeInfo(queueType).queueCount;
        if (requestedCount > exposedCount) {
            reportDebugMessage(
                DebugMessageSeverity::Warning,
                DebugMessageType::Performance,
                "More queues of type ",
                getDeviceQueueTypeName(queueType),
                " were requested (",
                requestedCount,
                ") than how many are exposed by the physical device (",
                exposedCount,
                "). This is OK, but may result in queue contention.");
        }
    }

    // Extensions and features will be validated by Vulkan validation layers
}

OwningPtr<Device> Application::createDevice(const DeviceSetup& deviceSetup, const char* debugName) {
    auto appContainer = static_cast<ApplicationContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(appContainer->getDebugTarget(), "createDevice", debugName);
    if constexpr (TephraValidationEnabled) {
        if (deviceSetup.physicalDevice == nullptr) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The 'deviceSetup.physicalDevice' parameter is nullptr.");
        }

        validateRequestedDeviceQueues(deviceSetup);
    }

    auto debugTarget = DebugTarget(appContainer->getDebugTarget(), DeviceTypeName, debugName);
    return OwningPtr<Device>(new DeviceContainer(appContainer, deviceSetup, std::move(debugTarget)));
}

ShaderModule Device::createShaderModule(ArrayParameter<const uint32_t> shaderCode, const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createShaderModule", debugName);

    VkShaderModuleHandle vkHandle = deviceImpl->getLogicalDevice()->createShaderModule(shaderCode);
    auto shaderModule = ShaderModule(vkMakeHandleLifeguard(vkHandle));

    deviceImpl->getLogicalDevice()->setObjectDebugName(shaderModule.vkGetShaderModuleHandle(), debugName);

    return shaderModule;
}

Sampler Device::createSampler(const SamplerSetup& setup, const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createSampler", debugName);

    VkSamplerHandle vkHandle = deviceImpl->getLogicalDevice()->createSampler(setup);
    auto sampler = Sampler(vkMakeHandleLifeguard(vkHandle));

    deviceImpl->getLogicalDevice()->setObjectDebugName(sampler.vkGetSamplerHandle(), debugName);

    return sampler;
}

DescriptorSetLayout Device::createDescriptorSetLayout(
    ArrayParameter<const DescriptorBinding> descriptorBindings,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createDescriptorSetLayout", debugName);

    VkDescriptorSetLayoutHandle vkHandle = deviceImpl->getLogicalDevice()->createDescriptorSetLayout(
        descriptorBindings);
    ScratchVector<VkDescriptorUpdateTemplateEntry> updateTemplateEntries;
    DescriptorPoolImpl::makeUpdateTemplate(descriptorBindings, &updateTemplateEntries);
    VkDescriptorUpdateTemplateHandle vkUpdateTemplateHandle =
        deviceImpl->getLogicalDevice()->createDescriptorSetUpdateTemplate(vkHandle, view(updateTemplateEntries));

    auto descriptorSetLayout = DescriptorSetLayout(
        vkMakeHandleLifeguard(vkHandle), vkMakeHandleLifeguard(vkUpdateTemplateHandle), descriptorBindings);

    deviceImpl->getLogicalDevice()->setObjectDebugName(descriptorSetLayout.vkGetDescriptorSetLayoutHandle(), debugName);

    return descriptorSetLayout;
}

PipelineLayout Device::createPipelineLayout(
    ArrayParameter<const DescriptorSetLayout* const> descriptorSetLayouts,
    ArrayParameter<const PushConstantRange> pushConstantRanges,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createPipelineLayout", debugName);

    VkPipelineLayoutHandle vkHandle = deviceImpl->getLogicalDevice()->createPipelineLayout(
        descriptorSetLayouts, pushConstantRanges);
    auto pipelineLayout = PipelineLayout(vkMakeHandleLifeguard(vkHandle));

    deviceImpl->getLogicalDevice()->setObjectDebugName(pipelineLayout.vkGetPipelineLayoutHandle(), debugName);

    return pipelineLayout;
}

OwningPtr<DescriptorPool> Device::createDescriptorPool(const DescriptorPoolSetup& setup, const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createDescriptorPool", debugName);

    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), DescriptorPoolTypeName, debugName);
    return OwningPtr<DescriptorPool>(new DescriptorPoolImpl(deviceImpl, setup, false, std::move(debugTarget)));
}

PipelineCache Device::createPipelineCache(ArrayParameter<const std::byte> data) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createPipelineCache", nullptr);

    VkPipelineCacheHandle vkHandle = deviceImpl->getLogicalDevice()->createPipelineCache(data);
    return PipelineCache(this, vkMakeHandleLifeguard(vkHandle));
}

PipelineCache Device::mergePipelineCaches(ArrayParameter<const PipelineCache* const> srcCaches) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "mergePipelineCaches", nullptr);

    VkPipelineCacheHandle vkDstHandle = deviceImpl->getLogicalDevice()->createPipelineCache({});

    ScratchVector<VkPipelineCacheHandle> vkSrcHandles(srcCaches.size());
    for (std::size_t i = 0; i < srcCaches.size(); i++) {
        vkSrcHandles[i] = srcCaches[i]->vkGetPipelineCacheHandle();
    }

    deviceImpl->getLogicalDevice()->mergePipelineCaches(view(vkSrcHandles), vkDstHandle);

    return PipelineCache(this, vkMakeHandleLifeguard(vkDstHandle));
}

void Device::compileComputePipelines(
    ArrayParameter<const ComputePipelineSetup* const> pipelineSetups,
    const PipelineCache* pipelineCache,
    ArrayParameter<Pipeline* const> compiledPipelines) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "compileComputePipelines", nullptr);
    if constexpr (TephraValidationEnabled) {
        if (pipelineSetups.size() != compiledPipelines.size()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The sizes of the 'pipelineSetups' (",
                pipelineSetups.size(),
                ") and 'compiledPipelines' (",
                compiledPipelines.size(),
                ") arrays do not match.");
        }
    }

    ComputePipelineInfoBuilder computePipelineInfoBuilder;
    ArrayView<VkComputePipelineCreateInfo> createInfos = computePipelineInfoBuilder.makeInfos(pipelineSetups);
    ScratchVector<VkPipelineHandle> vkCompiledPipelineHandles(compiledPipelines.size());

    deviceImpl->getLogicalDevice()->createComputePipelines(
        pipelineCache ? pipelineCache->vkGetPipelineCacheHandle() : VkPipelineCacheHandle(),
        createInfos,
        view(vkCompiledPipelineHandles));

    for (std::size_t i = 0; i < compiledPipelines.size(); i++) {
        *(compiledPipelines[i]) = Pipeline(vkMakeHandleLifeguard(vkCompiledPipelineHandles[i]));
        deviceImpl->getLogicalDevice()->setObjectDebugName(
            vkCompiledPipelineHandles[i], ComputePipelineInfoBuilder::getDebugName(pipelineSetups[i]));
    }
}

void Device::compileGraphicsPipelines(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups,
    const PipelineCache* pipelineCache,
    ArrayParameter<Pipeline* const> compiledPipelines) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "compileGraphicsPipelines", nullptr);
    if constexpr (TephraValidationEnabled) {
        if (pipelineSetups.size() != compiledPipelines.size()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The sizes of the 'pipelineSetups' (",
                pipelineSetups.size(),
                ") and 'compiledPipelines' (",
                compiledPipelines.size(),
                ") arrays do not match.");
        }
    }

    GraphicsPipelineInfoBuilder graphicsPipelineInfoBuilder;
    ArrayView<VkGraphicsPipelineCreateInfo> createInfos = graphicsPipelineInfoBuilder.makeInfos(pipelineSetups);
    ScratchVector<VkPipelineHandle> vkCompiledPipelineHandles(compiledPipelines.size());

    deviceImpl->getLogicalDevice()->createGraphicsPipelines(
        pipelineCache ? pipelineCache->vkGetPipelineCacheHandle() : VkPipelineCacheHandle(),
        createInfos,
        view(vkCompiledPipelineHandles));

    for (std::size_t i = 0; i < compiledPipelines.size(); i++) {
        *(compiledPipelines[i]) = Pipeline(vkMakeHandleLifeguard(vkCompiledPipelineHandles[i]));
        deviceImpl->getLogicalDevice()->setObjectDebugName(
            vkCompiledPipelineHandles[i], GraphicsPipelineInfoBuilder::getDebugName(pipelineSetups[i]));
    }
}

OwningPtr<JobResourcePool> Device::createJobResourcePool(const JobResourcePoolSetup& setup, const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createJobResourcePool", debugName);
    if constexpr (TephraValidationEnabled) {
        uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(setup.queue);
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "'setup.queue' is an invalid DeviceQueue handle.");
        }
    }

    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), JobResourcePoolTypeName, debugName);
    return OwningPtr<JobResourcePool>(new JobResourcePoolContainer(deviceImpl, setup, std::move(debugTarget)));
}

OwningPtr<Swapchain> Device::createSwapchainKHR(
    const SwapchainSetup& setup,
    Swapchain* oldSwapchain,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "createSwapchainKHR", debugName);

    VkSwapchainHandleKHR vkOldSwapchainHandle = oldSwapchain == nullptr ? VkSwapchainHandleKHR() :
                                                                          oldSwapchain->vkGetSwapchainHandle();

    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), SwapchainTypeName, debugName);
    ScratchVector<VkImageHandle> vkSwapchainImageHandles;
    VkSwapchainHandleKHR vkSwapchainHandle = deviceImpl->getLogicalDevice()->createSwapchainKHR(
        setup, vkOldSwapchainHandle, &vkSwapchainImageHandles);

    auto swapchain = OwningPtr<Swapchain>(new SwapchainImpl(
        deviceImpl,
        setup,
        vkMakeHandleLifeguard(vkSwapchainHandle),
        view(vkSwapchainImageHandles),
        std::move(debugTarget)));

    deviceImpl->getLogicalDevice()->setObjectDebugName(getOwnedPtr(swapchain)->vkGetSwapchainHandle(), debugName);

    return swapchain;
}

OwningPtr<Buffer> Device::allocateBuffer(
    const BufferSetup& setup,
    const MemoryPreference& memoryPreference,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "allocateBuffer", debugName);

    auto [bufferHandleLifeguard, allocationHandleLifeguard] = deviceImpl->getMemoryAllocator()->allocateBuffer(
        setup, memoryPreference);
    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), BufferTypeName, debugName);
    auto buffer = OwningPtr<Buffer>(new BufferImpl(
        deviceImpl,
        setup,
        std::move(bufferHandleLifeguard),
        std::move(allocationHandleLifeguard),
        std::move(debugTarget)));

    deviceImpl->getLogicalDevice()->setObjectDebugName(getOwnedPtr(buffer)->vkGetBufferHandle(), debugName);

    return buffer;
}

OwningPtr<Image> Device::allocateImage(const ImageSetup& setup, const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "allocateImage", debugName);

    auto [imageHandleLifeguard, allocationHandleLifeguard] = deviceImpl->getMemoryAllocator()->allocateImage(setup);
    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), ImageTypeName, debugName);
    auto image = OwningPtr<Image>(new ImageImpl(
        deviceImpl,
        setup,
        std::move(imageHandleLifeguard),
        std::move(allocationHandleLifeguard),
        std::move(debugTarget)));

    deviceImpl->getLogicalDevice()->setObjectDebugName(getOwnedPtr(image)->vkGetImageHandle(), debugName);

    return image;
}

OwningPtr<AccelerationStructure> Device::allocateAccelerationStructure(
    const AccelerationStructureSetup& setup,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "allocateAccelerationStructure", debugName);

    auto buildInfo = AccelerationStructureImpl::prepareBuildInfoForSizeQuery(setup);
    auto vkBuildSizes = deviceImpl->getLogicalDevice()->getAccelerationStructureBuildSizes(
        buildInfo.geomInfo, buildInfo.maxPrimitiveCounts.data());

    // Create backing buffer to hold the AS
    auto backingBufferSetup = BufferSetup(
        vkBuildSizes.accelerationStructureSize,
        BufferUsageMask::None(),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
    auto [bufferHandleLifeguard, allocationHandleLifeguard] = deviceImpl->getMemoryAllocator()->allocateBuffer(
        backingBufferSetup, MemoryPreference::Device);
    auto backingBuffer = OwningPtr<Buffer>(new BufferImpl(
        deviceImpl,
        backingBufferSetup,
        std::move(bufferHandleLifeguard),
        std::move(allocationHandleLifeguard),
        DebugTarget::makeSilent()));

    auto accelerationStructureLifeguard = vkMakeHandleLifeguard(
        deviceImpl->getLogicalDevice()->createAccelerationStructureKHR(backingBuffer->getDefaultView(), setup.type));
    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), AccelerationStructureTypeName, debugName);

    return {};
}

JobSemaphore Device::enqueueJob(
    const DeviceQueue& queue,
    Job job,
    ArrayParameter<const JobSemaphore> waitJobSemaphores,
    ArrayParameter<const ExternalSemaphore> waitExternalSemaphores,
    ArrayParameter<const ExternalSemaphore> signalExternalSemaphores) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);

#ifdef TEPHRA_ENABLE_DEBUG_CONTEXTS
    const char* debugJobName = JobResourcePoolContainer::getJobDebugTarget(job)->getObjectName();
    // The job will get destroyed during this function, we need to extend the lifetime of the object name
    std::string debugJobNameString;
    if (debugJobName != nullptr)
        debugJobNameString = debugJobName;
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "enqueueJob", debugJobNameString.c_str());
#endif

    JobData* jobData = JobResourcePoolContainer::getJobData(job);
    uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(queue);

    if constexpr (TephraValidationEnabled) {
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "'queue' is an invalid DeviceQueue handle.");
        }

        TEPHRA_ASSERT(jobData->resourcePoolImpl != nullptr);
        if (jobData->resourcePoolImpl->getBaseQueueIndex() != queueIndex) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The Job was enqueued to a different queue (",
                deviceImpl->getQueueMap()->getQueueInfo(queue).name,
                ") than the queue used when creating the JobResourcePool the Job was allocated from (",
                deviceImpl->getQueueMap()->getQueueInfos()[jobData->resourcePoolImpl->getBaseQueueIndex()].name,
                ").");
        }

        for (const auto& semaphore : waitJobSemaphores) {
            uint32_t semaphoreQueueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(semaphore.queue);
            if (queueIndex == ~0) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "'waitJobSemaphores[].queue' is an invalid DeviceQueue handle.");
            }

            if (queueIndex == semaphoreQueueIndex) {
                reportDebugMessage(
                    DebugMessageSeverity::Warning,
                    DebugMessageType::Performance,
                    "'waitJobSemaphores[].queue' is the same queue as 'queue' (",
                    deviceImpl->getQueueMap()->getQueueInfo(queue).name,
                    "). Waiting for a job previously submitted to the same queue is not necessary as they are "
                    "synchronized implicitly.");
            }
        }
    }

    // Add the semaphores to the job data structure as well
    for (const auto& semaphore : waitJobSemaphores) {
        jobData->semaphores.jobWaits.push_back(semaphore);
    }
    for (const auto& semaphore : waitExternalSemaphores) {
        jobData->semaphores.externalWaits.push_back(semaphore);
    }
    for (const auto& semaphore : signalExternalSemaphores) {
        jobData->semaphores.externalSignals.push_back(semaphore);
    }

    // Acquire new unique timestamp for the job that it will signal once complete
    JobSemaphore signalSemaphore;
    signalSemaphore.queue = queue;
    signalSemaphore.timestamp = deviceImpl->getTimelineManager()->trackNextTimestamp(queueIndex);
    jobData->semaphores.jobSignal = signalSemaphore;

    // Update the timeline manager to process its callbacks and free up some resources before allocating again
    deviceImpl->getTimelineManager()->update();

    // Enqueue the job
    QueueState* queueState = deviceImpl->getQueueState(queueIndex);
    queueState->enqueueJob(std::move(job));
    deviceImpl->getTimelineManager()->markPendingTimestamp(signalSemaphore.timestamp, queueIndex);

    // Make sure deferred destructor will get updated so handles can be safely released
    deviceImpl->getTimelineManager()->addCleanupCallback([deviceImpl]() {
        uint64_t reachedTimestamp = deviceImpl->getTimelineManager()->getLastReachedTimestampInAllQueues();
        deviceImpl->getDeferredDestructor()->destroyUpToTimestamp(reachedTimestamp);
    });

    return signalSemaphore;
}

void Device::submitQueuedJobs(
    const DeviceQueue& queue,
    const JobSemaphore& lastJobToSubmit,
    ArrayParameter<const JobSemaphore> waitJobSemaphores,
    ArrayParameter<const ExternalSemaphore> waitExternalSemaphores) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(
        deviceImpl->getDebugTarget(), "submitQueuedJobs", deviceImpl->getQueueMap()->getQueueInfo(queue).name.c_str());

    uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(queue);

    if constexpr (TephraValidationEnabled) {
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "'queue' is an invalid DeviceQueue handle.");
        }

        if (!lastJobToSubmit.isNull() && lastJobToSubmit.queue != queue) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The 'lastJobToSubmit' semaphore belongs to a job that was enqueued to a different queue than the one "
                "identified by the 'queue' parameter.");
        }
    }

    deviceImpl->getQueueState(queueIndex)->submitQueuedJobs(lastJobToSubmit, waitJobSemaphores, waitExternalSemaphores);
}

void Device::submitPresentImagesKHR(
    const DeviceQueue& queue,
    ArrayParameter<Swapchain* const> swapchains,
    ArrayParameter<const uint32_t> imageIndices) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(
        deviceImpl->getDebugTarget(),
        "submitPresentImagesKHR",
        deviceImpl->getQueueMap()->getQueueInfo(queue).name.c_str());

    uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(queue);

    if constexpr (TephraValidationEnabled) {
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "'queue' is an invalid DeviceQueue handle.");
        }

        if (swapchains.size() != imageIndices.size()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The sizes of the 'swapchains' (",
                swapchains.size(),
                ") and 'imageIndices' (",
                imageIndices.size(),
                ") arrays do not match.");
        }
    }

    SwapchainImpl::submitPresentImages(deviceImpl, queueIndex, swapchains, imageIndices);

    // It should generally be useful to do this here
    deviceImpl->getTimelineManager()->update();
}

bool Device::isJobSemaphoreSignalled(const JobSemaphore& semaphore) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "isJobSemaphoreSignalled", nullptr);

    uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(semaphore.queue);

    if constexpr (TephraValidationEnabled) {
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "'semaphore.queue' is an invalid DeviceQueue handle.");
        }
    }

    deviceImpl->getTimelineManager()->updateQueue(queueIndex);
    return deviceImpl->getTimelineManager()->wasTimestampReachedInQueue(queueIndex, semaphore.timestamp);
}

bool Device::waitForJobSemaphores(ArrayParameter<const JobSemaphore> semaphores, bool waitAll, Timeout timeout) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "waitForJobSemaphores", nullptr);

    // Reduce to one semaphore per queue
    ScratchVector<uint32_t> queueIndices;
    ScratchVector<uint64_t> queueTimestamps;

    for (const auto& semaphore : semaphores) {
        uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(semaphore.queue);

        if constexpr (TephraValidationEnabled) {
            if (queueIndex == ~0) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "'semaphores[].queue' is an invalid DeviceQueue handle.");
            }
        }

        bool added = false;
        for (std::size_t i = 0; i < queueIndices.size(); i++) {
            if (queueIndices[i] == queueIndex) {
                if (waitAll)
                    queueTimestamps[i] = tp::max(queueTimestamps[i], semaphore.timestamp);
                else
                    queueTimestamps[i] = tp::min(queueTimestamps[i], semaphore.timestamp);

                added = true;
                break;
            }
        }

        if (!added) {
            queueIndices.push_back(queueIndex);
            queueTimestamps.push_back(semaphore.timestamp);
        }
    }

    return deviceImpl->getTimelineManager()->waitForTimestamps(
        view(queueIndices), view(queueTimestamps), waitAll, timeout);
}

void Device::waitForIdle() {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "waitForIdle", nullptr);

    deviceImpl->getLogicalDevice()->waitForDeviceIdle();
    // Release resources and update callbacks as well
    deviceImpl->getTimelineManager()->update();
}

void Device::addCleanupCallback(CleanupCallback callback) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "addCleanupCallback", nullptr);

    deviceImpl->getTimelineManager()->addCleanupCallback(std::move(callback));
}

void Device::updateSemaphores() {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "updateSemaphores", nullptr);

    deviceImpl->getTimelineManager()->update();
}

OwningPtr<Buffer> Device::vkCreateExternalBuffer(
    const BufferSetup& setup,
    Lifeguard<VkBufferHandle>&& bufferHandle,
    Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "vkCreateExternalBuffer", nullptr);

    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), BufferTypeName, debugName);
    auto buffer = OwningPtr<Buffer>(new BufferImpl(
        deviceImpl, setup, std::move(bufferHandle), std::move(memoryAllocationHandle), std::move(debugTarget)));

    deviceImpl->getLogicalDevice()->setObjectDebugName(getOwnedPtr(buffer)->vkGetBufferHandle(), debugName);

    return buffer;
}

OwningPtr<Image> Device::vkCreateExternalImage(
    const ImageSetup& setup,
    Lifeguard<VkImageHandle>&& imageHandle,
    Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
    const char* debugName) {
    auto deviceImpl = static_cast<DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "vkCreateExternalImage", nullptr);

    auto debugTarget = DebugTarget(deviceImpl->getDebugTarget(), ImageTypeName, debugName);
    auto image = OwningPtr<Image>(new ImageImpl(
        deviceImpl, setup, std::move(imageHandle), std::move(memoryAllocationHandle), std::move(debugTarget)));

    deviceImpl->getLogicalDevice()->setObjectDebugName(getOwnedPtr(image)->vkGetImageHandle(), debugName);

    return image;
}

template <typename TypedHandle>
Lifeguard<TypedHandle> Device::vkMakeHandleLifeguard(TypedHandle vkHandle) {
    if (vkHandle != VK_NULL_HANDLE) {
        auto deviceImpl = static_cast<DeviceContainer*>(this);
        return Lifeguard<TypedHandle>(deviceImpl, vkHandle);
    } else {
        return {};
    }
}

MemoryHeapStatistics Device::getMemoryHeapStatistics(uint32_t memoryHeapIndex) const {
    auto deviceImpl = static_cast<const DeviceContainer*>(this);
    TEPHRA_DEBUG_SET_CONTEXT(deviceImpl->getDebugTarget(), "getMemoryHeapStatistics", nullptr);

    if constexpr (TephraValidationEnabled) {
        auto& memProperties = deviceImpl->getPhysicalDevice()->vkQueryProperties<VkPhysicalDeviceMemoryProperties>();
        if (memoryHeapIndex >= memProperties.memoryHeapCount) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "'memoryHeapIndex' (",
                memoryHeapIndex,
                ") specifies a heap that does not exist.");
        }
    }

    MemoryHeapStatistics stats{};
    VmaBudget budget = deviceImpl->getMemoryAllocator()->getMemoryHeapBudget(memoryHeapIndex);

    stats.allocationCount = budget.statistics.allocationCount;
    stats.allocationBytes = budget.statistics.allocationBytes;
    stats.blockCount = budget.statistics.blockCount;
    stats.blockBytes = budget.statistics.blockBytes;
    stats.processUsageBytes = budget.usage;
    stats.processBudgetBytes = budget.budget;
    return stats;
}

VkDeviceHandle Device::vkGetDeviceHandle() const {
    auto deviceImpl = static_cast<const DeviceContainer*>(this);
    return deviceImpl->getLogicalDevice()->vkGetDeviceHandle();
}

VmaAllocatorHandle Device::vmaGetAllocatorHandle() const {
    auto deviceImpl = static_cast<const DeviceContainer*>(this);
    return deviceImpl->getMemoryAllocator()->vmaGetAllocatorHandle();
}

VkQueueHandle Device::vkGetQueueHandle(const DeviceQueue& queue) const {
    auto deviceImpl = static_cast<const DeviceContainer*>(this);

    if constexpr (TephraValidationEnabled) {
        uint32_t queueIndex = deviceImpl->getQueueMap()->getQueueUniqueIndex(queue);
        if (queueIndex == ~0) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "'queue' is an invalid DeviceQueue handle.");
        }
    }

    return deviceImpl->getQueueMap()->getQueueInfo(queue).vkQueueHandle;
}

void* Device::vkLoadDeviceProcedure(const char* procedureName) const {
    auto deviceImpl = static_cast<const DeviceContainer*>(this);
    return deviceImpl->getParentAppImpl()->getInstance()->loadDeviceProcedure(vkGetDeviceHandle(), procedureName);
}

DeviceContainer::~DeviceContainer() {
    TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(getDebugTarget());
}

// Template declarations for vkMakeHandleLifeguard
template Lifeguard<VkDescriptorPoolHandle> Device::vkMakeHandleLifeguard(VkDescriptorPoolHandle vkHandle);
template Lifeguard<VkShaderModuleHandle> Device::vkMakeHandleLifeguard(VkShaderModuleHandle vkHandle);
template Lifeguard<VkDescriptorSetLayoutHandle> Device::vkMakeHandleLifeguard(VkDescriptorSetLayoutHandle vkHandle);
template Lifeguard<VkDescriptorUpdateTemplateHandle> Device::vkMakeHandleLifeguard(
    VkDescriptorUpdateTemplateHandle vkHandle);
template Lifeguard<VkPipelineLayoutHandle> Device::vkMakeHandleLifeguard(VkPipelineLayoutHandle vkHandle);
template Lifeguard<VkPipelineCacheHandle> Device::vkMakeHandleLifeguard(VkPipelineCacheHandle vkHandle);
template Lifeguard<VmaAllocationHandle> Device::vkMakeHandleLifeguard(VmaAllocationHandle vkHandle);
template Lifeguard<VkBufferHandle> Device::vkMakeHandleLifeguard(VkBufferHandle vkHandle);
template Lifeguard<VkBufferViewHandle> Device::vkMakeHandleLifeguard(VkBufferViewHandle vkHandle);
template Lifeguard<VkImageHandle> Device::vkMakeHandleLifeguard(VkImageHandle vkHandle);
template Lifeguard<VkImageViewHandle> Device::vkMakeHandleLifeguard(VkImageViewHandle vkHandle);
template Lifeguard<VkSamplerHandle> Device::vkMakeHandleLifeguard(VkSamplerHandle vkHandle);
template Lifeguard<VkDescriptorPoolHandle> Device::vkMakeHandleLifeguard(VkDescriptorPoolHandle vkHandle);
template Lifeguard<VkPipelineHandle> Device::vkMakeHandleLifeguard(VkPipelineHandle vkHandle);
template Lifeguard<VkSwapchainHandleKHR> Device::vkMakeHandleLifeguard(VkSwapchainHandleKHR vkHandle);
template Lifeguard<VkSemaphoreHandle> Device::vkMakeHandleLifeguard(VkSemaphoreHandle vkHandle);

}
