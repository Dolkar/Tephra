#pragma once

#include <tephra/physical_device.hpp>
#include <tephra/descriptor.hpp>
#include <tephra/acceleration_structure.hpp>
#include <tephra/buffer.hpp>
#include <tephra/image.hpp>
#include <tephra/memory.hpp>
#include <tephra/compute.hpp>
#include <tephra/render.hpp>
#include <tephra/semaphore.hpp>
#include <tephra/query.hpp>
#include <tephra/common.hpp>
#include <variant>

namespace tp {

/// Specifies additional properties of a tp::Job
enum class JobFlag {
    /// Hints that the job will not take a significant amount of time or resources when executed on the device.
    /// This may allow optimizations that aim to reduce the overhead of a job submission.
    Small,
};
TEPHRA_MAKE_ENUM_BIT_MASK(JobFlagMask, JobFlag)

/// An opaque handle used for recording a job's command lists.
class CommandPool;

class JobResourcePoolContainer;
struct JobData;

/// A job represents a single instance of work to be done on the device.
///
/// A job is created in a recording state, in which its methods can be called for recording commands and allocating
/// resources. The job can then be enqueued to a device queue by calling tp::Device::enqueueJob, returning ownership
/// of the handle and transitioning it to the enqueued state. To actually schedule the job for execution,
/// tp::Device::submitQueuedJobs needs to be called, moving it to the submitted state.
///
/// Buffers, images and descriptor sets can be allocated for use within the job and the render and compute lists it
/// executes. This allocation is handled by the parent tp::JobResourcePool and may be more efficient than a global
/// allocation. These resources have a limited lifetime bound to the execution of the job and they may not be accessed
/// outside of it.
///
/// @remarks
///     All methods of tp::Job and tp::Device::enqueueJob also access the parent tp::JobResourcePool that the job was
///     created from. This access must be synchronized - no two threads may operate on jobs that were created from
///     the same pool at the same time.
///
/// @see tp::JobResourcePool::createJob
/// @see tp::Device::enqueueJob
class Job {
public:
    /// Allocates a job-local buffer for use within this job.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     Job-local buffers can only exist in device-local memory and cannot be mapped for host access.
    ///     Use tp::Job::allocatePreinitializedBuffer for data uploads instead.
    /// @remarks
    ///     The buffer may only be used for commands within this job or in command lists that execute within it.
    /// @remarks
    ///     The buffer is not created until the job is enqueued. As a consequence it cannot be used as a tp::Descriptor
    ///     while the job is in the recording state. tp::FutureDescriptor with tp::Job::allocateLocalDescriptorSet
    ///     does not have this limitation.
    BufferView allocateLocalBuffer(const BufferSetup& setup, const char* debugName = nullptr);

    /// Allocates a job-local image for use within this job.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The image may only be used for commands within this job or in command lists that execute within it.
    /// @remarks
    ///     The image is not created until the job is enqueued. As a consequence it cannot be used as a tp::Descriptor
    ///     while the job is in the recording state. tp::FutureDescriptor with tp::Job::allocateLocalDescriptorSet
    ///     does not have this limitation.
    ImageView allocateLocalImage(const ImageSetup& setup, const char* debugName = nullptr);

    /// Allocates a preinitialized buffer for use within this job, with initial contents provided outside of it.
    /// @param setup
    ///     The setup structure describing the object.
    /// @param memoryPreference
    ///     The memory preference progression that will be used for allocating memory for the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The buffer may only be used for commands within this job or in command lists that execute within it,
    ///     unless such a use happens before the job gets submitted.
    /// @remarks
    ///     The intended use of preinitialized buffers is for uploading data to the device through
    ///     tp::HostMappedMemory. Unlike job-local buffers, they are created immediately.
    BufferView allocatePreinitializedBuffer(
        const BufferSetup& setup,
        const MemoryPreference& memoryPreference,
        const char* debugName = nullptr);

    /// Allocates a job-local descriptor set for use within this job.
    /// @param descriptorSetLayout
    ///     The layout to be used for the descriptor set and also the layout of the provided descriptors.
    /// @param descriptors
    ///     The array of descriptors following the given layout. See tp::DescriptorSet for details.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     The descriptor set may only be used for commands within this job or in command lists that execute
    ///     within it.
    /// @remarks
    ///     The descriptor set gets created when the job is enqueued, so its descriptors may reference other job-local
    ///     resources that have not been created yet.
    DescriptorSetView allocateLocalDescriptorSet(
        const DescriptorSetLayout* descriptorSetLayout,
        ArrayParameter<const FutureDescriptor> descriptors,
        const char* debugName = nullptr);

    AccelerationStructureView allocateLocalAccelerationStructureKHR(
        const AccelerationStructureSetup& setup,
        const char* debugName = nullptr);

    /// Creates a command pool for use with tp::ComputeList and tp::RenderList within this job.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     Only one thread may record commands with a single command pool at a time.
    /// @see tp::ComputeList::beginRecording
    /// @see tp::RenderList::beginRecording
    /// @see @vksymbol{VkCommandPool}
    CommandPool* createCommandPool(const char* debugName = nullptr);

    /// Prepares a buffer for future read-only usages.
    ///
    /// Makes the results of all previous accesses of the resource in this queue visible to the specified future read
    /// accesses in all queues of the target queue type. Any future access besides the one specified invalidates the
    /// export on the accessed range.
    ///
    /// While synchronization and memory visibility is handled automatically between commands recorded to jobs in the
    /// same queue, exporting a resource is still useful for the following reasons:
    /// - Render and compute passes have to explicitly declare any accesses of resources that weren't previously
    ///   exported.
    /// - If `targetQueueType` is not tp::QueueType::Undefined, the export operation additionally makes the resource
    ///   accessible to all of the queues of the given type.
    /// - To read back data from the device, the resource has to be exported with `readAccessMask` that contains
    ///   tp::ReadAccess::Host.
    /// - It reveals the future intended usage to the library beyond the scope of this job, allowing for potentially
    ///   more efficient placement of Vulkan pipeline barriers.
    ///
    /// @param buffer
    ///     The buffer to be exported.
    /// @param readAccessMask
    ///     The mask of future read-only accesses.
    /// @param targetQueueType
    ///     If not tp::QueueType::Undefined, the resource is made accessible to all of the queues of the given type.
    void cmdExportResource(
        const BufferView& buffer,
        ReadAccessMask readAccessMask,
        QueueType targetQueueType = QueueType::Undefined);

    /// Prepares an image for future read-only usages.
    /// @param image
    ///     The image to be exported.
    /// @param readAccessMask
    ///     The mask of future read-only accesses.
    /// @param targetQueueType
    ///     If not tp::QueueType::Undefined, the resource is made accessible to all of the queues of the given type.
    /// @remarks
    ///     `readAccessMask` must internally map to exactly one Vulkan image layout. See the following table for the
    ///     sets of read accesses that are compatible with each other and what layout they map to:
    ///     - tp::ReadAccess::(...)ShaderSampled: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
    ///     - tp::ReadAccess::(...)ShaderStorage, tp::ReadAccess::Unknown: `VK_IMAGE_LAYOUT_GENERAL`
    ///     - tp::ReadAccess::Transfer: `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`
    ///     - tp::ReadAccess::DepthStencilAttachment: `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL`
    ///     - tp::ReadAccess::ImagePresentKHR: `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
    void cmdExportResource(
        const ImageView& image,
        ReadAccessMask readAccessMask,
        QueueType targetQueueType = QueueType::Undefined);

    /// Prepares an image for future read-only usages.
    /// @param image
    ///     The image containing the range to be exported.
    /// @param range
    ///     The image subresource range to be exported.
    /// @param readAccessMask
    ///     The mask of future read-only accesses.
    /// @param targetQueueType
    ///     If not tp::QueueType::Undefined, the resource is made accessible to all of the queues of the given type.
    /// @remarks
    ///     `readAccessMask` must map to exactly one Vulkan image layout. See above for table.
    void cmdExportResource(
        const ImageView& image,
        const ImageSubresourceRange& range,
        ReadAccessMask readAccessMask,
        QueueType targetQueueType = QueueType::Undefined);

    /// Prepares an acceleration structure for future read-only usages.
    /// @param accelerationStructure
    ///     The acceleration structure to be exported.
    /// @param readAccessMask
    ///     The mask of future read-only accesses.
    /// @param targetQueueType
    ///     If not tp::QueueType::Undefined, the resource is made accessible to all of the queues of the given type.
    void cmdExportResource(
        const AccelerationStructureView& accelerationStructure,
        ReadAccessMask readAccessMask,
        QueueType targetQueueType = QueueType::Undefined);

    /// Discards the contents of the image, making them undefined for future accesses. Doing this may potentially
    /// improve performance.
    /// @param image
    ///     The image with contents to be discarded.
    void cmdDiscardContents(const ImageView& image);

    /// Discards the contents of the image range, making them undefined for future accesses. Doing this may potentially
    /// improve performance.
    /// @param image
    ///     The image with a range of contents to be discarded.
    /// @param range
    ///     The range of contents to be discarded.
    void cmdDiscardContents(const ImageView& image, ImageSubresourceRange range);

    /// Fills the buffer with a fixed value.
    /// @param dstBuffer
    ///     The buffer to be filled.
    /// @param value
    ///     The 4-byte word written repeatedly to fill the given buffer.
    /// @see @vksymbol{vkCmdFillBuffer}
    void cmdFillBuffer(const BufferView& dstBuffer, uint32_t value);

    /// Updates a range of the buffer's contents with the passed data.
    /// @param dstBuffer
    ///     The buffer with contents to be updated.
    /// @param data
    ///     The data to update. The length of the array defines the size of the range to be updated.
    /// @remarks
    ///     The size of the data array must be less or equal to 65536 bytes. For larger updates, use
    ///     tp::HostMappedMemory. A copy of the data is made upon this call, which is later copied again to a Vulkan
    ///     command buffer. It should therefore only be used for very small amounts of data.
    /// @remarks
    ///     The size of the `data` array must be a multiple of 4 and smaller or equal to the size of `dstBuffer`.
    /// @see @vksymbol{vkCmdUpdateBuffer}
    void cmdUpdateBuffer(const BufferView& dstBuffer, ArrayParameter<const std::byte> data);

    /// Copies regions of one buffer's contents to another.
    /// @param srcBuffer
    ///     The source buffer.
    /// @param dstBuffer
    ///     The destination buffer.
    /// @param copyRegions
    ///     An array specifying the regions to copy.
    /// @remarks
    ///     The result is undefined if the copy regions overlap in memory.
    /// @see @vksymbol{vkCmdCopyBuffer}
    void cmdCopyBuffer(
        const BufferView& srcBuffer,
        const BufferView& dstBuffer,
        ArrayParameter<const BufferCopyRegion> copyRegions);

    /// Copies regions of one image's contents to another.
    /// @param srcImage
    ///     The source image.
    /// @param dstImage
    ///     The destination image.
    /// @param copyRegions
    ///     An array specifying the regions to copy.
    /// @remarks
    ///     The result is undefined if the copy regions overlap in memory.
    /// @remarks
    ///     The formats of both images must share the same tp::FormatCompatibilityClass and must have the same number of
    ///     samples. If the formats are different, the data is reinterpreted without any format conversion.
    /// @see @vksymbol{vkCmdCopyImage}
    void cmdCopyImage(
        const ImageView& srcImage,
        const ImageView& dstImage,
        ArrayParameter<const ImageCopyRegion> copyRegions);

    /// Copies regions of a buffer's contents to an image.
    /// @param srcBuffer
    ///     The source buffer.
    /// @param dstImage
    ///     The destination image.
    /// @param copyRegions
    ///     An array specifying the regions to copy.
    /// @see @vksymbol{vkCmdCopyBufferToImage}
    void cmdCopyBufferToImage(
        const BufferView& srcBuffer,
        const ImageView& dstImage,
        ArrayParameter<const BufferImageCopyRegion> copyRegions);

    /// Copies regions of an image's contents to a buffer.
    /// @param srcImage
    ///     The source image.
    /// @param dstBuffer
    ///     The destination buffer.
    /// @param copyRegions
    ///     An array specifying the regions to copy.
    /// @see @vksymbol{vkCmdCopyImageToBuffer}
    void cmdCopyImageToBuffer(
        const ImageView& srcImage,
        const BufferView& dstBuffer,
        ArrayParameter<const BufferImageCopyRegion> copyRegions);

    /// Copies regions of one image's contents to another, potentially performing format conversion, arbitrary scaling
    /// and filtering.
    /// @param srcImage
    ///     The source image.
    /// @param dstImage
    ///     The destination image.
    /// @param blitRegions
    ///     An array specifying the regions to copy.
    /// @param filter
    ///     The filter to apply when scaling is involved.
    /// @remarks
    ///     The images must not be multisampled. Use tp::Job::cmdResolveImage for that purpose.
    /// @see @vksymbol{vkCmdBlitImage}
    void cmdBlitImage(
        const ImageView& srcImage,
        const ImageView& dstImage,
        ArrayParameter<const ImageBlitRegion> blitRegions,
        Filter filter = Filter::Linear);

    /// Clears contents of the image to a fixed value.
    /// @param dstImage
    ///     The destination image.
    /// @param value
    ///     The value that the image will be cleared to. It must be valid for the format of the image.
    /// @remarks
    ///     Clearing images as part of a render pass may be more efficient.
    /// @see @vksymbol{vkCmdClearColorImage}
    /// @see @vksymbol{vkCmdClearDepthStencilImage}
    void cmdClearImage(const ImageView& dstImage, ClearValue value);

    /// Clears ranges of contents of the image to a fixed value.
    /// @param dstImage
    ///     The destination image.
    /// @param value
    ///     The value that the image will be cleared to. It must be valid for the format of the image.
    /// @param ranges
    ///     An array specifying the ranges of the image to clear.
    /// @remarks
    ///     Clearing images as part of a render pass may be more efficient.
    /// @see @vksymbol{vkCmdClearColorImage}
    /// @see @vksymbol{vkCmdClearDepthStencilImage}
    void cmdClearImage(const ImageView& dstImage, ClearValue value, ArrayParameter<const ImageSubresourceRange> ranges);

    /// Resolves regions of a multisampled color image to a single sample color image.
    /// @param srcImage
    ///     The source multisampled image.
    /// @param dstImage
    ///     The destination image.
    /// @param resolveRegions
    ///     An array specifying the regions to resolve.
    /// @remarks
    ///     Resolving images as part of a render pass may be more efficient.
    /// @remarks
    ///     Only images with color formats are supported. For automatically resolving depth stencil images, consider
    ///     @vksymbol{VK_KHR_depth_stencil_resolve}
    /// @see @vksymbol{vkCmdResolveImage}
    void cmdResolveImage(
        const ImageView& srcImage,
        const ImageView& dstImage,
        ArrayParameter<const ImageCopyRegion> resolveRegions);

    /// Forms a compute pass that executes lists of compute commands.
    /// @param setup
    ///     The setup structure describing the compute pass and its use of resources.
    /// @param commandRecording
    ///     Describes how compute commands are to be recorded for the compute pass. The parameter can be either:
    ///     - A non-empty array view of null tp::ComputeList objects that will be initialized by the function call.
    ///       Commands can be recorded to these lists while the job is in an enqueued state. The lists are executed
    ///       in the order they are in this array and lists with no recorded commands will be skipped.
    ///     - A function callback to record commands to a tp::ComputeList that will be provided as its parameter. This
    ///       function will be called as a part of the next tp::Device::submitQueuedJobs call after the job has been
    ///       enqueued to the same queue.
    /// @param debugName
    ///     The debug name identifier for the compute pass.
    /// @remarks
    ///     Any usage of resources inside the compute pass beyond accesses that the resources were previously exported
    ///     for must be specified inside the `setup` structure.
    /// @remarks
    ///     The dependencies between the compute pass and the rest of the tp::Job are synchronized automatically.
    ///     Dependencies between commands executed within the same compute pass are not, and must be synchronized
    ///     manually with tp::ComputeList::cmdPipelineBarrier.
    /// @remarks
    ///     Inline callbacks will be called during the call to tp::Device::submitQueuedJobs in the provided order.
    ///     Device queue thread safety rules apply: No operations on the target queue are permitted inside the callback.
    void cmdExecuteComputePass(
        const ComputePassSetup& setup,
        std::variant<ArrayView<ComputeList>, ComputeInlineCallback> commandRecording,
        const char* debugName = nullptr);

    /// Forms a render pass that executes lists of render commands.
    /// @param setup
    ///     The setup structure describing the render pass, its attachments and its non-attachment resource usage.
    /// @param commandRecording
    ///     Describes how render commands are to be recorded for the render pass. The parameter can be either:
    ///     - A non-empty array view of null tp::RenderList objects that will be initialized by the function call.
    ///       Commands can be recorded to these lists while the job is in an enqueued state. The lists are executed
    ///       in the order they are in this array and lists with no recorded commands will be skipped.
    ///     - A function callback to record commands to a tp::RenderList that will be provided as its parameter. This
    ///       function will be called as a part of the next tp::Device::submitQueuedJobs call after the job has been
    ///       enqueued to the same queue.
    /// @param debugName
    ///     The debug name identifier for the render pass.
    /// @remarks
    ///     Any usage of non-attachment resources inside the render pass beyond accesses that the resources were
    ///     previously exported for must be specified inside the `setup` structure.
    /// @remarks
    ///     The dependencies between the render pass and the rest of the tp::Job are synchronized automatically.
    ///     Dependencies between commands executed within the same render pass are not, except for guarantees
    ///     provided by the rasterization order.
    /// @remarks
    ///     Inline callbacks will be called during the call to tp::Device::submitQueuedJobs in the provided order.
    ///     Device queue thread safety rules apply: No operations on the target queue are permitted inside the callback.
    /// @see @vksymbol{VkCmdBeginRendering}
    void cmdExecuteRenderPass(
        const RenderPassSetup& setup,
        std::variant<ArrayView<RenderList>, RenderInlineCallback> commandRecording,
        const char* debugName = nullptr);

    /// Begins a debug label, marking the following commands until the next tp::Job::cmdEndDebugLabel with the
    /// given name and optional color for display in validation and debugging tools.
    /// @param name
    ///     The name of the label.
    /// @param color
    ///     The color of the label. Only used by external tools.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdBeginDebugLabel(const char* name, ArrayParameter<const float> color = {});

    /// Inserts a debug label, marking the following commands with the given name and optional color for display in
    /// validation and debugging tools.
    /// @param name
    ///     The name of the label.
    /// @param color
    ///     The color of the label. Only used by external tools.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdInsertDebugLabel(const char* name, ArrayParameter<const float> color = {});

    /// Ends the last debug label. Must be preceded by tp::Job::cmdBeginDebugLabel.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdEndDebugLabel();

    /// Queries the time on the device as part of the given pipeline stage and writes the result to the provided query
    /// object.
    /// @param query
    ///     The timestamp query object that the result will be written to.
    /// @param stage
    ///     The pipeline stage at which the timestamp should be measured. This means a time point at which all the
    ///     previously submitted commands have finished executing the given pipeline stage.
    void cmdWriteTimestamp(const TimestampQuery& query, PipelineStage stage);

    void cmdBuildAccelerationStructuresKHR(ArrayParameter<const AccelerationStructureBuildInfo> buildInfos);

    void cmdCopyAccelerationStructureKHR(
        const AccelerationStructureView& srcView,
        const AccelerationStructureView& dstView);

    /// Updates the internal synchronization state for the buffer view to the given external access.
    ///
    /// This should be used after a resource has been accessed by an operation done outside of Tephra and before it
    /// is accessed by a Tephra command again. This command allows those future Tephra accesses to be synchronized
    /// against the external access properly.
    ///
    /// @param buffer
    ///     The buffer to be imported.
    /// @param vkStageMask
    ///     The Vulkan pipeline stage mask of the external access.
    /// @param vkAccessMask
    ///     The Vulkan access mask of the external access.
    /// @remarks
    ///     If the last external access to the imported resource is already being synchronized with a
    ///     tp::ExternalSemaphore, then `vkStageMask` may be `VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT` and `vkAccessMask` may
    ///     be `VK_ACCESS_NONE`, signifying that no additional synchronization is required.
    /// @remarks
    ///     The Vulkan queue family that this job will execute on must have acquired ownership of the resource.
    void vkCmdImportExternalResource(
        const BufferView& buffer,
        VkPipelineStageFlags vkStageMask,
        VkAccessFlags vkAccessMask);

    /// Updates the internal synchronization state for the image view to the given external access.
    /// @param image
    ///     The image to be imported.
    /// @param vkImageLayout
    ///     The Vulkan image layout that the image subresource is transitioned to by the external access.
    /// @param vkStageMask
    ///     The Vulkan pipeline stage mask of the external access.
    /// @param vkAccessMask
    ///     The Vulkan access mask of the external access.
    void vkCmdImportExternalResource(
        const ImageView& image,
        VkImageLayout vkImageLayout,
        VkPipelineStageFlags vkStageMask,
        VkAccessFlags vkAccessMask);

    /// Updates the internal synchronization state for the subresource range to the given external access.
    /// @param image
    ///     The image to be imported.
    /// @param range
    ///     The affected subresource range.
    /// @param vkImageLayout
    ///     The Vulkan image layout that the image subresource is transitioned to by the external access.
    /// @param vkStageMask
    ///     The Vulkan pipeline stage mask of the external access.
    /// @param vkAccessMask
    ///     The Vulkan access mask of the external access.
    void vkCmdImportExternalResource(
        const ImageView& image,
        const ImageSubresourceRange& range,
        VkImageLayout vkImageLayout,
        VkPipelineStageFlags vkStageMask,
        VkAccessFlags vkAccessMask);

    TEPHRA_MAKE_NONCOPYABLE(Job);
    TEPHRA_MAKE_MOVABLE(Job);

    virtual ~Job();

protected:
    friend class JobResourcePoolContainer;

    DebugTargetPtr debugTarget;
    JobData* jobData;

    Job(JobData* jobData, DebugTarget debugTarget);

    void finalize();
};

/// Specifies additional properties of a tp::JobResourcePool.
enum class JobResourcePoolFlag {
    /// Normally, only images with the exact match of formats are able to be aliased. By specifying this tag, images
    /// of different formats that are from the same format compatibility class may be aliased together.
    /// @remarks
    ///     This can lead to a reduced memory usage, but may also reduce performance on some platforms.
    AliasCompatibleFormats,
    /// Disables suballocation and aliasing of all resources. This means that every requested job-local resource will
    /// correspond to a single Vulkan resource. tp::OverallocationBehavior for buffers will be ignored.
    /// @remarks
    ///     This can be useful for debugging, since it allows passing debug names to those Vulkan resources as long as
    ///     #TEPHRA_ENABLE_DEBUG_NAMES and tp::ApplicationExtension::EXT_DebugUtils are enabled.
    DisableSuballocation
};
TEPHRA_MAKE_ENUM_BIT_MASK(JobResourcePoolFlagMask, JobResourcePoolFlag)

/// Used as configuration for creating a new tp::JobResourcePool object.
/// @see tp::Device::createJobResourcePool
struct JobResourcePoolSetup {
    DeviceQueue queue;
    JobResourcePoolFlagMask flags;
    OverallocationBehavior bufferOverallocationBehavior;
    OverallocationBehavior preinitBufferOverallocationBehavior;
    OverallocationBehavior descriptorOverallocationBehavior;

    /// @param queue
    ///     The device queue that the pool will be associated to. Jobs allocated from this pool can then only be
    ///     enqueued to this queue.
    /// @param flags
    ///     Additional flags for creation of the pool.
    /// @param bufferOverallocationBehavior
    ///     The overallocation behavior of job-local buffers.
    /// @param preinitBufferOverallocationBehavior
    ///     The overallocation behavior of preinitialized buffers.
    /// @param descriptorOverallocationBehavior
    ///     The overallocation behavior of job-local descriptor sets.
    JobResourcePoolSetup(
        DeviceQueue queue,
        JobResourcePoolFlagMask flags = {},
        OverallocationBehavior bufferOverallocationBehavior = { 1.25f, 1.5f, 65536 },
        OverallocationBehavior preinitBufferOverallocationBehavior = { 3.0f, 1.5f, 65536 },
        OverallocationBehavior descriptorOverallocationBehavior = { 3.0f, 1.5f, 128 });
};

/// Contains statistics about the current allocations of a tp::JobResourcePool.
/// @see tp::JobResourcePool::getStatistics
struct JobResourcePoolStatistics {
    /// The number of backing allocations made for job-local buffers.
    uint32_t bufferAllocationCount;
    /// The size of all backing allocations made for job-local buffers.
    uint64_t bufferAllocationBytes;
    /// The number of backing allocations made for job-local images.
    uint32_t imageAllocationCount;
    /// The size of all backing allocations made for job-local images.
    uint64_t imageAllocationBytes;
    /// The number of backing allocations made for preinitialized buffers.
    uint32_t preinitBufferAllocationCount;
    /// The size of all backing allocations made for preinitialized buffers.
    uint64_t preinitBufferAllocationBytes;

    /// The total size of all backing allocations made for all job resources.
    uint64_t getTotalAllocationBytes() const {
        return bufferAllocationBytes + imageAllocationBytes + preinitBufferAllocationBytes;
    }
};

/// Manages the job-local resources used by tp::Job objects created from it. Enables efficient allocation and reuse
/// of these resources between consecutive jobs. Jobs created from a tp::JobResourcePool can only be enqueued to the
/// same device queue that the pool was created for, allowing the allocator to better reuse resources. Similar jobs
/// that are submitted periodically therefore benefit from being allocated from the same tp::JobResourcePool.
/// @see tp::Device::createJobResourcePool
class JobResourcePool : public Ownable {
public:
    /// Creates a new tp::Job object that can be later enqueued to the pool's associated device queue.
    /// @param flags
    ///     Additional flags for creation of the job.
    /// @param debugName
    ///     The debug name identifier for the job.
    /// @see tp::Device::enqueueJob
    Job createJob(JobFlagMask flags = {}, const char* debugName = nullptr);

    /// Attempts to free unused resources from the pool. Returns the number of bytes freed, if any.
    /// @param latestTrimmed
    ///     If given, serves as a hint to only free the resources that have been last used during the job associated to
    ///     the given semaphore. The semaphore must be from the same queue as the one associated with this pool.
    ///     If the semaphore is null, all currently unused resources may be freed.
    uint64_t trim(const JobSemaphore& latestTrimmed = {});

    /// Returns the current statistics of this resource pool.
    JobResourcePoolStatistics getStatistics() const;

    TEPHRA_MAKE_INTERFACE(JobResourcePool);

protected:
    JobResourcePool() {}
};

// for some reason, doxygen ignores incomplete types
#ifdef DOXYGEN
class CommandPool {};
#endif

}
