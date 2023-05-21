#pragma once

#include <tephra/swapchain.hpp>
#include <tephra/job.hpp>
#include <tephra/pipeline.hpp>
#include <tephra/descriptor.hpp>
#include <tephra/buffer.hpp>
#include <tephra/image.hpp>
#include <tephra/render.hpp>
#include <tephra/memory.hpp>
#include <tephra/debug_handler.hpp>
#include <tephra/common.hpp>
#include <functional>

namespace tp {

/// Returns statistics associated with a Vulkan memory heap.
/// @see tp::Device::getMemoryHeapStatistics
/// @see @vmasymbol{VmaBudget,struct_vma_budget}
struct MemoryHeapStatistics {
    /// The number of resources allocated from this heap.
    uint32_t allocationCount;
    /// The size of all resources allocated from this heap in bytes.
    uint64_t allocationBytes;
    /// The number of blocks of backing @vksymbol{VkDeviceMemory}.
    uint32_t blockCount;
    /// The size of all blocks of backing memory allocated from this heap in bytes.
    /// It is always greater or equal to `allocationBytes`.
    /// @remarks
    ///     The difference `(blockBytes - allocationBytes)` is the amount of memory allocated from this heap,
    ///     but unused by any resource.
    uint64_t blockBytes;
    /// The estimated current memory usage of this process in bytes. Fetched from system if
    /// tp::DeviceExtension::EXT_MemoryBudget is enabled.
    uint64_t processUsageBytes;
    /// The estimated amount of memory available for use by this process in bytes. Fetched from system if
    /// tp::DeviceExtension::EXT_MemoryBudget is enabled.
    /// @remarks
    ///     The difference `(processBudgetBytes - processUsageBytes)` is the amount of remaining memory available for
    ///     use before either previous allocations start paging out, or new allocations fail.
    uint64_t processBudgetBytes;
};

/// The type of the user-provided function callback that can be used for freeing external resources safely.
/// @see tp::Device::addCleanupCallback
using CleanupCallback = std::function<void()>;

/// Used to configure the device-wide Vulkan Memory Allocator.
/// @see tp::DeviceSetup
/// @see @vmasymbol{VmaAllocatorCreateInfo,struct_vma_allocator_create_info}
struct MemoryAllocatorSetup {
    uint64_t preferredLargeHeapBlockSize;
    VmaDeviceMemoryCallbacks* vmaDeviceMemoryCallbacks;

    /// @param preferredLargeHeapBlockSize
    ///     The preferred size in bytes of a single memory block to be allocated from large heaps > 1 GiB.
    ///     Set to 0 to use the VMA default, which is currently 256 MiB.
    /// @param vmaDeviceMemoryCallbacks
    ///     Informative callbacks for @vksymbol{vkAllocateMemory}, @vksymbol{vkFreeMemory}.
    ///     See @vmasymbol{VmaDeviceMemoryCallbacks,struct_vma_device_memory_callbacks}
    MemoryAllocatorSetup(
        uint64_t preferredLargeHeapBlockSize = 0,
        VmaDeviceMemoryCallbacks* vmaDeviceMemoryCallbacks = nullptr);
};

/// Used as configuration for creating a new tp::Device object.
/// @see tp::Application::createDevice
/// @see @vksymbol{VkDeviceCreateInfo}
struct DeviceSetup {
    const PhysicalDevice* physicalDevice;
    ArrayView<const DeviceQueue> queues;
    ArrayView<const char* const> extensions;
    const VkFeatureMap* vkFeatureMap;
    MemoryAllocatorSetup memoryAllocatorSetup;
    void* vkCreateInfoExtPtr;

    /// @param physicalDevice
    ///     The physical device used by the Device. It needs to be one of the pointers returned by
    ///     tp::Application::getPhysicalDevices.
    /// @param queues
    ///     An array of queues that will be available for use with the Device. At least one
    ///     queue is required. A Graphics queue is not guaranteed to be supported and should
    ///     be checked with tp::PhysicalDevice::getQueueTypeInfo.
    /// @param extensions
    ///     The set of device extensions to enable. The extensions must be supported by the device,
    ///     as can be checked with tp::PhysicalDevice::isExtensionAvailable. See tp::DeviceExtension.
    /// @param vkFeatureMap
    ///     If not `nullptr`, points to a map of Vulkan structures that describe which
    ///     features are to be enabled on the device. The features must be supported by the device,
    ///     as can be checked with tp::PhysicalDevice::vkQueryFeatures. Any features that depend on extensions must
    ///     have their extensions enabled as well.
    /// @param memoryAllocatorSetup
    ///     The configuration of the device-wide Vulkan Memory Allocator.
    /// @param vkCreateInfoExtPtr
    ///     The pointer to additional Vulkan setup structure to be passed in `pNext` of @vksymbol{VkDeviceCreateInfo}.
    /// @remarks
    ///     The number of requested queues of a particular type can be greater than the number of queues exposed
    ///     by the physical device, as long as at least one queue is exposed. In that case the "logical" queues will
    ///     be mapped onto the exposed queues in a round-robin fashion.
    DeviceSetup(
        const PhysicalDevice* physicalDevice,
        ArrayView<const DeviceQueue> queues,
        ArrayView<const char* const> extensions = {},
        const VkFeatureMap* vkFeatureMap = nullptr,
        MemoryAllocatorSetup memoryAllocatorSetup = {},
        void* vkCreateInfoExtPtr = nullptr);
};

/// Represents a connection to a tp::PhysicalDevice, through which its functionality can be accessed.
///
/// A device object is the main means of interacting with the actual device on the platform. Through it most other
/// objects in Tephra are created. Such objects can only be used with this device and other objects created from it.
/// The device also provides the means to submit work to the device, by enqueueing tp::Job objects to a particular
/// tp::DeviceQueue with tp::Device::enqueueJob and then submitting them for execution with
/// tp::Device::submitQueuedJobs.
///
/// @remarks
///     Access to the Device object is internally synchronized, meaning it is safe to operate on it from multiple
///     threads at the same time. However, beware that the device's queues aren't. Only one method may operate on
///     a particular tp::DeviceQueue at any time.
/// @see tp::Application::createDevice
/// @see @vksymbol{VkDevice}
class Device : public Ownable {
public:
    /// Creates a tp::Sampler object according to the given setup structure.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    Sampler createSampler(const SamplerSetup& setup, const char* debugName = nullptr);

    /// Creates a tp::ShaderModule object out of the given SPIR-V shader code.
    /// @param shaderCode
    ///     The SPIR-V code to be used to create the shader module.
    /// @param debugName
    ///     The debug name identifier for the object.
    ShaderModule createShaderModule(ArrayParameter<const uint32_t> shaderCode, const char* debugName = nullptr);

    /// Creates a tp::DescriptorSetLayout object from the given bindings.
    /// @param descriptorBindings
    ///     The descriptor bindings that define the layout.
    /// @param debugName
    ///     The debug name identifier for the object.
    DescriptorSetLayout createDescriptorSetLayout(
        ArrayParameter<const DescriptorBinding> descriptorBindings,
        const char* debugName = nullptr);

    /// Creates a tp::PipelineLayout object from the given descriptor set and push constant layouts.
    /// @param descriptorSetLayouts
    ///     An array describing the layout of descriptor sets that will be accessed by the pipelines and that will
    ///     need to be bound to each set number.
    /// @param pushConstantRanges
    ///     The layout of push constants that will be accessed by the pipelines.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The set number provided during the creation of each tp::DescriptorSetLayout must match
    ///     its index in the array.
    PipelineLayout createPipelineLayout(
        ArrayParameter<const DescriptorSetLayout* const> descriptorSetLayouts,
        ArrayParameter<const PushConstantRange> pushConstantRanges = {},
        const char* debugName = nullptr);

    /// Creates a tp::RenderPassLayout object from the given attachment descriptions and subpass layouts.
    /// @param attachmentDescriptions
    ///     Describes the layout and properties of attachments that will be used during a render pass
    ///     executed with this layout.
    /// @param subpassLayouts
    ///     Describes the layout and properties of the subpasses inside the render pass created with this layout.
    /// @param debugName
    ///     The debug name identifier for the object.
    RenderPassLayout createRenderPassLayout(
        ArrayParameter<const AttachmentDescription> attachmentDescriptions,
        ArrayParameter<const SubpassLayout> subpassLayouts,
        const char* debugName = nullptr);

    /// Creates a tp::DescriptorPool object according to the given setup structure.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    OwningPtr<DescriptorPool> createDescriptorPool(const DescriptorPoolSetup& setup, const char* debugName = nullptr);

    /// Creates a tp::PipelineCache object.
    /// @param data
    ///     An optional parameter specifying the source binary data that the tp::PipelineCache should
    ///     be initialized to. This data would previously come from tp::PipelineCache::getData.
    /// @remarks
    ///     The intention is that the data may be saved to disk between runs of the application to speed up
    ///     compilation on the next launch if tp::PhysicalDevice::pipelineCacheUUID matches.
    PipelineCache createPipelineCache(ArrayParameter<const std::byte> data = {});

    /// Creates a new tp::PipelineCache object by merging together multiple existing tp::PipelineCache objects.
    PipelineCache mergePipelineCaches(ArrayParameter<const PipelineCache* const> srcCaches);

    /// Batch compiles multiple compute tp::Pipeline objects.
    /// @param pipelineSetups
    ///     The setup structures describing the compute pipelines that are to be compiled.
    /// @param pipelineCache
    ///     The tp::PipelineCache object to be used to accelerate the compilation, can be nullptr.
    /// @param compiledPipelines
    ///     An output array of pointers to tp::Pipeline objects that will represent the compiled pipelines.
    ///     The size of this array must match the size of `pipelineSetups` and all of the elements must point
    ///     be valid pointers.
    /// @remarks
    ///     Pipeline compilation can be slow. The use of a tp::pipelineCache is recommended as is splitting
    ///     the pipeline compilation into multiple threads.
    void compileComputePipelines(
        ArrayParameter<const ComputePipelineSetup* const> pipelineSetups,
        const PipelineCache* pipelineCache,
        ArrayParameter<Pipeline* const> compiledPipelines);

    /// Batch compiles multiple graphics tp::Pipeline objects.
    /// @param pipelineSetups
    ///     The setup structures describing the graphics pipelines that are to be compiled.
    /// @param pipelineCache
    ///     The tp::PipelineCache object to be used to accelerate the compilation, can be nullptr.
    /// @param compiledPipelines
    ///     An output array of pointers to tp::Pipeline objects that will represent the compiled pipelines.
    ///     The size of this array must match the size of `pipelineSetups` and all of the elements must point
    ///     be valid pointers.
    /// @remarks
    ///     Pipeline compilation can be slow. The use of a tp::pipelineCache is recommended as is splitting
    ///     the pipeline compilation into multiple threads.
    void compileGraphicsPipelines(
        ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups,
        const PipelineCache* pipelineCache,
        ArrayParameter<Pipeline* const> compiledPipelines);

    /// Creates a tp::JobResourcePool object according to the given setup structure.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    OwningPtr<JobResourcePool> createJobResourcePool(const JobResourcePoolSetup& setup, const char* debugName = nullptr);

    /// Creates a tp::Swapchain object according to the given setup structure.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param oldSwapchain
    ///     An old swapchain to reuse resources of. It will be switched to a retired state and new images
    ///     can no longer be acquired from it.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The use of this function requires the tp::DeviceExtension::KHR_Swapchain extension to be enabled.
    OwningPtr<Swapchain> createSwapchainKHR(
        const SwapchainSetup& setup,
        Swapchain* oldSwapchain = nullptr,
        const char* debugName = nullptr);

    /// Creates a tp::Buffer object according to the given setup structure and allocates memory for it according
    /// to the memory preference.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param memoryPreference
    ///     The memory preference progression that will be used for allocating memory for the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    OwningPtr<Buffer> allocateBuffer(
        const BufferSetup& setup,
        const MemoryPreference& memoryPreference,
        const char* debugName = nullptr);

    /// Creates a tp::Image object according to the given setup structure and allocates memory for it.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    OwningPtr<Image> allocateImage(const ImageSetup& setup, const char* debugName = nullptr);

    /// Enqueues the given tp::Job to the specified queue, creating and initializing its local resources.
    ///
    /// When the job gets enqueued, all of the job-local resources get created and become accessible, and command
    /// lists can be recorded. To actually schedule execution of the job on the device, tp::Device::submitQueuedJobs
    /// must be called.
    ///
    /// @param queue
    ///     The queue that the job will be submitted to. The queue must match the queue used for creating the
    ///     tp::JobResourcePool object that is the parent of the enqueued job.
    /// @param job
    ///     The tp::Job object to enqueue. The ownership is transferred from the user over to the implementation.
    /// @param waitJobSemaphores
    ///     A list of job semaphores that the job will wait on before actually executing on the device. It is only
    ///     necessary to wait on semaphores of jobs submitted to other queues.
    /// @param waitExternalSemaphores
    ///     A list of external semaphores the job will wait on before executing on the device.
    /// @param signalExternalSemaphores
    ///     A list of external semaphores the job will signal once it finishes executing on the device.
    /// @returns
    ///     Returns a job semaphore that will be signalled once the job finishes executing on the device.
    /// @remarks
    ///     The semaphores specified in `waitJobSemaphores` must belong to jobs that will be submitted through
    ///     tp::Device::submitQueuedJobs before this job gets submitted. The same applies to `waitExternalSemaphores`,
    ///     which must have a signalling operation submitted before this job.
    /// @remarks
    ///     It is recommended to call tp::Device::submitQueuedJobs within a reasonable timeframe.
    ///     Jobs that are hanging in the enqueued state may prevent some resources from being deallocated.
    JobSemaphore enqueueJob(
        const DeviceQueue& queue,
        Job job,
        ArrayParameter<const JobSemaphore> waitJobSemaphores = {},
        ArrayParameter<const ExternalSemaphore> waitExternalSemaphores = {},
        ArrayParameter<const ExternalSemaphore> signalExternalSemaphores = {});

    /// Submits all tp::Job objects previously enqueued to the specified queue and schedules them to be executed
    /// on the device.
    /// @param queue
    ///     The queue to have its enqueued jobs submitted for execution.
    void submitQueuedJobs(const DeviceQueue& queue);

    /// Submits a present operation to the specified queue for each of the given tp::Swapchain objects,
    /// queueing the given acquired image from each swapchain for presentation.
    /// @param queue
    ///     The queue that the present operation will be submitted to.
    /// @param swapchains
    ///     An array of swapchains whose images will be presented.
    /// @param imageIndices
    ///     An array of indices of the acquired swapchain images to be presented. The indices should be
    ///     the tp::AcquiredImageInfo::imageIndex of an image previously acquired from the corresponding swapchain
    ///     in the `swapchains` array.
    /// @remarks
    ///     The sizes of the `swapchains` and `imageIndices` arrays must be the same.
    /// @remarks
    ///     Each image to be presented must have been exported with the tp::Job::cmdExportResource command to
    ///     the presenting queue for present operations as part of a previously submitted job.
    /// @see tp::Swapchain documentation for an overview of the presentation workflow.
    void submitPresentImagesKHR(
        const DeviceQueue& queue,
        ArrayParameter<Swapchain* const> swapchains,
        ArrayParameter<const uint32_t> imageIndices);

    /// Returns `true` if the given tp::JobSemaphore has been signalled, meaning the job has finished executing
    /// on the device.
    /// @param semaphore
    ///     The job semaphore to query the state of.
    bool isJobSemaphoreSignalled(const JobSemaphore& semaphore);

    /// Waits until the given tp::JobSemaphore handles have been signalled or until the timeout has been reached.
    /// @param semaphores
    ///     The semaphores to wait for.
    /// @param waitAll
    ///     If `true`, the function returns when all of the given semaphores have been signalled. Otherwise,
    ///     it returns when at least one of them has been signalled.
    /// @param timeout
    ///     The timeout limit or waiting.
    /// @returns
    ///     Returns `true` if all (or at least one of, depending on the `waitAll` parameter) the semaphores have been
    ///     signalled. Returns `false` when the timeout has been reached and the semaphores are still unsignalled.
    /// @remarks
    ///     The jobs signalling the semaphores must already be submitted for execution, otherwise the semaphores
    ///     will never be signalled.
    /// @remarks
    ///     Waiting alone does not guarantee that the data will be visible to the host. An appropriate export
    ///     operation is also required.
    bool waitForJobSemaphores(
        ArrayParameter<const JobSemaphore> semaphores,
        bool waitAll = true,
        Timeout timeout = Timeout::Indefinite());

    /// Waits until the device becomes idle. It guarantees that all submitted jobs have finished executing and their
    /// corresponding semaphores have been signalled.
    /// @remarks
    ///     Waiting alone does not guarantee that the data will be visible to the host. An appropriate export
    ///     operation is also required.
    void waitForIdle();

    /// Stores a function that will be called after all currently enqueued or submitted jobs have finished executing
    /// on the device, allowing it to free external resources that were used up until this point in time.
    /// @param callback
    ///     The function to be called.
    /// @remarks
    ///     The function will **not** be called the moment the semaphores become signalled. Their status is only checked
    ///     occasionally as part of various other API calls. This update can be triggered explicitly through
    ///     tp::Device::updateSemaphores.
    /// @remarks
    ///     Other device methods that operate on queues (e.g. enqueuing a follow-up job) must **not** be called from
    ///     within the callback function.
    void addCleanupCallback(CleanupCallback callback);

    /// Updates the status of job semaphores and triggers the freeing of resources and calling cleanup callbacks if
    /// some jobs have finished executing since the last update.
    void updateSemaphores();

    /// Creates a tp::Buffer object out of a raw Vulkan buffer handle and an optional VMA memory allocation handle.
    /// @param setup
    ///     The setup structure that would result in a similar buffer if created with tp::Device::allocateBuffer.
    /// @param bufferHandle
    ///     A lifeguard handle for a Vulkan buffer created from the same Vulkan device as returned by
    ///     tp::Device::vkGetDeviceHandle.
    /// @param memoryAllocationHandle
    ///     A lifeguard handle for the associated VMA memory allocation that has been allocated with the same VMA
    ///     allocator as returned by tp::Device::vmaGetAllocatorHandle. It can be null, but if it is,
    ///     tp::Buffer::mapForHostAccess, tp::Buffer::getMemoryLocation and tp::Buffer::getRequiredViewAlignment must
    ///     not be called on the resulting buffer.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     `bufferHandle` must have been created from the same Vulkan device as returned by
    ///     tp::Device::vkGetDeviceHandle.
    ///     `memoryAllocationHandle` must have
    /// @remarks
    ///     The lifeguard handles can be either owning or non-owning, which determines whether the handles will be
    ///     properly disposed of when the buffer is destroyed. See tp::Device::vkMakeHandleLifeguard or
    ///     tp::Lifeguard::NonOwning.
    OwningPtr<Buffer> vkCreateExternalBuffer(
        const BufferSetup& setup,
        Lifeguard<VkBufferHandle>&& bufferHandle,
        Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
        const char* debugName = nullptr);

    /// Creates a tp::Image object out of a raw Vulkan image handle and an optional VMA memory allocation handle.
    /// @param setup
    ///     The setup structure that would result in a similar image if created with tp::Device::allocateImage.
    /// @param imageHandle
    ///     A lifeguard handle for a Vulkan image created from the same Vulkan device as returned by
    ///     tp::Device::vkGetDeviceHandle.
    /// @param memoryAllocationHandle
    ///     A lifeguard handle for the associated VMA memory allocation that has been allocated with the same
    ///     VMA allocator as returned by tp::Device::vmaGetAllocatorHandle. It can be null, but if it is,
    ///     tp::Image::getMemoryLocation must not be called on the resulting image.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The lifeguard handles can be either owning or non-owning, which determines whether the handles will be
    ///     properly disposed of when the image is destroyed. See tp::Device::vkMakeHandleLifeguard or
    ///     tp::Lifeguard::NonOwning.
    OwningPtr<Image> vkCreateExternalImage(
        const ImageSetup& setup,
        Lifeguard<VkImageHandle>&& imageHandle,
        Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
        const char* debugName = nullptr);

    /// Wraps the given Vulkan handle object in an owning tp::Lifeguard, ensuring its safe deletion after the lifeguard
    /// gets destroyed.
    /// Example usage: `device->vkMakeHandleLifeguard(VkImageHandle(vkImage))`
    /// @remarks
    ///     The handle must have been created from this device.
    /// @remarks
    ///     A non-owning handle can be created with tp::Lifeguard::NonOwning.
    /// @remarks
    ///     Only certain types of handles (those that Tephra knows how to destroy) are supported. For others,
    ///     consider using tp::Device::addCleanupCallback instead.
    template <typename TypedHandle>
    Lifeguard<TypedHandle> vkMakeHandleLifeguard(TypedHandle vkHandle);

    /// Returns the statistics for the Vulkan memory heap with the given index.
    /// @remarks
    ///     Use tp::PhysicalDevice::getMemoryLocationInfo to retrieve the heap index associated with a particular
    ///     tp::MemoryLocation.
    MemoryHeapStatistics getMemoryHeapStatistics(uint32_t memoryHeapIndex) const;

    /// Returns the Vulkan @vksymbol{VkDevice} handle.
    VkDeviceHandle vkGetDeviceHandle() const;

    /// Returns the VMA @vmasymbol{VmaAllocator,struct_vma_allocator} handle.
    VmaAllocatorHandle vmaGetAllocatorHandle() const;

    /// Returns the Vulkan @vksymbol{VkQueue} handle associated with the given tp::DeviceQueue.
    /// @param queue
    ///     The queue to return the handle for.
    VkQueueHandle vkGetQueueHandle(const DeviceQueue& queue) const;

    /// Loads a Vulkan device procedure with the given name and returns a pointer to it, or `nullptr`
    /// if not successful.
    void* vkLoadDeviceProcedure(const char* procedureName) const;

    TEPHRA_MAKE_INTERFACE(Device)

protected:
    Device() {}
};

}
